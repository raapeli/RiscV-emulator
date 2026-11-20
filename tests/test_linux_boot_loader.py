import os
from pathlib import Path
import select
import struct
import subprocess
import sys
import tempfile
import time


def expect_failure(command, message):
    result = subprocess.run(command, capture_output=True, timeout=5)
    if result.returncode == 0 or message not in result.stderr:
        raise RuntimeError(f"Expected {message!r}, got {result}")


def read_guest_output(guest, size, check):
    deadline = time.monotonic() + 5
    result = bytearray()
    while len(result) < size:
        remaining = max(0, deadline - time.monotonic())
        if not select.select([guest.stdout], [], [], remaining)[0]:
            raise RuntimeError(f"Timed out waiting for {check}")
        chunk = os.read(guest.stdout.fileno(), size - len(result))
        if not chunk:
            raise RuntimeError(f"Guest exited while waiting for {check}")
        result.extend(chunk)
    return bytes(result)


def expect_guest_output(guest, expected, check):
    result = read_guest_output(guest, len(expected), check)
    if result != expected:
        raise RuntimeError(f"{check} failed: {result!r}")


def read_guest_dtb(command):
    with subprocess.Popen(command, stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE) as guest:
        try:
            expect_guest_output(guest, b"P", "firmware handoff and UART setup")
            guest.stdin.write(b"\0")
            guest.stdin.flush()
            expect_guest_output(guest, b"Q", "polled UART input")
            guest.stdin.write(b"X")
            guest.stdin.flush()
            expect_guest_output(guest, b"R", "UART interrupt delivery")
            header = read_guest_output(guest, 40, "DTB header")
            magic, size = struct.unpack_from(">II", header)
            if magic != 0xD00DFEED or not 40 <= size <= 0x200000:
                raise RuntimeError(f"Invalid DTB header: {header.hex()}")
            return header + read_guest_output(guest, size - len(header), "DTB body")
        finally:
            if guest.poll() is None:
                guest.terminate()


def dtb_properties(blob):
    _, _, offset, strings_offset, _, _, _, _, strings_size, struct_size = (
        struct.unpack_from(">10I", blob)
    )
    strings = blob[strings_offset:strings_offset + strings_size]
    structure_end = offset + struct_size
    nodes = []
    properties = {}
    while offset < structure_end:
        token, = struct.unpack_from(">I", blob, offset)
        offset += 4
        if token == 1:  # FDT_BEGIN_NODE
            end = blob.index(0, offset, structure_end)
            nodes.append(blob[offset:end].decode())
            offset = (end + 4) & ~3
        elif token == 2:  # FDT_END_NODE
            nodes.pop()
        elif token == 3:  # FDT_PROP
            length, name_offset = struct.unpack_from(">II", blob, offset)
            offset += 8
            name = strings[name_offset:strings.index(0, name_offset)].decode()
            properties["/".join(nodes), name] = blob[offset:offset + length]
            offset += (length + 3) & ~3
        elif token == 9 and not nodes and offset == structure_end:  # FDT_END
            return properties
        elif token != 4:  # FDT_NOP
            raise RuntimeError(f"Invalid DTB structure token: {token}")
    raise RuntimeError("Missing DTB end token")


def check_generated_dtb(blob, initrd_size):
    props = dtb_properties(blob)
    cpu = "/cpus/cpu@0"
    cpu_intc = cpu + "/interrupt-controller"
    plic = "/soc/interrupt-controller@c000000"
    uart = "/soc/serial@10000000"
    expected = {
        ("/chosen", "stdout-path"): uart.encode() + b":115200n8\0",
        ("/chosen", "bootargs"): b"console=ttyS0,115200 earlycon\0",
        ("/chosen", "linux,initrd-start"): struct.pack(">Q", 0x84000000),
        ("/chosen", "linux,initrd-end"): struct.pack(">Q", 0x84000000 + initrd_size),
        ("/aliases", "serial0"): uart.encode() + b"\0",
        ("/memory@80000000", "reg"): struct.pack(">QQ", 0x80000000, 0x08000000),
        ("/cpus", "timebase-frequency"): struct.pack(">I", 10000000),
        (cpu, "reg"): struct.pack(">I", 0),
        (cpu, "mmu-type"): b"riscv,sv39\0",
        (cpu, "riscv,isa"): b"rv64imafdc_zicsr_zifencei\0",
        ("/soc/clint@2000000", "compatible"): b"sifive,clint0\0",
        ("/soc/clint@2000000", "reg"): struct.pack(">QQ", 0x02000000, 0x10000),
        ("/soc/clint@2000000", "interrupts-extended"):
            props[cpu_intc, "phandle"] + struct.pack(">I", 3) +
            props[cpu_intc, "phandle"] + struct.pack(">I", 7),
        (plic, "reg"): struct.pack(">QQ", 0x0C000000, 0x04000000),
        (plic, "compatible"): b"sifive,plic-1.0.0\0riscv,plic0\0",
        (plic, "riscv,ndev"): struct.pack(">I", 31),
        (plic, "interrupts-extended"):
            props[cpu_intc, "phandle"] + struct.pack(">I", 11) +
            props[cpu_intc, "phandle"] + struct.pack(">I", 9),
        (uart, "compatible"): b"ns16550a\0",
        (uart, "clock-frequency"): struct.pack(">I", 3686400),
        (uart, "reg"): struct.pack(">QQ", 0x10000000, 8),
        (uart, "interrupt-parent"): props[plic, "phandle"],
        (uart, "interrupts"): struct.pack(">I", 10),
    }
    for prop, value in expected.items():
        if props.get(prop) != value:
            raise RuntimeError(f"Expected {prop} to be {value!r}, got {props.get(prop)!r}")


def main():
    emulator, firmware = sys.argv[1:]
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        kernel = path / "Image"
        initrd = path / "initramfs.cpio"
        dtb = path / "guest.dtb"
        kernel.write_bytes(b"K")
        initrd_contents = b"Initramfs"
        initrd.write_bytes(initrd_contents)
        command = [emulator, "--linux", "--initrd", initrd, "--kernel", kernel,
                   "--firmware", firmware]

        generated = read_guest_dtb(command)
        check_generated_dtb(generated, len(initrd_contents))
        custom = generated.replace(b"console=ttyS0", b"console=ttyS1")
        dtb.write_bytes(custom)
        if read_guest_dtb(command + ["--dtb", dtb]) != custom:
            raise RuntimeError("Custom DTB was modified")

        expect_failure(command[:-2], b"Missing Linux boot argument: --firmware")
        expect_failure(command + ["--kernel", kernel], b"Invalid Linux boot argument")

        too_large = path / "too-large.bin"
        with too_large.open("wb") as image:
            image.truncate(0x200001)
        expect_failure(command[:-1] + [too_large],
                       b"--firmware image exceeds its guest RAM region")
        expect_failure(command + ["--dtb", too_large],
                       b"--dtb image exceeds its guest RAM region")

        initrd.write_bytes(b"")
        expect_failure(command, b"--initrd image is empty")


if __name__ == "__main__":
    main()
