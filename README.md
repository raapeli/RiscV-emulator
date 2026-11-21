# RISC-V Emulator

A C++23 RISC-V emulator that runs bare-metal programs and boots Linux to a
BusyBox shell. The RV64 implementation includes compressed instructions,
atomics, single- and double-precision floating point, privileged modes, PMP,
and Sv39 virtual memory.

## Build and test

You need CMake 4.0 or newer, Make or Ninja, a recent C++ compiler and standard
library with C++23 support, and the MPFR and libfdt development libraries.

On Arch Linux, install the host build dependencies and the tools for the guest
tests with these commands.

```sh
sudo pacman -S --needed git cmake make clang mpfr dtc
sudo pacman -S --needed python riscv64-elf-gcc riscv64-elf-binutils
```

From a fresh checkout, build the emulator and run the available tests.

```sh
git clone --recurse-submodules https://github.com/raapeli/RiscV-emulator.git
cd RiscV-emulator
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Use the same three commands with `debug` or `relwithdebinfo` to switch builds.

| Preset | Build directory | Use |
| --- | --- | --- |
| `debug` | `build/debug` | Step through emulator code and get instruction traces on ISA test failures |
| `release` | `build/release` | Run an optimized emulator or boot Linux |
| `relwithdebinfo` | `build/relwithdebinfo` | Profile an optimized build with debug symbols and frame pointers |

For an existing checkout, run `git submodule update --init --recursive` to
fetch the ISA tests and their test environment. Rerun the CMake configure
command after installing tools or initializing the submodules.

| Tests | Additional requirements |
| --- | --- |
| CPU, PMP, Sv39, floating point, and devices | Host build dependencies |
| Upstream RV64 ISA suites | Recursive submodules and bare-metal GCC, objcopy, and objdump |
| Linux boot loader and userspace syscall example | Bare-metal GCC and objcopy, plus Python 3 |
| Linux and BusyBox shell | Python 3 and [Linux boot images](#build-linux-boot-images) |

CMake reports when it skips the ISA, boot loader, or syscall tests. The
regular build generates the ISA binaries and their symbol dumps under
the selected build's `tests/isa` directory. If your bare-metal tools are named
`riscv64-unknown-elf-gcc` and so on, configure with
`-DRISCV_TOOLCHAIN_PREFIX=riscv64-unknown-elf-`.

Use `ctest --preset release -N` to list the configured tests. To run only
the Sv39 test, use `ctest --preset release -R '^sv39_test$'`.
Failed ISA cases save logs under the suite's build directory, for example
`build/debug/tests/isa/rv64ui/logs`.
Use `-DBUILD_TESTING=OFF` when configuring a build without tests.

## Run bare-metal examples

Pass a flat binary to `./build/release/RiscV`. The emulator loads it at `0x8000`.

### Console shell

Build the RV64IMA shell with the bare-metal cross-toolchain from the test
setup.

```sh
riscv64-elf-gcc -march=rv64ima -mabi=lp64 -Os -ffreestanding -nostdlib \
  -msmall-data-limit=0 -mno-relax -Texamples/console-shell/link.ld \
  examples/console-shell/start.S examples/console-shell/shell.c \
  -o build/console-shell.elf
riscv64-elf-objcopy -O binary build/console-shell.elf build/console-shell.bin
./build/release/RiscV build/console-shell.bin
```

At the `>` prompt, enter `help` or `echo hello`. Press Ctrl+C to stop the
emulator.

### Kernel and userspace

When bare-metal GCC and objcopy are available, the regular build creates this
example.

```sh
./build/release/RiscV build/release/examples/kernel-userspace/kernel-userspace.bin
```

A U-mode program calls `write` to print `userspace: hello` and then calls
`exit`. The kernel reports exit status 7 and idles. The example uses PMP to
restrict access to kernel RAM and the console, and to catch writes across the
user stack boundary. Press Ctrl+C to stop it.

### Flat-binary console registers

| Address | Guest operation | Behavior |
| --- | --- | --- |
| `0x10000000` | Load byte | Consume one input byte, or return zero if none is ready |
| `0x10000000` | Store byte | Write and flush one output byte |
| `0x10000001` | Load byte | Return 1 when an input byte is ready, otherwise 0 |

Poll the status register before reading data to distinguish a zero byte from
an empty buffer. With a terminal in line mode, input becomes available when
you press Enter. You can also pipe input into the emulator.

## Boot Linux

### Build Linux boot images

The [boot recipe](examples/linux-boot/build.sh) verifies the source archives
and builds OpenSBI 1.6, Linux 6.12, and BusyBox 1.37.0. Allow about 3 GiB for
sources and build output.

Install a Linux-targeting RV64 cross-toolchain with static target libc. The
default prefix is `riscv64-linux-gnu-`. On Arch, the recipe was tested with
`riscv64-linux-gnu-gcc`, `riscv64-linux-gnu-binutils`, and
`riscv64-linux-gnu-glibc`. The build also uses a host C compiler, GNU make,
curl, GNU coreutils and tar, xz, bzip2, Python 3, dtc, bc, flex, bison,
and the OpenSSL and libelf development headers.

```sh
bash examples/linux-boot/build.sh
```

The recipe writes `fw_jump.bin`, `Image`, and `initramfs.cpio` to
`build/linux-boot`. Pass a directory as the first argument to change the
output location. Set `CROSS_COMPILE` to change the compiler prefix,
`JOBS` to limit parallelism, or `LINUX_BOOT_SOURCE_CACHE` to reuse
downloads across build directories.

### Start the shell

```sh
./build/release/RiscV --linux --firmware build/linux-boot/fw_jump.bin \
  --kernel build/linux-boot/Image --initrd build/linux-boot/initramfs.cpio
```

Wait for `BusyBox shell ready` and enter a command. The UART connects to the
host's standard input and output. Press Ctrl+C to stop the emulator.

Guest RAM starts at `0x80000000` and defaults to 128 MiB. Use
`--ram-size 256` for 256 MiB. The minimum is 127 MiB because the firmware
expects the DTB at a fixed address. The generated DTB describes the RAM,
single RV64 hart, Sv39, MMIO devices, serial console, and initramfs bounds.
If you pass `--dtb guest.dtb`, match its memory node to the configured RAM.

| Boot images and devices | Guest physical address |
| --- | --- |
| OpenSBI firmware | `0x80000000` |
| Linux kernel | `0x80200000` |
| Initramfs | `0x84000000` |
| Device tree | `0x87e00000` |
| CLINT | `0x02000000` |
| PLIC | `0x0c000000` |
| 16550 UART | `0x10000000`, PLIC source 10 |
| Optional virtio block device | `0x10001000`, PLIC source 1 |

The boot loader starts the firmware in machine mode with the hart ID in
`a0` and the DTB address in `a1`. The CLINT's nominal timebase is 10 MHz,
with one timer increment per emulator tick. Other machine configurations
can attach devices through `Bus::DeviceMap`.

### Test the Linux and BusyBox boot

Point CMake at the image directory to enable the integration test. Use a
Release build for the 180-second boot and command timeout.

```sh
cmake --preset release -DLINUX_BOOT_TEST_IMAGES="$PWD/build/linux-boot"
cmake --build --preset release
ctest --preset release -R '^linux_busybox_boot_test$'
```

The test sends a shell command over the UART and checks its output. It saves
the console log in `build/release/tests/linux_busybox_boot.log`. For custom
images, have `/init` print `BusyBox shell ready` and start an interactive serial
shell.

### Attach a persistent disk

Create an ext4 image if you do not already have one.

```sh
truncate -s 64M disk.img
mkfs.ext4 disk.img
```

Add `--disk disk.img` to the Linux boot command. The emulator opens the raw
image for writes, and the recipe's Linux kernel exposes it as `/dev/vda`.

In BusyBox, mount the disk, write a file, and unmount it before stopping the
emulator.

```sh
/bin/busybox mount -t devtmpfs devtmpfs /dev
/bin/busybox mkdir -p /mnt
/bin/busybox mount -t ext4 /dev/vda /mnt
echo hello > /mnt/hello.txt
/bin/busybox umount /mnt
```
