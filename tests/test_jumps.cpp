#include "bus.h"
#include "cpu.h"

#include <cstdint>
#include <iostream>
#include <utility>

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

// ------------------------------------------------------------
// Jump instruction wrappers
// ------------------------------------------------------------
static inline std::pair<uint64_t, uint64_t> jal_rv64() {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);

  bus->write(DRAM_BASE, 32, 0x008000EF); // jal x1, 8
  x->write(1, 0);
  cpu->execute();

  uint64_t link = x->read(1);
  uint64_t out_new_pc = cpu->pc;
  return {link, out_new_pc};
}

static inline std::pair<uint64_t, uint64_t> jalr_rv64(uint64_t base_addr) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);

  bus->write(DRAM_BASE, 32, 0x004580E7); // jalr x1, 4(x11)
  x->write(11, base_addr);
  x->write(1, 0);
  cpu->execute();

  uint64_t link = x->read(1);
  uint64_t out_new_pc = cpu->pc;
  return {link, out_new_pc};
}

// ------------------------------------------------------------
// Branch instruction wrappers
// ------------------------------------------------------------

// bytes forward by 8
static constexpr uint64_t BR_OFF8 = 8;

static inline uint64_t beq_rv64(uint64_t v1, uint64_t v2) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);

  bus->write(DRAM_BASE, 32, 0x00628463); // BEQ x5,x6,+8
  x->write(5, v1);
  x->write(6, v2);
  cpu->execute();

  return cpu->pc;
}

static inline uint64_t bne_rv64(uint64_t v1, uint64_t v2) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);

  bus->write(DRAM_BASE, 32, 0x00629463); // BNE x5,x6,+8
  x->write(5, v1);
  x->write(6, v2);
  cpu->execute();

  return cpu->pc;
}

static inline uint64_t blt_rv64(uint64_t v1, uint64_t v2) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);

  bus->write(DRAM_BASE, 32, 0x0062C463); // BLT x5,x6,+8
  x->write(5, v1);
  x->write(6, v2);
  cpu->execute();

  return cpu->pc;
}

static inline uint64_t bge_rv64(uint64_t v1, uint64_t v2) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);

  bus->write(DRAM_BASE, 32, 0x0062D463); // BGE x5,x6,+8
  x->write(5, v1);
  x->write(6, v2);
  cpu->execute();

  return cpu->pc;
}

static inline uint64_t bltu_rv64(uint64_t v1, uint64_t v2) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);

  bus->write(DRAM_BASE, 32, 0x0062E463); // BLTU x5,x6,+8
  x->write(5, v1);
  x->write(6, v2);
  cpu->execute();

  return cpu->pc;
}

static inline uint64_t bgeu_rv64(uint64_t v1, uint64_t v2) {
  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);

  bus->write(DRAM_BASE, 32, 0x0062F463); // BGEU x5,x6,+8
  x->write(5, v1);
  x->write(6, v2);
  cpu->execute();

  return cpu->pc;
}

// ============================================================
// JAL / JALR / BRANCH tests
// ============================================================
int main() {
  // ---- JAL tests ----
  {
    auto [link, new_pc] = jal_rv64();
    TEST_CASE("JAL link value", link, DRAM_BASE + 4);
    TEST_CASE("JAL target pc", new_pc, DRAM_BASE + 8);
  }

  // ---- JALR tests ----
  {
    uint64_t base = 0x1000;
    auto [link, new_pc] = jalr_rv64(base);
    TEST_CASE("JALR link value", link, DRAM_BASE + 4);
    TEST_CASE("JALR target pc", new_pc, (base + 4) & ~1u);
  }

  // ---- BEQ tests ----
  {
    TEST_CASE("BEQ taken", beq_rv64(7, 7), DRAM_BASE + BR_OFF8);
    TEST_CASE("BEQ not taken", beq_rv64(7, 8), DRAM_BASE + 4);
  }

  // ---- BNE tests ----
  {
    TEST_CASE("BNE taken", bne_rv64(7, 8), DRAM_BASE + BR_OFF8);
    TEST_CASE("BNE not taken", bne_rv64(7, 7), DRAM_BASE + 4);
  }

  // ---- BLT (signed) tests ----
  {
    TEST_CASE("BLT taken", blt_rv64((uint64_t)-1, 0), DRAM_BASE + BR_OFF8);
    TEST_CASE("BLT not taken", blt_rv64(0, (uint64_t)-1), DRAM_BASE + 4);
  }

  // ---- BGE (signed) tests ----
  {
    TEST_CASE("BGE taken", bge_rv64(0, (uint64_t)-1), DRAM_BASE + BR_OFF8);
    TEST_CASE("BGE not taken", bge_rv64((uint64_t)-1, 0), DRAM_BASE + 4);
  }

  // ---- BLTU (unsigned) tests ----
  {
    TEST_CASE("BLTU taken", bltu_rv64(1u, 2u), DRAM_BASE + BR_OFF8);
    TEST_CASE("BLTU not taken", bltu_rv64(2u, 1u), DRAM_BASE + 4);
  }

  // ---- BGEU (unsigned) tests ----
  {
    TEST_CASE("BGEU taken", bgeu_rv64(2u, 1u), DRAM_BASE + BR_OFF8);
    TEST_CASE("BGEU not taken", bgeu_rv64(1u, 2u), DRAM_BASE + 4);
  }

  return 0;
}
