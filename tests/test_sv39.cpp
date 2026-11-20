#include "cpu.h"

#include <cstdlib>
#include <iostream>

using csr::Address;

static void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

int main() {
  constexpr uint64_t valid = 1, read = 2, write = 4, execute = 8;
  constexpr uint64_t user = 16, accessed = 64, dirty = 128;
  constexpr uint64_t code = DRAM_BASE + 0x1000;
  constexpr uint64_t data = DRAM_BASE + 0x2000;
  constexpr uint64_t root = DRAM_BASE + 0x4000;
  constexpr uint64_t middle = root + 0x1000;
  constexpr uint64_t leaf = middle + 0x1000;
  constexpr uint64_t virtual_code = 0x4000;
  constexpr uint64_t virtual_data = 0x5000;
  constexpr uint64_t data_pte = leaf + 5 * 8;
  constexpr uint64_t unknown_mode = 9ULL << csr::Satp::MODE_SHIFT;

  Bus bus;
  Cpu cpu(&bus);
  auto &regs = *cpu.cregs;
  regs.write(Address::PMPADDR0, UINT64_MAX);
  regs.write(Address::PMPCFG0, 0x1f); // Allow all physical accesses.
  const uint64_t satp = (csr::Satp::SV39 << csr::Satp::MODE_SHIFT) | (root >> 12);
  regs.write(Address::SATP, satp);
  regs.write(Address::SATP, unknown_mode);
  require(regs.load(Address::SATP) == satp, "Unsupported satp mode must be ignored");
  regs.write(Address::MTVEC, DRAM_BASE);

  auto pte = [](uint64_t physical, uint64_t flags) {
    return ((physical >> 12) << 10) | valid | flags;
  };
  bus.write(root, DOUBLEWORD, pte(middle, 0));
  bus.write(middle, DOUBLEWORD, pte(leaf, 0));
  bus.write(leaf + 4 * 8, DOUBLEWORD, pte(code, execute));
  bus.write(data_pte, DOUBLEWORD, pte(data, read | write | user));
  bus.write(data, WORD, 42);
  bus.write(code, WORD, 0x00052583); // lw a1, 0(a0)
  cpu.xregs->write(10, virtual_data);

  auto step = [&](uint64_t pc = virtual_code) {
    cpu.mode = Mode::SUPERVISOR;
    cpu.pc = pc;
    return cpu.execute();
  };
  auto result = step();
  require(!result && result.error() == Exception::LoadPageFault &&
              !(bus.read(data_pte, DOUBLEWORD) & accessed),
          "Without SUM, a supervisor load from a user page must fault");

  regs.write(Address::SSTATUS, csr::Mask::SUM);
  require(step().has_value() && cpu.xregs->read(11) == 42 &&
              (bus.read(data_pte, DOUBLEWORD) & (accessed | dirty)) == accessed &&
              (bus.read(leaf + 4 * 8, DOUBLEWORD) & accessed),
          "Translated fetch and load must set A without setting D");

  bus.write(code, WORD, 0x00052023); // sw zero, 0(a0)
  require(step().has_value() && bus.read(data, WORD) == 0 &&
              (bus.read(data_pte, DOUBLEWORD) & dirty),
          "Translated store must set D and write to the mapped address");

  bus.write(code, WORD, 0x00052583);
  bus.write(data_pte, DOUBLEWORD, pte(data, execute | user));
  regs.write(Address::SSTATUS, csr::Mask::SUM | csr::Mask::MXR);
  require(step().has_value(), "MXR must allow a load from an execute-only page");
  result = step(virtual_data);
  require(!result && result.error() == Exception::InstructionPageFault,
          "SUM must not allow supervisor execution from a user page");

  cpu.xregs->write(10, 1ULL << 39);
  cpu.mode = Mode::SUPERVISOR;
  cpu.pc = virtual_code;
  cpu.oneTick();
  require(regs.load(Address::MCAUSE) == Exception::LoadPageFault &&
              regs.load(Address::MEPC) == virtual_code &&
              regs.load(Address::MTVAL) == (1ULL << 39),
          "Noncanonical addresses must fault and report the original address");
}
