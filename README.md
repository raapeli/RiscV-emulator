# Risc-V emulator

A Risc-V emulator built in modern C++23. 

**This is still a WIP with more features to come**

## Features

- [X] RV32I/RV64I
- [X] Exceptions 
- [X] Privileged ISA
- [X] CSRs
- [ ] Virtual memory
- [ ] Peripheral devices

## Build

### Requirements

```cmake clang```

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

All tests are not yet passing

## Usage

Run the program RiscV with an argument to a RiscV binary with no headers.

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
