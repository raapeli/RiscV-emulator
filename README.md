# Risc-V emulator

A Risc-V emulator built in modern C++23. 

**This is still a WIP with more features to come**

## Features

- [X] RV32I/RV64I
- [X] RV64 compressed integer instructions
- [X] RV64 single- and double-precision floating point
- [X] Exceptions 
- [X] Privileged ISA
- [X] CSRs
- [X] Sv39 virtual memory
- [ ] Peripheral devices

## Build

### Requirements

CMake, Clang, MPFR, and libfdt development headers and libraries
(`mpfr` and `dtc` on Arch).

```
$ git clone --recurse-submodules https://github.com/raapeli/RiscV-emulator.git
$ cmake -S . -B build -DCMAKE_CXX_COMPILER=clang++
$ cd build && make
```

## Testing

On Arch, install packages:

``` 
riscv64-elf-binutils
riscv64-elf-gcc
``` 
For other distros, install similar packages

For building the tests run in addition to normal build instructions
``` 
cmake -P MakeTests.cmake
cd build && ctest --output-on-failure .
``` 

Keep the generated `dumped` directory alongside each suite's `bin` directory.
The runner uses those symbol addresses to load tests at their link address and
read results reported by guest trap handlers through `tohost`.

### Linux and BusyBox integration test

Build the test images from the [Linux boot recipe](examples/linux-boot/build.sh).
It uses OpenSBI 1.6, Linux 6.12, and BusyBox 1.37.0, and checks the SHA-256
digest of each source archive. The kernel and BusyBox config fragments and the
initramfs inputs are in [examples/linux-boot](examples/linux-boot).

The build needs a host C compiler, GNU make, curl, GNU coreutils and tar,
xz, bzip2, Python 3, dtc, bc, flex, bison, and the OpenSSL and libelf development
headers. Use an RV64 Linux cross-toolchain with static target libc. The tested
Arch toolchain is GCC 16.2.1, binutils 2.47, and glibc 2.44, from
`riscv64-linux-gnu-gcc`, `riscv64-linux-gnu-binutils`, and
`riscv64-linux-gnu-glibc`. Allow about 3 GiB for sources and build output.

```sh
bash examples/linux-boot/build.sh
```

The script writes `fw_jump.bin`, `Image`, and `initramfs.cpio` to
`build/linux-boot` by default. Pass an output directory as the first argument
for a separate build. Set `CROSS_COMPILE` to override the default
`riscv64-linux-gnu-` prefix or `JOBS` to limit build parallelism. The script
caches archives under the output directory's `downloads` subdirectory, or in
`LINUX_BOOT_SOURCE_CACHE` when set.

Set `LINUX_BOOT_TEST_IMAGES` to the output directory to opt in to the boot
test. Custom images can use the same test if their `/init` prints
`BusyBox shell ready` and starts an interactive shell on the serial console.
Use a Release build for the 180-second boot and command timeout.

```sh
cmake -S . -B build/linux-integration -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Release -DLINUX_BOOT_TEST_IMAGES="$PWD/build/linux-boot"
cmake --build build/linux-integration --target RiscV
ctest --test-dir build/linux-integration -R '^linux_busybox_boot_test$' --output-on-failure
```

The test sends a shell command over the UART and checks its output. The serial
log is saved to `build/linux-integration/tests/linux_busybox_boot.log`.

## Usage

Run the program RiscV with an argument to a RiscV binary with no headers.

### Linux boot image loader

The Linux boot mode loads the boot images into guest RAM and starts the firmware
in machine mode with hart ID zero and the DTB address in `a0` and `a1`.
RAM defaults to 128 MiB. Use `--ram-size 256` to allocate 256 MiB and report
that size in the generated device tree.

```sh
./build/RiscV --linux --firmware fw_jump.bin --kernel Image \
  --initrd initramfs.cpio
```

The firmware, kernel, initramfs, and DTB start at `0x80000000`, `0x80200000`,
`0x84000000`, and `0x87e00000` respectively. The generated DTB describes the
single RV64 hart, Sv39, RAM, and MMIO devices. It also sets the serial console
and the initramfs bounds using the image size. The fixed DTB address requires
at least 127 MiB of RAM. If you pass `--dtb guest.dtb`, make sure its memory
node matches the configured RAM range starting at `0x80000000`.

Linux mode exposes a single-hart CLINT at `0x02000000`, a PLIC at
`0x0c000000`, and a byte-wide 16550 UART at `0x10000000` wired to PLIC source
10. The timer advances once per emulator tick with a nominal timebase of 10 MHz.
The UART connects to standard input and output and uses the host terminal's
line mode.

Other machine configurations can attach these devices independently through
`Bus::DeviceMap`. The UART models the divisor latch, receive FIFO, loopback,
and receive and transmit interrupts. Transmission is immediate and the receive
FIFO timeout is measured in emulator ticks.

### Persistent Linux disk

Pass `--disk` to attach a writable raw image. The generated device tree maps
the virtio block device at `0x10001000` on PLIC source 1. Linux discovers it as
`/dev/vda` with the driver settings in the Linux boot recipe.

For a new ext4 image, run from the repository root:

```sh
truncate -s 64M disk.img
mkfs.ext4 disk.img
./build/RiscV --linux --firmware build/linux-boot/fw_jump.bin \
  --kernel build/linux-boot/Image --initrd build/linux-boot/initramfs.cpio \
  --disk disk.img
```

In the BusyBox shell, mount the disk and unmount it before stopping the emulator:

```sh
/bin/busybox mount -t devtmpfs devtmpfs /dev
/bin/busybox mkdir /mnt
/bin/busybox mount -t ext4 /dev/vda /mnt
echo hello > /mnt/hello.txt
/bin/busybox umount /mnt
```

### Polled console

In flat-binary mode, the emulator connects guest byte loads and stores to
standard input and output.

| Address | Guest operation | Behavior |
| --- | --- | --- |
| `0x10000000` | Load byte | Consume one input byte, or return zero when no byte is ready. |
| `0x10000000` | Store byte | Write and flush one output byte. |
| `0x10000001` | Load byte | Return `1` if an input byte is ready, otherwise `0`. |

Poll the status register before reading data to distinguish a zero byte from an
empty input buffer. With a terminal in its usual line mode, input becomes
available when you press Enter. Input can also be piped into the emulator.

### Bare-metal shell example

From the repository root, build the emulator and the RV64IMA shell with the
same cross-toolchain used to build the tests.

```sh
cmake --build build
riscv64-elf-gcc -march=rv64ima -mabi=lp64 -Os -ffreestanding -nostdlib \
  -msmall-data-limit=0 -mno-relax -Texamples/console-shell/link.ld \
  examples/console-shell/start.S examples/console-shell/shell.c \
  -o build/console-shell.elf
riscv64-elf-objcopy -O binary build/console-shell.elf build/console-shell.bin
./build/RiscV build/console-shell.bin
```

At the `>` prompt, enter `help` or `echo hello`. Press Ctrl+C to stop the emulator.

### Kernel and userspace example

With `riscv64-elf-gcc` and `riscv64-elf-objcopy` available, the regular build
creates `build/examples/kernel-userspace/kernel-userspace.bin`. Run it through
the emulator from the repository root.

```sh
cmake --build build
./build/RiscV build/examples/kernel-userspace/kernel-userspace.bin
```

The embedded U-mode program prints `userspace: hello` through `write` and
then calls `exit`. The kernel prints `kernel: user exited with status 7` and
idles. Press Ctrl+C to stop the emulator. With Python 3 installed, CTest checks
the syscall round trip automatically.

The example runs in flat memory with physical memory protection (PMP). User
code can execute and read its text and can read and write its own stack. The
kernel demonstrates that direct user stores to kernel RAM, the console, and
across the user stack boundary fault. Console output succeeds through `write`.
