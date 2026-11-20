import os
from pathlib import Path
import select
import subprocess
import sys
import time


BOOT_TIMEOUT = 180
SHELL_READY = b"BusyBox shell ready"
SHELL_COMMAND = b"printf 'RISCV_LINUX_BOOT_%s\\n' OK\n"
COMMAND_OUTPUT = b"RISCV_LINUX_BOOT_OK"


def check_shell(guest, log):
    deadline = time.monotonic() + BOOT_TIMEOUT
    output = b""
    command_sent = False

    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0 or not select.select([guest.stdout], [], [], remaining)[0]:
            step = "the shell command output" if command_sent else "the BusyBox shell"
            raise RuntimeError(f"Timed out waiting for {step}")

        chunk = os.read(guest.stdout.fileno(), 4096)
        if not chunk:
            raise RuntimeError("Emulator closed the console before the test completed")
        log.write(chunk)

        output = (output + chunk)[-8192:]
        if not command_sent and SHELL_READY in output:
            guest.stdin.write(SHELL_COMMAND)
            guest.stdin.flush()
            command_sent = True

        # The terminal can echo SHELL_COMMAND. Require its expanded output line.
        if command_sent and b"\n" + COMMAND_OUTPUT + b"\n" in output.replace(b"\r\n", b"\n"):
            return


def main():
    emulator, image_dir, log_path = sys.argv[1:]
    images = Path(image_dir)
    command = [emulator, "--linux", "--firmware", images / "fw_jump.bin",
               "--kernel", images / "Image", "--initrd", images / "initramfs.cpio"]

    try:
        with open(log_path, "wb", buffering=0) as log:
            with subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT) as guest:
                try:
                    check_shell(guest, log)
                finally:
                    if guest.poll() is None:
                        guest.terminate()
                        try:
                            guest.wait(timeout=2)
                        except subprocess.TimeoutExpired:
                            guest.kill()
                            guest.wait()
    except (OSError, RuntimeError) as error:
        raise SystemExit(f"{error}\nSerial log: {log_path}") from None


if __name__ == "__main__":
    main()
