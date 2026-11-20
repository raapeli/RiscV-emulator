import os
import select
import subprocess
import sys
import time


def main():
    expected = (b"kernel: entering U-mode\n"
                b"kernel: blocked user store to kernel RAM\n"
                b"kernel: blocked user store to console\n"
                b"kernel: blocked user store across stack boundary\n"
                b"userspace: hello\n"
                b"kernel: user exited with status 7\n")
    deadline = time.monotonic() + 5
    with subprocess.Popen(sys.argv[1:], stdin=subprocess.DEVNULL,
                          stdout=subprocess.PIPE) as guest:
        output = b""
        try:
            while len(output) < len(expected):
                remaining = max(0, deadline - time.monotonic())
                if not select.select([guest.stdout], [], [], remaining)[0]:
                    break
                chunk = os.read(guest.stdout.fileno(), len(expected) - len(output))
                if not chunk:
                    break
                output += chunk
                if not expected.startswith(output):
                    break
        finally:
            if guest.poll() is None:
                guest.terminate()

    if output != expected:
        sys.exit(f"Unexpected kernel output: {output!r}")


if __name__ == "__main__":
    main()
