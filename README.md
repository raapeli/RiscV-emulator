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
$ git clone https://github.com/raapeli/RiscV-emulator.git
$ cmake -S . -B build -DCMAKE_CXX_COMPILER=clang++
$ cd build && make
```


## Usage

Run the program RiscV with an argument to a RiscV binary with no headers.

Run tests with

``` 
cmake test
```

