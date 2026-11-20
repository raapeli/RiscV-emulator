#include "interrupt.h"
#include "cpu.h"
#include "csrs.h"

void Interrupt::take_trap(Cpu *cpu) {
  uint64_t pc = cpu->pc;
  Mode mode = cpu->mode;

  bool delegated = (cpu->cregs->load(csr::Address::MIDELEG) >> cause) & 1;
  uint64_t trap_cause = (1ULL << 63) | cause;

  if (mode <= Mode::SUPERVISOR && delegated) {

    cpu->mode = Mode::SUPERVISOR;

    uint64_t stvec_val = cpu->cregs->load(csr::Address::STVEC);
    uint64_t vt_offset = 0;

    if ((stvec_val & 3) == 1) {
      vt_offset = cause * 4;
    }
    cpu->pc = (stvec_val & ~3ULL) + vt_offset;

    cpu->cregs->write(csr::Address::SEPC, pc & ~1ULL);

    cpu->cregs->write(csr::Address::SCAUSE, trap_cause);

    cpu->cregs->write(csr::Address::STVAL, 0);

    cpu->cregs->write_bit_sstatus(
        csr::Mask::SSTATUSBit::SPIE,
        cpu->cregs->read_bit_sstatus(csr::Mask::SSTATUSBit::SIE));
    cpu->cregs->write_bit_sstatus(csr::Mask::SSTATUSBit::SIE, 0);

    if (mode == Mode::USER) {
      cpu->cregs->write_bit_sstatus(csr::Mask::SSTATUSBit::SPP, 0);
    } else {
      cpu->cregs->write_bit_sstatus(csr::Mask::SSTATUSBit::SPP, 1);
    }
  } else {

    cpu->mode = Mode::MACHINE;

    uint64_t mtvec_val = cpu->cregs->load(csr::Address::MTVEC);
    uint64_t vt_offset = 0;

    if ((mtvec_val & 3) == 1) {
      vt_offset = cause * 4;
    }

    cpu->pc = (mtvec_val & ~3ULL) + vt_offset;

    cpu->cregs->write(csr::Address::MEPC, pc & ~1ULL);

    cpu->cregs->write(csr::Address::MCAUSE, trap_cause);

    cpu->cregs->write(csr::Address::MTVAL, 0);

    cpu->cregs->write_bit_mstatus(
        csr::Mask::MSTATUSBit::MPIE,
        cpu->cregs->read_bit_mstatus(csr::Mask::MSTATUSBit::MIE));
    cpu->cregs->write_bit_mstatus(csr::Mask::MSTATUSBit::MIE, 0);

    cpu->cregs->write_bits(csr::Address::MSTATUS, csr::Mask::MPP_HIGH_BIT,
                           csr::Mask::MPP_LOW_BIT, mode);
  }
}
