#include "bus.h"
#include "cpu.h"
#include <cstdint>
#include <iostream>

#define TEST_CASE(name, actual, expected)                                      \
  do {                                                                         \
    auto _act = (actual);                                                      \
    auto _exp = (expected);                                                    \
    if (_act != _exp) {                                                        \
      std::cerr << "FAILED: " << name << "\n"                                  \
                << "  expected: " << _exp << " " << std::hex << _exp           \
                << std::dec << "\n"                                            \
                << "  got:      " << _act << " " << std::hex << _act           \
                << std::dec << std::endl;                                      \
      return 1;                                                                \
    }                                                                          \
  } while (0)
// ------------------------------------------------------
// Multiplication instruction tests (RV32IM)
// -----------------------------------------------------

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

// ----------------------------------------------------
// Division instruction tests (RV32IM)
// ----------------------------------------------------

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

// ---------------------------------------------------
// ALU instruction wrappers (RV32I)
// ---------------------------------------------------

static inline int32_t add_rv32(int32_t a, int32_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C58533); // add x10, x11, x12
  cpu->execute();
  int32_t result = xregs->read(10);

  return result;
}

static inline int32_t sub_rv32(int32_t a, int32_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x40C58533); // sub x10, x11, x12
  cpu->execute();
  int32_t result = xregs->read(10);

  return result;
}

static inline int32_t sll_rv32(int32_t a, int32_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C59533); // sll x10, x11, x12
  cpu->execute();
  int32_t result = xregs->read(10);

  return result;
}

static inline int32_t slt_rv32(int32_t a, int32_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C5A533); // slt x10, x11, x12
  cpu->execute();
  int32_t result = xregs->read(10);

  return result;
}

static inline int32_t sltu_rv32(uint32_t a, uint32_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C5B533); // sltu x10, x11, x12
  cpu->execute();
  int32_t result = xregs->read(10);

  return result;
}

static inline int32_t xor_rv32(int32_t a, int32_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C5C533); // xor x10, x11, x12
  cpu->execute();
  int32_t result = xregs->read(10);

  return result;
}

static inline int32_t srl_rv32(int32_t a, int32_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C5D533); // srl x10, x11, x12
  cpu->execute();
  int32_t result = xregs->read(10);

  return result;
}

static inline int32_t sra_rv32(int32_t a, int32_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x40C5D533); // sra x10, x11, x12
  cpu->execute();
  int32_t result = xregs->read(10);

  return result;
}

static inline int32_t or_rv32(int32_t a, int32_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C5E533); // or x10, x11, x12
  cpu->execute();
  int32_t result = xregs->read(10);

  return result;
}

static inline int32_t and_rv32(int32_t a, int32_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C5F533); // and x10, x11, x12
  cpu->execute();
  int32_t result = xregs->read(10);

  return result;
}

// --------------------------------------------------
// Immediate ALU instruction wrappers
// --------------------------------------------------
static inline int32_t addi_rv32(int32_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x00558513); // addi x10, x11, 5
  cpu->execute();
  int32_t result = x->read(10);

  return result;
}

static inline int32_t slti_rv32(int32_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x0055A513); // slti x10, x11, 5
  cpu->execute();
  int32_t result = x->read(10);

  return result;
}

static inline int32_t sltiu_rv32(uint32_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x0055B513); // sltiu x10, x11, 5
  cpu->execute();
  int32_t result = x->read(10);

  return result;
}

static inline int32_t xori_rv32(int32_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x0055C513); // xori x10, x11, 5
  cpu->execute();
  int32_t result = x->read(10);

  return result;
}

static inline int32_t ori_rv32(int32_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x0055E513); // ori x10, x11, 5
  cpu->execute();
  int32_t result = x->read(10);

  return result;
}

static inline int32_t andi_rv32(int32_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x0055F513); // andi x10, x11, 5
  cpu->execute();
  int32_t result = x->read(10);

  return result;
}

static inline int32_t slli_rv32(int32_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x00559513); // slli x10, x11, 5
  cpu->execute();
  int32_t result = x->read(10);

  return result;
}

static inline int32_t srli_rv32(int32_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x0055D513); // srli x10, x11, 5
  cpu->execute();
  int32_t result = x->read(10);

  return result;
}

static inline int32_t srai_rv32(int32_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x4055D513); // srai x10, x11, 5
  cpu->execute();
  int32_t result = x->read(10);

  return result;
}
int main() {
  int32_t a = 0x12345678, b = -12345;
  int32_t imm = 5;
  uint32_t shamt = imm & 0x3f;

  TEST_CASE("MUL", mul_rv32(a, b), (int32_t)((int64_t)a * (int64_t)b));
  TEST_CASE("MULH", mulh_rv32(a, b),
            (int32_t)(((int64_t)a * (int64_t)b) >> 32));
  TEST_CASE("MULHSU", mulhsu_rv32(a, (uint32_t)b),
            (int32_t)(((int64_t)a * (uint64_t)(uint32_t)b) >> 32));
  TEST_CASE("MULHU", mulhu_rv32((uint32_t)a, (uint32_t)b),
            (uint32_t)(((uint64_t)(uint32_t)a * (uint64_t)(uint32_t)b) >> 32));

  TEST_CASE("DIV", div_rv32(a, b), a / b);
  TEST_CASE("DIVU", divu_rv32(a, b), (uint32_t)a / (uint32_t)b);
  TEST_CASE("REM", rem_rv32(a, b), a % b);
  TEST_CASE("REMU", remu_rv32(a, b), (uint32_t)a % (uint32_t)b);

  TEST_CASE("ADD", add_rv32(a, b), (int32_t)((uint32_t)a + (uint32_t)b));
  TEST_CASE("SUB", sub_rv32(a, b), (int32_t)((uint32_t)a - (uint32_t)b));
  TEST_CASE("SLL", sll_rv32(a, b), (a << (b & 0x3F)));
  TEST_CASE("SLT", slt_rv32(a, b), ((a < b) ? 1 : 0));
  TEST_CASE("SLTU", sltu_rv32(a, b), ((uint32_t)a < (uint32_t)b ? 1 : 0));
  TEST_CASE("XOR", xor_rv32(a, b), (a ^ b));
  TEST_CASE("SRL", srl_rv32(a, b), ((uint32_t)a >> (b & 0x3F)));
  TEST_CASE("SRA", sra_rv32(a, b), (a >> (b & 0x3F)));
  TEST_CASE("OR", or_rv32(a, b), (a | b));
  TEST_CASE("AND", and_rv32(a, b), (a & b));

  TEST_CASE("ADDI", addi_rv32(a), (int32_t)((uint32_t)a + (uint32_t)imm));
  TEST_CASE("SLTI", slti_rv32(a), (a < imm ? 1 : 0));
  TEST_CASE("SLTIU", sltiu_rv32(a), ((uint32_t)a < (uint32_t)imm ? 1 : 0));
  TEST_CASE("XORI", xori_rv32(a), (a ^ imm));
  TEST_CASE("ORI", ori_rv32(a), (a | imm));
  TEST_CASE("ANDI", andi_rv32(a), (a & imm));
  TEST_CASE("SLLI", slli_rv32(a), (int32_t)((uint32_t)a << shamt));
  TEST_CASE("SRLI", srli_rv32(a), (int32_t)((uint32_t)a >> shamt));
  TEST_CASE("SRAI", srai_rv32(a), (a >> shamt));

  std::cout << "All multiplication instruction tests passed!" << std::endl;
  return 0;
}
