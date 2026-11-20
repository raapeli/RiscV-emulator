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
  constexpr uint64_t data = DRAM_BASE + 0x3000;
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
  auto fence = [&] {
    bus.write(code + 4, WORD, 0x12000073); // sfence.vma zero, zero
    require(step(virtual_code + 4).has_value(), "SFENCE.VMA must execute in S-mode");
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

  regs.write(Address::SSTATUS, 0);
  result = step();
  require(!result && result.error() == Exception::LoadPageFault,
          "A cached user mapping must still require SUM for supervisor loads");
  regs.write(Address::SSTATUS, csr::Mask::SUM);

  bus.write(code, WORD, 0x00052023); // sw zero, 0(a0)
  require(step().has_value() && bus.read(data, WORD) == 0 &&
              (bus.read(data_pte, DOUBLEWORD) & dirty),
          "Translated store must set D and write to the mapped address");

  bus.write(code, WORD, 0x00052583);
  constexpr uint64_t replacement = data + 0x5000;
  bus.write(replacement, WORD, 77);
  bus.write(data_pte, DOUBLEWORD, pte(replacement, read | write | user));
  require(step().has_value() && cpu.xregs->read(11) == 0,
          "An unfenced PTE change must leave the cached translation usable");
  regs.write(Address::SATP, satp | (1ULL << 44)); // Switch to ASID 1.
  require(step().has_value() && cpu.xregs->read(11) == 77,
          "A different ASID must miss the existing translation");
  regs.write(Address::SATP, satp);
  fence();
  require(step().has_value() && cpu.xregs->read(11) == 77,
          "SFENCE.VMA must make the remapped page visible");

  regs.write(Address::PMPADDR0 + 1, UINT64_MAX);
  regs.write(Address::PMPADDR0, root >> 2);
  regs.write(Address::PMPCFG0, 0x1f10); // Deny the root PTE, allow other memory.
  result = step();
  require(!result && result.error() == Exception::InstructionAccessFault,
          "A PMP change must invalidate translations of newly blocked page tables");
  regs.write(Address::PMPCFG0, 0x1f00);

  bus.write(data_pte, DOUBLEWORD, pte(data, execute | user));
  fence();
  regs.write(Address::SSTATUS, csr::Mask::SUM | csr::Mask::MXR);
  require(step().has_value(), "MXR must allow a load from an execute-only page");
  regs.write(Address::SSTATUS, csr::Mask::SUM);
  result = step();
  require(!result && result.error() == Exception::LoadPageFault,
          "A cached execute-only mapping must still require MXR for loads");
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

  constexpr uint64_t last_parcel = 0xffe;
  bus.write(code + last_parcel, HALFWORD, 0x0513); // Low half of li a0, 7.
  bus.write(data, HALFWORD, 0x0070);
  bus.write(data_pte, DOUBLEWORD, pte(data, execute));
  fence();
  require(step(virtual_code + last_parcel).has_value() &&
              cpu.pc == virtual_data + 2 && cpu.xregs->read(10) == 7,
          "Split instruction fetch must translate both parcels");

  bus.write(data_pte, DOUBLEWORD, 0);
  fence();
  cpu.mode = Mode::SUPERVISOR;
  cpu.pc = virtual_code + last_parcel;
  cpu.oneTick();
  require(regs.load(Address::MCAUSE) == Exception::InstructionPageFault &&
              regs.load(Address::MEPC) == virtual_code + last_parcel &&
              regs.load(Address::MTVAL) == virtual_data,
          "A fault on the second parcel must report its virtual address");

  bus.write(code + last_parcel, HALFWORD, 0x0001); // c.nop
  require(step(virtual_code + last_parcel).has_value() && cpu.pc == virtual_data,
          "A compressed instruction must fetch only its own parcel");

  constexpr uint64_t superpage = 1ULL << 30;
  bus.write(root + 8, DOUBLEWORD, pte(0, read | execute));
  bus.write(data, WORD, 84);
  cpu.xregs->write(10, superpage + data);
  require(step(superpage + code).has_value() &&
              step(superpage + code).has_value() && cpu.xregs->read(11) == 84,
          "Cached addresses within a superpage must preserve the full offset");
}
