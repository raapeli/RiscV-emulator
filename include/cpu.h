#pragma once

#include "bus.h"

#include <cstdint>

#define REGISTER_COUNT 32

const uint8_t BYTE = 8;
const uint8_t HALFWORD = 16;
const uint8_t WORD = 32;

class XRegisters {
public:
  void write(uint32_t rd, uint32_t value);
  uint32_t read(uint32_t reg);

private:
  uint32_t registers[REGISTER_COUNT] = {};
};

class Cpu {
public:
  Cpu(XRegisters *xregs, Bus *bus);
  int execute();
  ~Cpu();
  // Program counter
  uint32_t pc = DRAM_BASE;

private:
  XRegisters *xregs;
  Bus *bus;

  uint32_t fetch();
  int executeGeneral(uint32_t inst);
};
