#pragma once

#include "bus.h"
#include "csrs.h"
#include "exception.h"
#include "interrupt.h"

#include <array>
#include <cstdint>
#include <expected>
#include <optional>

#define REGISTER_COUNT 32

const uint8_t BYTE = 8;
const uint8_t HALFWORD = 16;
const uint8_t WORD = 32;
const uint8_t DOUBLEWORD = 64;

class XRegisters {
public:
  void write(uint64_t rd, uint64_t value);
  uint64_t read(uint64_t reg);

private:
  std::array<uint64_t, REGISTER_COUNT> registers = {};
};

enum Mode : uint64_t { USER = 0, SUPERVISOR = 01, MACHINE = 3 };

// TODO: Make the cpu loop. Make an emulator class or just slap the loop
// into the CPU ?

class Cpu {
public:
  Cpu(XRegisters *xregs, Bus *bus);
  std::expected<uint64_t, Exception::ExceptionValue> execute();
  ~Cpu();
  // Program counter
  uint64_t pc = DRAM_BASE;
  // Privilege level
  Mode mode = Mode::MACHINE;
  csr::Csr cregs;

  std::optional<Interrupt::InterruptValue> check_pending_interrupt();

  Interrupt state;
  Exception exception;

private:
  XRegisters *xregs;
  Bus *bus;

  uint64_t fetch();
  std::expected<uint64_t, Exception::ExceptionValue>
  executeGeneral(uint64_t inst);
};
