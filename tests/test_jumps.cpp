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

static inline std::pair<uint32_t, uint32_t> jal_rv32() {

  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);

  bus->write(DRAM_BASE, 32, 0x008000EF); // jal x1, 8
  x->write(1, 0);                        // clear link register
  cpu->execute();

  // After execution:
  //   - x1 holds link (pc+4)
  //   - pc = pc + offset (8)
  uint32_t link = x->read(1);
  uint32_t out_new_pc = cpu->pc; // or simulate reading updated PC
  return std::pair(link, out_new_pc);
}

static inline std::pair<uint32_t, uint32_t> jalr_rv32(uint32_t base_addr) {

  Bus *bus = new Bus();
  XRegisters *x = new XRegisters();
  Cpu *cpu = new Cpu(x, bus);

  bus->write(DRAM_BASE, 32, 0x004580E7); // jalr x1, 4(x11)
  x->write(11, base_addr);
  x->write(1, 0);
  cpu->execute();

  // After execution:
  //   - x1 holds pc+4
  //   - pc = (base + imm) & ~1
  uint32_t link = x->read(1);
  uint32_t out_new_pc = cpu->pc;
  return std::pair(link, out_new_pc);
}

// ============================================================
// JAL / JALR tests
// ============================================================
int main() {
  uint32_t new_pc = 0;
  uint32_t link;

  // ---- JAL tests ----
  link = jal_rv32().first;
  new_pc = jal_rv32().second;
  TEST_CASE("JAL link value", link, DRAM_BASE + 4);
  TEST_CASE("JAL target pc", new_pc, DRAM_BASE + 8);

  // ---- JALR tests ----
  uint32_t base = 0x1000;
  link = jalr_rv32(base).first;
  new_pc = jalr_rv32(base).second;
  TEST_CASE("JALR link value", link, DRAM_BASE + 4);
  TEST_CASE("JALR target pc", new_pc, (base + 4) & ~1u);

  return 0;
}
