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
                << "  expected: " << _exp << " 0x" << std::hex << _exp         \
                << std::dec << "\n"                                            \
                << "  got:      " << _act << " 0x" << std::hex << _act         \
                << std::dec << std::endl;                                      \
      return 1;                                                                \
    }                                                                          \
  } while (0)
// ------------------------------------------------------
// Multiplication instruction tests (RV32IM)
// -----------------------------------------------------

static inline int64_t mul_rv64(int64_t a, int64_t b) {

  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C58533);
  cpu->execute();
  return xregs->read(10);
}

static inline int64_t mulh_rv64(int64_t a, int64_t b) {

  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C59533);
  cpu->execute();
  return xregs->read(10);
}

static inline int64_t mulhsu_rv64(int64_t a, int64_t b) {

  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C5A533);
  cpu->execute();
  return xregs->read(10);
}

static inline int64_t mulhu_rv64(int64_t a, int64_t b) {

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

static inline int64_t div_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);

  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C5C533); // div x10, x11, x12
  cpu->execute();

  int64_t result = xregs->read(10);
  return result;
}

static inline uint64_t divu_rv64(uint64_t a, uint64_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);

  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C5D533); // divu x10, x11, x12
  cpu->execute();

  uint64_t result = xregs->read(10);
  return result;
}

static inline int64_t rem_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);

  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C5E533); // rem x10, x11, x12
  cpu->execute();

  int64_t result = xregs->read(10);
  return result;
}

static inline uint64_t remu_rv64(uint64_t a, uint64_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);

  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C5F533); // remu x10, x11, x12
  cpu->execute();

  uint64_t result = xregs->read(10);
  return result;
}

// ---------------------------------------------------
// ALU instruction wrappers (RV32I)
// ---------------------------------------------------

static inline int64_t add_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C58533); // add x10, x11, x12
  cpu->execute();
  int64_t result = xregs->read(10);

  return result;
}

static inline int64_t sub_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x40C58533); // sub x10, x11, x12
  cpu->execute();
  int64_t result = xregs->read(10);

  return result;
}

static inline int64_t sll_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C59533); // sll x10, x11, x12
  cpu->execute();
  int64_t result = xregs->read(10);

  return result;
}

static inline int64_t slt_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C5A533); // slt x10, x11, x12
  cpu->execute();
  int64_t result = xregs->read(10);

  return result;
}

static inline int64_t sltu_rv64(uint64_t a, uint64_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C5B533); // sltu x10, x11, x12
  cpu->execute();
  int64_t result = xregs->read(10);

  return result;
}

static inline int64_t xor_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C5C533); // xor x10, x11, x12
  cpu->execute();
  int64_t result = xregs->read(10);

  return result;
}

static inline int64_t srl_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C5D533); // srl x10, x11, x12
  cpu->execute();
  int64_t result = xregs->read(10);

  return result;
}

static inline int64_t sra_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x40C5D533); // sra x10, x11, x12
  cpu->execute();
  int64_t result = xregs->read(10);

  return result;
}

static inline int64_t or_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C5E533); // or x10, x11, x12
  cpu->execute();
  int64_t result = xregs->read(10);

  return result;
}

static inline int64_t and_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *xregs = new XRegisters();
  Cpu *cpu = new Cpu(xregs, bus);
  xregs->write(11, a);
  xregs->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C5F533); // and x10, x11, x12
  cpu->execute();
  int64_t result = xregs->read(10);

  return result;
}

// --------------------------------------------------
// Immediate ALU instruction wrappers
// --------------------------------------------------
static inline int64_t addi_rv64(int64_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x00558513); // addi x10, x11, 5
  cpu->execute();
  int64_t result = x->read(10);

  return result;
}

static inline int64_t slti_rv64(int64_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x0055A513); // slti x10, x11, 5
  cpu->execute();
  int64_t result = x->read(10);

  return result;
}

static inline int64_t sltiu_rv64(uint64_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x0055B513); // sltiu x10, x11, 5
  cpu->execute();
  int64_t result = x->read(10);

  return result;
}

static inline int64_t xori_rv64(int64_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x0055C513); // xori x10, x11, 5
  cpu->execute();
  int64_t result = x->read(10);

  return result;
}

static inline int64_t ori_rv64(int64_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x0055E513); // ori x10, x11, 5
  cpu->execute();
  int64_t result = x->read(10);

  return result;
}

static inline int64_t andi_rv64(int64_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x0055F513); // andi x10, x11, 5
  cpu->execute();
  int64_t result = x->read(10);

  return result;
}

static inline int64_t slli_rv64(int64_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x00559513); // slli x10, x11, 5
  cpu->execute();
  int64_t result = x->read(10);

  return result;
}

static inline int64_t srli_rv64(int64_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x0055D513); // srli x10, x11, 5
  cpu->execute();
  int64_t result = x->read(10);

  return result;
}

static inline int64_t srai_rv64(int64_t a) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32, 0x4055D513); // srai x10, x11, 5
  cpu->execute();
  int64_t result = x->read(10);

  return result;
}

// ------------------------------------------------------------
// U‑type instruction wrappers (RV32I)
// ------------------------------------------------------------
static inline int64_t lui_rv64() {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  bus->write(DRAM_BASE, 32, 0xABCDE537);
  cpu->execute();
  int64_t result = x->read(10);
  return result;
}

static inline int64_t auipc_rv64() {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  bus->write(DRAM_BASE, 32, 0xABCDE517);
  cpu->execute();
  int64_t result = x->read(10);
  return result;
}

// -----------------------------------------------------------
// Load instruction wrappers
// -----------------------------------------------------------
static inline int64_t lb_rv64(int8_t memval) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  bus->write(DRAM_BASE + 1000, 32, memval);
  x->write(11, DRAM_BASE + 500);
  bus->write(DRAM_BASE, 32, 0x1F458603); // lb x12, 500(x11)
  cpu->execute();
  int64_t result = x->read(12);
  return result;
}

static inline int64_t lh_rv64(int16_t memval) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  bus->write(DRAM_BASE + 1000, 32, memval);
  x->write(11, DRAM_BASE + 500);
  bus->write(DRAM_BASE, 32, 0x1F459603); // lh x12, 500(x11)
  cpu->execute();
  int64_t result = x->read(12);
  return result;
}

static inline int64_t lw_rv64(int64_t memval) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  bus->write(DRAM_BASE + 1000, 32, memval);
  x->write(11, DRAM_BASE + 500);
  bus->write(DRAM_BASE, 32, 0x1F45A603); // lw x12, 500(x11)
  cpu->execute();
  int64_t result = x->read(12);
  return result;
}

static inline int64_t lwu_rv64(int64_t memval) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  bus->write(DRAM_BASE + 1000, 32, memval);
  x->write(11, DRAM_BASE + 500);
  bus->write(DRAM_BASE, 32, 0x1F45e603); // lwu x12, 500(x11)
  cpu->execute();
  int64_t result = x->read(12);
  return result;
}

static inline int64_t ld_rv64(int64_t memval) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  bus->write(DRAM_BASE + 1000, 64, memval);
  x->write(11, DRAM_BASE + 500);
  bus->write(DRAM_BASE, 32, 0x1F45b603); // ld x12, 500(x11)
  cpu->execute();
  int64_t result = x->read(12);
  return result;
}

static inline int64_t lbu_rv64(uint8_t memval) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  bus->write(DRAM_BASE + 1000, 32, memval);
  x->write(11, DRAM_BASE + 500);
  bus->write(DRAM_BASE, 32, 0x1F45C603); // lbu x12, 500(x11)
  cpu->execute();
  int64_t result = x->read(12);
  return result;
}

static inline int64_t lhu_rv64(uint16_t memval) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  bus->write(DRAM_BASE + 1000, 32, memval);
  x->write(11, DRAM_BASE + 500);
  bus->write(DRAM_BASE, 32, 0x1F45D603); // lhu x12, 500(x11)
  cpu->execute();
  int64_t result = x->read(12);
  return result;
}

// ------------------------------------------------------------
// W-instruction wrappers
// ------------------------------------------------------------

static inline uint64_t addw_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  x->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C5853b); // addw x10, x11, x12
  cpu->execute();
  return x->read(10);
}

static inline uint64_t subw_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  x->write(12, b);
  bus->write(DRAM_BASE, 32, 0x40C5853B); // subw x10, x11, x12
  cpu->execute();
  return x->read(10);
}

static inline uint64_t sllw_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  x->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C5953B); // sllw x10, x11, x12
  cpu->execute();
  return x->read(10);
}

inline uint64_t srlw_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  x->write(12, b);
  bus->write(DRAM_BASE, 32, 0x00C5D53B); // srlw x10, x11, x12
  cpu->execute();
  return x->read(10);
}

static inline uint64_t sraw_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  x->write(12, b);
  bus->write(DRAM_BASE, 32, 0x40C5D53B); // sraw x10, x11, x12
  cpu->execute();
  return x->read(10);
}

static inline uint64_t mulw_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  x->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C5853B); // mulw x10, x11, x12
  cpu->execute();
  return x->read(10);
}

static inline uint64_t divw_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  x->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C5C53B); // divw x10, x11, x12
  cpu->execute();
  return x->read(10);
}

static inline uint64_t divuw_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  x->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C5D53B); // divuw x10, x11, x12
  cpu->execute();
  return x->read(10);
}

static inline uint64_t remw_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  x->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C5E53B); // remw x10, x11, x12
  cpu->execute();
  return x->read(10);
}

static inline uint64_t remuw_rv64(int64_t a, int64_t b) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  x->write(12, b);
  bus->write(DRAM_BASE, 32, 0x02C5F53B); // remuw x10, x11, x12
  cpu->execute();
  return x->read(10);
}

// W-Extension Immediate (rs1=11, rd=10)
static inline uint64_t addiw_rv64(int64_t a, int64_t imm) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32,
             0x0005851B | ((imm & 0xFFF) << 20)); // addiw x10, x11, imm
  cpu->execute();
  return x->read(10);
}

static inline uint64_t slliw_rv64(int64_t a, int64_t shamt) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32,
             0x0005C51B | ((shamt & 0x1F) << 20)); // slliw x10, x11, shamt
  cpu->execute();
  return x->read(10);
}

static inline uint64_t srliw_rv64(int64_t a, int64_t shamt) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32,
             0x0005D51B | ((shamt & 0x1F) << 20)); // srliw x10, x11, shamt
  cpu->execute();
  return x->read(10);
}

static inline uint64_t sraiw_rv64(int64_t a, int64_t shamt) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(11, a);
  bus->write(DRAM_BASE, 32,
             0x4005D51B | ((shamt & 0x1F) << 20)); // sraiw x10, x11, shamt
  cpu->execute();
  return x->read(10);
}

// ------------------------------------------------------------
// Store instruction wrappers
// ------------------------------------------------------------
static inline uint64_t sb_rv64(uint8_t val) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(12, val);
  x->write(11, DRAM_BASE + 500);
  bus->write(DRAM_BASE + 1000, WORD, 0xBBBBBBBB);
  bus->write(DRAM_BASE, 32, 0x1EC58A23); // sb x12, 500(x11)
  cpu->execute();
  int64_t result = bus->read(DRAM_BASE + 1000, WORD);
  return result;
}

static inline uint64_t sh_rv64(uint16_t val) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(12, val);
  x->write(11, DRAM_BASE + 500);
  bus->write(DRAM_BASE + 1000, WORD, 0xBBBBBBBB);
  bus->write(DRAM_BASE, 32, 0x1EC59A23); // sh x12, 500(x11)
  cpu->execute();
  int64_t result = bus->read(DRAM_BASE + 1000, WORD);
  return result;
}

static inline uint64_t sw_rv64(uint64_t val) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(12, val);
  x->write(11, DRAM_BASE + 500);
  x->write(12, val);
  bus->write(DRAM_BASE, 32, 0x1EC5AA23); // sw x12, 500(x11)
  cpu->execute();
  int64_t result = bus->read(DRAM_BASE + 1000, WORD);
  return result;
}

static inline uint64_t sd_rv64(uint64_t val) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);
  x->write(12, val);
  x->write(11, DRAM_BASE + 500);
  x->write(12, val);
  bus->write(DRAM_BASE, 32, 0x1EC5BA23); // sw x12, 500(x11)
  cpu->execute();
  int64_t result = bus->read(DRAM_BASE + 1000, DOUBLEWORD);
  return result;
}
// ------------------------------------------------------------
// Expected-value helpers
// ------------------------------------------------------------
static inline int64_t expect_lui(uint64_t imm20) {
  return static_cast<int64_t>(imm20 << 12);
}

static inline int64_t expect_auipc(uint64_t imm20) {
  return static_cast<int64_t>(DRAM_BASE + (imm20 << 12));
}

int main() {
  int64_t a = 0x12345678, b = -12345;
  int64_t imm = 5;
  uint64_t shamt = imm & 0x3f;
  uint64_t imm20 = 0xabcde;
  int64_t a32w = 0x50502323, b32w = 0xFFFF8245;
  uint32_t imm32w = 123;

  TEST_CASE("MUL", mul_rv64(a, b), (int64_t)(a * b));
  TEST_CASE("MULH", mulh_rv64(a, b),
            (int64_t)(((__int128_t)a * (__int128_t)b) >> 64));
  TEST_CASE("MULHSU", mulhsu_rv64(a, b),
            (int64_t)((((__uint128_t)(__int128_t)(int64_t)a *
                        (__uint128_t)(uint64_t)b) >>
                       64)));
  TEST_CASE(
      "MULHU", mulhu_rv64(a, b),
      (int64_t)(((__uint128_t)(uint64_t)a * (__uint128_t)(uint64_t)b) >> 64));

  TEST_CASE("DIV", div_rv64(a, b), a / b);
  TEST_CASE("DIVU", divu_rv64(a, b), (uint64_t)a / (uint64_t)b);
  TEST_CASE("REM", rem_rv64(a, b), a % b);
  TEST_CASE("REMU", remu_rv64(a, b), (uint64_t)a % (uint64_t)b);

  TEST_CASE("ADD", add_rv64(a, b), (int64_t)((uint64_t)a + (uint64_t)b));
  TEST_CASE("SUB", sub_rv64(a, b), (int64_t)((uint64_t)a - (uint64_t)b));
  TEST_CASE("SLL", sll_rv64(a, b), (a << (b & 0x3F)));
  TEST_CASE("SLT", slt_rv64(a, b), ((a < b) ? 1 : 0));
  TEST_CASE("SLTU", sltu_rv64(a, b), ((uint64_t)a < (uint64_t)b ? 1 : 0));
  TEST_CASE("XOR", xor_rv64(a, b), (a ^ b));
  TEST_CASE("SRL", srl_rv64(a, b), (int64_t)((uint64_t)a >> (b & 0x3F)));
  TEST_CASE("SRA", sra_rv64(a, b), (a >> (b & 0x3F)));
  TEST_CASE("OR", or_rv64(a, b), (a | b));
  TEST_CASE("AND", and_rv64(a, b), (a & b));

  TEST_CASE("ADDI", addi_rv64(a), (int64_t)((uint64_t)a + (uint64_t)imm));
  TEST_CASE("SLTI", slti_rv64(a), (a < imm ? 1 : 0));
  TEST_CASE("SLTIU", sltiu_rv64(a), ((uint64_t)a < (uint64_t)imm ? 1 : 0));
  TEST_CASE("XORI", xori_rv64(a), (a ^ imm));
  TEST_CASE("ORI", ori_rv64(a), (a | imm));
  TEST_CASE("ANDI", andi_rv64(a), (a & imm));
  TEST_CASE("SLLI", slli_rv64(a), (int64_t)((uint64_t)a << shamt));
  TEST_CASE("SRLI", srli_rv64(a), (int64_t)((uint64_t)a >> shamt));
  TEST_CASE("SRAI", srai_rv64(a), (a >> shamt));

  TEST_CASE("ADDW", addw_rv64(a32w, b32w),
            (uint64_t)(int32_t)((int32_t)a32w + (int32_t)b32w));
  TEST_CASE("SUBW", subw_rv64(a32w, b32w),
            (uint64_t)(int32_t)((int32_t)a32w - (int32_t)b32w));
  TEST_CASE("SLLW", sllw_rv64(a32w, b32w),
            (uint64_t)(int32_t)((uint32_t)a32w << (b32w & 0x1F)));
  TEST_CASE("SRLW", srlw_rv64(a32w, b32w),
            (uint64_t)((uint32_t)a32w >> (b32w & 0x1F)));
  TEST_CASE("SRAW", sraw_rv64(a32w, b32w),
            (uint64_t)(int32_t)((int32_t)a32w >> (b32w & 0x1F)));

  TEST_CASE("MULW", mulw_rv64(a32w, b32w),
            (uint64_t)(int32_t)((int64_t)a32w * (int64_t)b32w));

  TEST_CASE("DIVW", divw_rv64(a32w, b32w),
            (uint64_t)(int32_t)((int32_t)a32w / (int32_t)b32w));
  TEST_CASE("DIVUW", divuw_rv64(a32w, b32w),
            (uint64_t)((uint32_t)a32w / (uint32_t)b32w));
  TEST_CASE("REMW", remw_rv64(a32w, b32w),
            (uint64_t)(int32_t)((int32_t)a32w % (int32_t)b32w));
  TEST_CASE("REMUW", remuw_rv64(a32w, b32w),
            (uint64_t)((uint32_t)a32w % (uint32_t)b32w));

  TEST_CASE("ADDIW positive", addiw_rv64(a32w, imm32w),
            (uint64_t)(int32_t)((int64_t)a32w + imm32w));
  TEST_CASE("ADDIW negative", addiw_rv64(-1, imm32w),
            (uint64_t)(int32_t)((int64_t)-1 + imm32w));
  TEST_CASE("SLLIW", slliw_rv64(a32w, imm32w),
            (uint64_t)(int32_t)((uint64_t)a32w << imm32w));
  TEST_CASE("SRLIW", srliw_rv64(a32w, imm32w),
            (uint64_t)((uint32_t)a32w >> imm32w));
  TEST_CASE("SRAIW", sraiw_rv64(a32w, imm32w),
            (uint64_t)(int32_t)((int32_t)a32w >> imm32w));

  TEST_CASE("LUI", lui_rv64(), expect_lui(imm20));
  TEST_CASE("AUIPC", auipc_rv64(), expect_auipc(imm20));

  TEST_CASE("LB  positive", lb_rv64(0x7F), 0x000000000000007F);
  TEST_CASE("LBU positive", lbu_rv64(0x7F), 0x000000000000007F);
  TEST_CASE("LB  negative", lb_rv64(0x80), (int64_t)0xFFFFFFFFFFFFFF80);
  TEST_CASE("LBU negative", lbu_rv64(0x80), 0x0000000000000080);

  TEST_CASE("LH  positive", lh_rv64(0x7FFF), 0x0000000000007FFF);
  TEST_CASE("LHU positive", lhu_rv64(0x7FFF), 0x0000000000007FFF);
  TEST_CASE("LH  negative", lh_rv64(0x8000), (int64_t)0xFFFFFFFFFFFF8000);
  TEST_CASE("LHU negative", lhu_rv64(0x8000), 0x0000000000008000);

  TEST_CASE("LW positive", lw_rv64(0x87654321), (int64_t)0xFFFFFFFF87654321);
  TEST_CASE("LW negative", lw_rv64(0xF2345678), (int64_t)0xFFFFFFFFF2345678);

  TEST_CASE("LWU positive", lwu_rv64(0x87654321), 0x0000000087654321);
  TEST_CASE("LWU negative", lwu_rv64(0xF2345678), 0x00000000F2345678);

  TEST_CASE("LD positive", ld_rv64(0x123456789ABCDEF0), 0x123456789ABCDEF0);
  TEST_CASE("LD negative", ld_rv64(0xF23456789ABCDEF0),
            (int64_t)0xF23456789ABCDEF0);

  TEST_CASE("SB", sb_rv64(0xAA), 0xFFFFFFFFBBBBBBAA);
  TEST_CASE("SH", sh_rv64(0xAAAA), 0xFFFFFFFFBBBBAAAA);
  TEST_CASE("SW", sw_rv64(0xDEADBEEF), 0xFFFFFFFFDEADBEEF);
  TEST_CASE("SD", sd_rv64(0xDEADBEEFDEADBEEF), 0xDEADBEEFDEADBEEF);
  std::cout << "All multiplication instruction tests passed!" << std::endl;
  return 0;
}
