#include "cpu.h"

#include <cstdlib>
#include <iostream>

namespace {

using csr::Address;
using csr::Mask;

constexpr uint64_t CLINT = 0x03000000;
constexpr uint64_t MSIP = CLINT;
constexpr uint64_t MTIMECMP = CLINT + 0x4000;
constexpr uint64_t MTIME = CLINT + 0xbff8;
constexpr uint64_t PLIC = 0x14000000;
constexpr uint64_t MACHINE_ENABLE = PLIC + 0x2000;
constexpr uint64_t MACHINE_CLAIM = PLIC + 0x200004;
constexpr uint64_t SUPERVISOR_ENABLE = PLIC + 0x2080;
constexpr uint64_t SUPERVISOR_THRESHOLD = PLIC + 0x201000;
constexpr uint64_t SUPERVISOR_CLAIM = SUPERVISOR_THRESHOLD + 4;
constexpr uint64_t INTERRUPT = 1ULL << 63;

void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

void test_clint() {
  Bus bus(false, DRAM_BASE, DRAM_SIZE, {CLINT, std::nullopt, std::nullopt});
  require(bus.clint() && !bus.plic() && !bus.contains_mmio_access(PLIC, WORD, false),
          "CLINT can be installed independently");
  Cpu cpu(&bus);
  auto &regs = *cpu.cregs;
  const uint64_t handler = DRAM_BASE + 0x100;
  regs.write(Address::MTVEC, handler);
  regs.write(Address::MIE, Mask::MTIP);
  regs.write_bit_mstatus(Mask::MSTATUSBit::MIE, 1);
  bus.write(DRAM_BASE, WORD, 0x00000013); // nop
  bus.write(handler, WORD, 0x00000013);

  bus.write(MTIME, DOUBLEWORD, 10);
  bus.write(MTIMECMP + 4, WORD, 0);
  bus.write(MTIMECMP, WORD, 12);
  require(bus.read(MTIMECMP, DOUBLEWORD) == 12, "CLINT split compare writes failed");
  regs.write(Address::MIP, Mask::MTIP);
  cpu.oneTick();
  require(cpu.pc == DRAM_BASE + 4 && regs.load(Address::TIME) == 11 &&
              !(regs.load(Address::MIP) & Mask::MTIP),
          "Timer fired before the compare deadline");

  cpu.oneTick();
  require(cpu.pc == handler + 4 && regs.load(Address::TIME) == 12 &&
              regs.load(Address::MEPC) == DRAM_BASE + 4 &&
              regs.load(Address::MCAUSE) == (INTERRUPT | 7),
          "CPU did not take the machine timer interrupt at the deadline");

  bus.write(MTIMECMP, DOUBLEWORD, 20);
  bus.write(MSIP, WORD, 1);
  regs.write(Address::MIE, Mask::MSIP);
  regs.write_bit_mstatus(Mask::MSTATUSBit::MIE, 1);
  cpu.oneTick();
  require(regs.load(Address::MCAUSE) == (INTERRUPT | 3) &&
              !(regs.load(Address::MIP) & Mask::MTIP),
          "Timer rearm or machine software interrupt failed");
  bus.write(MSIP, WORD, 0);
  bus.write(handler + 4, WORD, 0x00000013);
  cpu.oneTick();
  require(!(regs.load(Address::MIP) & Mask::MSIP),
          "Clearing the CLINT software interrupt must clear MSIP");

  regs.write(Address::PMPADDR0, UINT64_MAX);
  regs.write(Address::PMPCFG0, 0x1f);
  bus.write(DRAM_BASE, WORD, 0xc0102573); // csrr a0, time
  cpu.mode = Mode::SUPERVISOR;
  cpu.pc = DRAM_BASE;
  auto denied = cpu.execute();
  require(!denied && denied.error() == Exception::IllegalInstruction,
          "Supervisor time reads require mcounteren.TM");
  regs.write(Address::MCOUNTEREN, 1U << 1);
  require(cpu.execute().has_value() && cpu.xregs->read(10) == bus.read(MTIME, DOUBLEWORD),
          "The time CSR must report the CLINT clock");
}

void test_plic() {
  Bus bus(false, DRAM_BASE, DRAM_SIZE, {std::nullopt, PLIC, std::nullopt});
  require(bus.plic() && !bus.clint() && !bus.contains_mmio_access(MTIME, DOUBLEWORD, false),
          "PLIC can be installed independently");
  Cpu cpu(&bus);
  auto &regs = *cpu.cregs;
  auto &devices = *bus.plic();
  constexpr uint32_t enabled = (1U << 10) | (1U << 11) | (1U << 12);
  bus.write(PLIC + 10 * 4, WORD, 1);
  bus.write(PLIC + 11 * 4, WORD, 3);
  bus.write(PLIC + 12 * 4, WORD, 3);
  bus.write(SUPERVISOR_ENABLE, WORD, enabled);
  bus.write(SUPERVISOR_THRESHOLD, WORD, 3);
  for (unsigned source : {10U, 11U, 12U})
    devices.set_external_level(source, true);
  require(devices.pending_interrupts() == 0 &&
              bus.read(SUPERVISOR_CLAIM, WORD) == 11,
          "PLIC threshold must suppress notifications while claims can poll");
  devices.set_external_level(11, false);
  bus.write(SUPERVISOR_CLAIM, WORD, 11);
  require(bus.read(SUPERVISOR_CLAIM, WORD) == 12,
          "PLIC must prefer higher priority and then lower source ID");
  bus.write(SUPERVISOR_CLAIM, WORD, 12);
  require(bus.read(SUPERVISOR_CLAIM, WORD) == 12,
          "Completing a still-asserted level must reissue the request");
  devices.set_external_level(12, false);
  bus.write(SUPERVISOR_CLAIM, WORD, 12);
  bus.write(SUPERVISOR_THRESHOLD, WORD, 0);

  regs.write(Address::MIDELEG, Mask::SEIP);
  regs.write(Address::MIE, Mask::SEIP);
  regs.write_bit_sstatus(Mask::SSTATUSBit::SIE, 1);
  regs.write(Address::PMPADDR0, UINT64_MAX);
  regs.write(Address::PMPCFG0, 0x1f); // Permit the supervisor instruction fetch.
  bus.write(DRAM_BASE, WORD, 0x344322f3); // csrrs t0, mip, t1
  cpu.xregs->write(6, Mask::SSIP);
  cpu.oneTick();
  require(cpu.xregs->read(5) & Mask::SEIP, "CSR read must see the external SEIP line");

  const uint64_t handler = DRAM_BASE + 0x200;
  regs.write(Address::STVEC, handler);
  bus.write(handler, WORD, 0x00000013);
  cpu.mode = Mode::SUPERVISOR;
  cpu.oneTick();
  require(regs.load(Address::SCAUSE) == (INTERRUPT | 9) && cpu.pc == handler + 4,
          "PLIC must deliver a supervisor external interrupt");
  require(bus.read(SUPERVISOR_CLAIM, WORD) == 10, "Unexpected source at claim");
  devices.set_external_level(10, false);
  bus.write(SUPERVISOR_CLAIM, WORD, 10);
  bus.write(handler + 4, WORD, 0x00000013);
  cpu.oneTick();
  require(regs.load(Address::MIP) == Mask::SSIP,
          "CSRRS must preserve software pending bits without latching external SEIP");

  bus.write(MACHINE_ENABLE, WORD, 1U << 10);
  devices.set_external_level(10, true);
  cpu.mode = Mode::MACHINE;
  regs.write(Address::MTVEC, handler);
  regs.write(Address::MIE, Mask::MEIP);
  regs.write_bit_mstatus(Mask::MSTATUSBit::MIE, 1);
  cpu.oneTick();
  require(regs.load(Address::MCAUSE) == (INTERRUPT | 11) &&
              bus.read(MACHINE_CLAIM, WORD) == 10,
          "PLIC must deliver and claim a machine external interrupt");
}

} // namespace

int main() {
  test_clint();
  test_plic();
}
