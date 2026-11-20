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

CMake, Clang, and the MPFR development headers and library (`mpfr` on Arch).

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

## Usage

Run the program RiscV with an argument to a RiscV binary with no headers.

### Linux boot image loader

The Linux boot mode loads four files into 128 MiB of guest RAM and starts the
firmware in machine mode with hart ID zero and the DTB address in `a0` and `a1`.

```sh
./build/RiscV --linux --firmware fw_jump.bin --kernel Image \
  --dtb guest.dtb --initrd initramfs.cpio
```

The firmware, kernel, initramfs, and DTB start at `0x80000000`, `0x80200000`,
`0x84000000`, and `0x87e00000` respectively. The supplied DTB is passed through
as-is. Linux mode exposes a single-hart CLINT at `0x02000000` and a PLIC at
`0x0c000000`. The timer advances once per emulator tick. The serial UART and
Linux boot artifacts are still pending.

Other machine configurations can attach either interrupt device independently
by supplying its MMIO base through `Bus::InterruptMap`.

### Polled console

The emulator connects guest byte loads and stores to standard input and output.

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
