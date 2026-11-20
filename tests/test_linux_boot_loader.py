import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile


def expect_failure(command, message):
    result = subprocess.run(command, capture_output=True, timeout=5)
    if result.returncode == 0 or message not in result.stderr:
        raise RuntimeError(f"Expected {message!r}, got {result}")


def main():
    emulator, firmware = sys.argv[1:]
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        kernel = path / "Image"
        initrd = path / "initramfs.cpio"
        dtb = path / "guest.dtb"
        kernel.write_bytes(b"K")
        initrd.write_bytes(b"I")
        dtb.write_bytes(b"\xd0\x0d\xfe\xed")
        command = [emulator, "--linux", "--initrd", initrd, "--kernel", kernel,
                   "--firmware", firmware, "--dtb", dtb]

        with subprocess.Popen(command, stdin=subprocess.DEVNULL,
                              stdout=subprocess.PIPE) as guest:
            try:
                if not select.select([guest.stdout], [], [], 5)[0]:
                    raise RuntimeError("Timed out waiting for the firmware probe")
                result = os.read(guest.stdout.fileno(), 1)
                if result != b"P":
                    raise RuntimeError(f"Firmware handoff check failed: {result!r}")
            finally:
                if guest.poll() is None:
                    guest.terminate()

        expect_failure(command[:-2], b"Missing Linux boot argument: --dtb")
        expect_failure(command + ["--kernel", kernel], b"Invalid Linux boot argument")

        too_large = path / "too-large.bin"
        with too_large.open("wb") as image:
            image.truncate(0x200001)
        expect_failure(command[:-4] + ["--firmware", too_large, "--dtb", dtb],
                       b"--firmware image exceeds its guest RAM region")
        expect_failure(command[:-1] + [too_large],
                       b"--dtb image exceeds its guest RAM region")

        initrd.write_bytes(b"")
        expect_failure(command, b"--initrd image is empty")


if __name__ == "__main__":
    main()
