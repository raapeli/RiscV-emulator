#include "bus.h"
#include "cpu.h"

#include <iostream>

int main() {
  Bus bus(true);
  const uint64_t end = DRAM_BASE + DRAM_SIZE;

  for (uint8_t size : {BYTE, HALFWORD, WORD, DOUBLEWORD}) {
    const uint64_t last = end - size / 8;
    bus.write(last, size, 0x5a);
    bus.write(last + 1, size, 0xff);

    if (bus.read(last, size) != 0x5a ||
        bus.read(last + 1, size) != UINT64_MAX ||
        bus.read(DRAM_BASE - 1, size) != UINT64_MAX) {
      std::cerr << "Incorrect RAM bounds for " << unsigned(size)
                << "-bit access\n";
      return 1;
    }
  }

  Cpu cpu(&bus);
  auto &regs = *cpu.cregs;
  regs.write(csr::Address::MTVEC, DRAM_BASE + 0x100);
  auto expect_fault = [&](uint64_t pc, uint64_t address,
                          Exception::ExceptionValue cause) {
    cpu.pc = pc;
    cpu.oneTick();
    return regs.load(csr::Address::MCAUSE) == cause &&
           regs.load(csr::Address::MEPC) == pc &&
           regs.load(csr::Address::MTVAL) == address;
  };

  if (!expect_fault(CONSOLE_DATA, CONSOLE_DATA,
                    Exception::InstructionAccessFault)) {
    std::cerr << "Instruction fetch from console MMIO must fault\n";
    return 1;
  }

  cpu.xregs->write(10, end);
  cpu.xregs->write(11, 42);
  bus.write(DRAM_BASE, WORD, 0x00052583); // lw a1, 0(a0)
  if (!expect_fault(DRAM_BASE, end, Exception::LoadAccessFault) ||
      cpu.xregs->read(11) != 42) {
    std::cerr << "Unmapped load must fault before changing its destination\n";
    return 1;
  }

  cpu.xregs->write(10, end - 1);
  bus.write(end - 1, BYTE, 0x5a);
  bus.write(DRAM_BASE, WORD, 0x00052023); // sw zero, 0(a0)
  if (!expect_fault(DRAM_BASE, end - 1, Exception::StoreAmoAccessFault) ||
      bus.read(end - 1, BYTE) != 0x5a) {
    std::cerr << "Store crossing the end of RAM must fault without writing\n";
    return 1;
  }

  constexpr uint64_t linked_base = 0x80000000;
  Bus linked_bus(false, linked_base);
  Cpu linked_cpu(&linked_bus);
  linked_bus.write(linked_base, WORD, 0x02a00513); // li a0, 42
  if (linked_cpu.pc != linked_base || !linked_cpu.execute() ||
      linked_cpu.xregs->read(10) != 42 ||
      linked_bus.contains_ram_access(DRAM_BASE, BYTE)) {
    std::cerr << "CPU must boot at the configured RAM base\n";
    return 1;
  }
}
