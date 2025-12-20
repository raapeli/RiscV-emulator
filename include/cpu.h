#pragma once

#include "bus.h"
#include "csrs.h"
#include "exception.h"

#include <expected>

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

enum class Mode { USER = 0, SUPERVISOR = 01, MACHINE = 3 };

class Cpu {
public:
  Cpu(XRegisters *xregs, Bus *bus);
  std::expected<uint32_t, Exception::ExceptionValue> execute();
  ~Cpu();
  // Program counter
  uint32_t pc = DRAM_BASE;
  // Privilege level
  Mode mode = Mode::MACHINE;
  csr::Csr cregs;

private:
  XRegisters *xregs;
  Bus *bus;

  uint32_t fetch();
  std::expected<uint32_t, Exception::ExceptionValue>
  executeGeneral(uint32_t inst);
};
