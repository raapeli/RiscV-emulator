#include "bus.h"
#include "cpu.h"
#include <cstdint>
#include <iostream>

#define TEST_CASE(name, expr)                                                  \
  do {                                                                         \
    if (!(expr)) {                                                             \
      std::cerr << "Test failed: " << name << std::endl;                       \
      return 1;                                                                \
    }                                                                          \
  } while (0)

// MULTIPLICATION TESTS

static inline int32_t mul_rv32(int32_t a, int32_t b) {

  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C58533);
  cpu->execute();
  return xregs->read(10);
}

static inline int32_t mulh_rv32(int32_t a, int32_t b) {

  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C59533);
  cpu->execute();
  return xregs->read(10);
}

static inline int32_t mulhsu_rv32(int32_t a, uint32_t b) {

  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C5A533);
  cpu->execute();
  return xregs->read(10);
}

static inline uint32_t mulhu_rv32(uint32_t a, uint32_t b) {

  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C5B533);
  cpu->execute();
  return xregs->read(10);
}

// DIVISION TESTS

static inline int32_t div_rv32(int32_t a, int32_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);

  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C5C533); // div x10, x11, x12
  cpu->execute();

  int32_t result = xregs->read(10);
  return result;
}

static inline uint32_t divu_rv32(uint32_t a, uint32_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);

  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C5D533); // divu x10, x11, x12
  cpu->execute();

  uint32_t result = xregs->read(10);
  return result;
}

static inline int32_t rem_rv32(int32_t a, int32_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);

  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C5E533); // rem x10, x11, x12
  cpu->execute();

  int32_t result = xregs->read(10);
  return result;
}

static inline uint32_t remu_rv32(uint32_t a, uint32_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);

  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C5F533); // remu x10, x11, x12
  cpu->execute();

  uint32_t result = xregs->read(10);
  return result;
}

int main() {
  int32_t a = 0x12345678, b = -12345;

  TEST_CASE("MUL", mul_rv32(a, b) == (int32_t)((int64_t)a * (int64_t)b));
  TEST_CASE("MULH",
            mulh_rv32(a, b) == (int32_t)(((int64_t)a * (int64_t)b) >> 32));
  TEST_CASE("MULHSU",
            mulhsu_rv32(a, (uint32_t)b) ==
                (int32_t)(((int64_t)a * (uint64_t)(uint32_t)b) >> 32));
  TEST_CASE(
      "MULHU",
      mulhu_rv32((uint32_t)a, (uint32_t)b) ==
          (uint32_t)(((uint64_t)(uint32_t)a * (uint64_t)(uint32_t)b) >> 32));

  TEST_CASE("DIV", div_rv32(a, b) == a / b);
  TEST_CASE("DIVU", divu_rv32(a, b) == (uint32_t)a / (uint32_t)b);
  TEST_CASE("REM", rem_rv32(a, b) == a % b);
  TEST_CASE("REMU", remu_rv32(a, b) == (uint32_t)a % (uint32_t)b);

  std::cout << "All multiplication instruction tests passed!" << std::endl;
  return 0;
}
