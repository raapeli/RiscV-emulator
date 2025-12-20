#include "exception.h"
#include "cpu.h"
#include "csrs.h"
#include <cstdint>

uint32_t Exception::epc(uint32_t pc) {
  if (exception == 3 || (exception <= 12 && exception >= 8))
    return pc;
  return pc + 4;
}

Trap Exception::take_trap(Cpu *cpu) {

  uint32_t ex_pc = epc(cpu->pc);
  Mode mode = cpu->mode;

  bool medeleg_f = (cpu->cregs.load(csr::Address::MEDELEG) >> exception) & 1;

  if (mode <= Mode::SUPERVISOR && medeleg_f) {

    cpu->mode = Mode::SUPERVISOR;

    cpu->pc = cpu->cregs.load(csr::Address::STVEC) & ~3;

    cpu->cregs.store(csr::Address::SEPC, ex_pc & ~1);

    cpu->cregs.store(csr::Address::SCAUSE, exception);
    cpu->cregs.store(csr::Address::STVAL, ex_pc);

    cpu->cregs.write_bit_sstatus(
        csr::Mask::SSTATUSBit::SPIE,
        cpu->cregs.read_bit_sstatus(csr::Mask::SSTATUSBit::SIE));
    cpu->cregs.write_bit_sstatus(csr::Mask::SSTATUSBit::SIE, 0);
    cpu->cregs.write_bit_sstatus(csr::Mask::SSTATUSBit::SPP,
                                 static_cast<uint32_t>(mode));
  } else {
    cpu->mode = Mode::MACHINE;

    cpu->cregs.store(csr::Address::MEPC, ex_pc & ~1);

    cpu->cregs.store(csr::Address::MCAUSE, exception);

    cpu->cregs.store(csr::Address::MTVAL, ex_pc);

    cpu->cregs.write_bit_mstatus(
        csr::Mask::MSTATUSBit::MPIE,
        cpu->cregs.read_bit_mstatus(csr::Mask::MSTATUSBit::MIE));
    cpu->cregs.write_bit_mstatus(csr::Mask::MSTATUSBit::MIE, 0);
    cpu->cregs.write_bits(csr::Address::MSTATUS, 12, 11,
                          static_cast<uint32_t>(mode));
  }
  return Trap::Fatal;
}
