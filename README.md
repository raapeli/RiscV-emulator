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
