#include "cpu.h"

#include <cstdlib>
#include <iostream>

using csr::Address;
using csr::Csr;

static void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

static void test_matching() {
  Csr regs;
  constexpr uint64_t base = DRAM_BASE;
  require(!regs.permits_access(base, 1, Csr::PMP_READ, false),
          "User access without a matching entry must fail");

  regs.write(Address::PMPADDR0, base >> 2);
  regs.write(Address::PMPADDR0 + 1, (base + 8) >> 2);
  regs.write(Address::PMPCFG0, 0x8900); // Locked, read-only TOR entry 1.
  regs.write(Address::PMPADDR0, 0);
  regs.write(Address::PMPADDR0 + 1, 0);
  regs.write(Address::PMPCFG0, 0);
  require(regs.load(Address::PMPADDR0) == base >> 2 &&
              regs.load(Address::PMPADDR0 + 1) == (base + 8) >> 2 &&
              regs.load(Address::PMPCFG0) == 0x8900,
          "Locked TOR must retain both bounds and its configuration");

  regs.write(Address::PMPADDR0 + 2, (base >> 2) | 1);
  regs.write(Address::PMPCFG0, 0x1b0000); // Overlapping RW NAPOT entry 2.
  require(regs.permits_access(base, 8, Csr::PMP_READ, false) &&
              !regs.permits_access(base, 8, Csr::PMP_WRITE, true) &&
              !regs.permits_access(base + 4, 8, Csr::PMP_READ, false) &&
              regs.permits_access(base + 8, 8, Csr::PMP_WRITE, false),
          "The first matching entry must permit every byte of an access");

  regs.write(Address::PMPADDR0 + 15, (base + 32) >> 2);
  regs.write(Address::PMPCFG2, 0x11ULL << 56); // Read-only NA4 entry 15.
  require(regs.permits_access(base + 32, 4, Csr::PMP_READ, false) &&
              !regs.permits_access(base + 36, 1, Csr::PMP_READ, false),
          "NA4 must match exactly four bytes");
}

static void test_cpu_accesses() {
  Bus bus;
  Cpu cpu(&bus);
  Csr &regs = *cpu.cregs;
  constexpr uint64_t data = DRAM_BASE + 0x100;
  regs.write(Address::PMPADDR0, DRAM_BASE >> 2);
  regs.write(Address::PMPADDR0 + 1, data >> 2);
  regs.write(Address::PMPCFG0, 0x191d); // RX code and read-only data, 8 bytes each.
  cpu.xregs->write(10, data);
  bus.write(data, WORD, 42);

  auto execute = [&](uint32_t inst) {
    cpu.pc = DRAM_BASE;
    bus.write(cpu.pc, WORD, inst);
    return cpu.execute();
  };
  cpu.mode = Mode::USER;
  require(execute(0x00052583).has_value() && cpu.xregs->read(11) == 42,
          "User load from readable data must succeed");
  auto result = execute(0x00052023); // sw zero, 0(a0)
  require(!result && result.error() == Exception::StoreAmoAccessFault,
          "User store to read-only data must fault");
  result = execute(0x000525af); // amoadd.w a1, zero, (a0)
  require(!result && result.error() == Exception::StoreAmoAccessFault &&
              bus.read(data, WORD) == 42,
          "AMO must check both permissions before touching memory");
  cpu.pc = data;
  result = cpu.execute();
  require(!result && result.error() == Exception::InstructionAccessFault,
          "Instruction fetch from non-executable data must fault");

  cpu.mode = Mode::MACHINE;
  regs.write_bit_mstatus(csr::Mask::MSTATUSBit::MPRV, 1);
  result = execute(0x00052023);
  require(!result && result.error() == Exception::StoreAmoAccessFault,
          "An MPRV data access must use the saved user privilege");
  regs.write_bit_mstatus(csr::Mask::MSTATUSBit::MPRV, 0);
  require(execute(0x00052023).has_value() && bus.read(data, WORD) == 0,
          "Machine mode must bypass unlocked entry permissions");
}

static void test_cache_updates() {
  Csr first;
  Csr second;
  constexpr uint64_t base = DRAM_BASE;
  first.write(Address::PMPADDR0, base >> 2);
  first.write(Address::PMPADDR0 + 1, (base + 8) >> 2);
  first.write(Address::PMPCFG0, 0x0900); // Read-only TOR entry 1.
  require(first.permits_access(base, 8, Csr::PMP_READ, false) &&
              !second.permits_access(base, 1, Csr::PMP_READ, false),
          "Each hart must have its own decoded PMP regions");

  first.write(Address::PMPADDR0, (base + 4) >> 2);
  require(!first.permits_access(base, 1, Csr::PMP_READ, false) &&
              first.permits_access(base + 4, 4, Csr::PMP_READ, false),
          "Writing the preceding address must update a TOR region's lower bound");

  second.write(Address::PMPADDR0, UINT64_MAX);
  second.write(Address::PMPCFG0, 0x19); // Maximum-sized read-only NAPOT entry.
  require(second.permits_access((1ULL << 56) - 1, 1, Csr::PMP_READ, false) &&
              !second.permits_access(1ULL << 56, 1, Csr::PMP_READ, false) &&
              !first.permits_access(base, 1, Csr::PMP_READ, false),
          "Maximum NAPOT coverage must respect the physical limit and hart");
  first.write(Address::PMPCFG0, 0);
  require(!first.permits_access(base + 4, 4, Csr::PMP_READ, false) &&
              second.permits_access(base, 1, Csr::PMP_READ, false),
          "Disabling a region must remove it from only its owning hart");
}

int main() {
  test_matching();
  test_cpu_accesses();
  test_cache_updates();
}
