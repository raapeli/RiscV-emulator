#include "exception.h"
#include "cpu.h"
#include "csrs.h"

Trap Exception::take_trap(Cpu *cpu) {
  const uint64_t pc = cpu->pc;
  uint64_t ex_pc = pc + 4;
  uint64_t tval = ex_pc;
  Trap trap = Trap::Fatal;
  switch (exception) {
  case InstructionAddressMisaligned:
    ex_pc = pc;
    tval = 0;
    trap = Trap::Contained;
    break;
  case IllegalInstruction:
    ex_pc = pc;
    tval = 0;
    trap = Trap::Invisible;
    break;
  case InstructionAccessFault:
  case LoadAccessFault:
  case StoreAmoAccessFault:
    ex_pc = pc;
    tval = access_address;
    trap = Trap::Contained;
    break;
  case Breakpoint:
  case EnvironmentCallUmode:
  case EnvironmentCallSmode:
  case EnvironmentCallMmode:
    ex_pc = tval = pc;
    trap = Trap::Requested;
    break;
  case InstructionPageFault:
    ex_pc = tval = pc;
    trap = Trap::Invisible;
    break;
  case LoadPageFault:
  case StoreAmoPageFault:
    trap = Trap::Invisible;
    break;
  default:
    break;
  }

  Mode mode = cpu->mode;

  bool medeleg_f = (cpu->cregs->load(csr::Address::MEDELEG) >> exception) & 1;

  if (mode <= Mode::SUPERVISOR && medeleg_f) {

    cpu->mode = Mode::SUPERVISOR;

    cpu->pc = cpu->cregs->load(csr::Address::STVEC) & ~3;

    cpu->cregs->write(csr::Address::SEPC, ex_pc & ~1);

    cpu->cregs->write(csr::Address::SCAUSE, exception);
    cpu->cregs->write(csr::Address::STVAL, tval);

    cpu->cregs->write_bit_sstatus(
        csr::Mask::SSTATUSBit::SPIE,
        cpu->cregs->read_bit_sstatus(csr::Mask::SSTATUSBit::SIE));
    cpu->cregs->write_bit_sstatus(csr::Mask::SSTATUSBit::SIE, 0);
    cpu->cregs->write_bit_sstatus(csr::Mask::SSTATUSBit::SPP, mode);
  } else {
    cpu->mode = Mode::MACHINE;

    cpu->pc = cpu->cregs->load(csr::Address::MTVEC) & ~1;

    cpu->cregs->write(csr::Address::MEPC, ex_pc & ~1);

    cpu->cregs->write(csr::Address::MCAUSE, exception);

    cpu->cregs->write(csr::Address::MTVAL, tval);

    cpu->cregs->write_bit_mstatus(
        csr::Mask::MSTATUSBit::MPIE,
        cpu->cregs->read_bit_mstatus(csr::Mask::MSTATUSBit::MIE));
    cpu->cregs->write_bit_mstatus(csr::Mask::MSTATUSBit::MIE, 0);
    cpu->cregs->write_bits(csr::Address::MSTATUS, csr::Mask::MPP_HIGH_BIT,
                           csr::Mask::MPP_LOW_BIT, mode);
  }

  return trap;
}

std::string Exception::get_exception_str(Exception::ExceptionValue excpt) {
  switch (excpt) {
  case InstructionAddressMisaligned:
    return "InstructionAddressMisaligned";
  case InstructionAccessFault:
    return "InstructionAccessFault";
  case IllegalInstruction:
    return "IllegalInstruction";
  case Breakpoint:
    return "Breakpoint";
  case LoadAddressMisaligned:
    return "LoadAddressMisaligned";
  case LoadAccessFault:
    return "LoadAccessFault";
  case StoreAmoAddressMisaligned:
    return "StoreAddressMisaligned";
  case StoreAmoAccessFault:
    return "StoreAccessFault";
  case EnvironmentCallUmode:
    return "ECallUMode";
  case EnvironmentCallSmode:
    return "ECallSMode";
  case Exception::EnvironmentCallMmode:
    return "ECallMMode";
  case InstructionPageFault:
    return "InstructionPageFault";
  case LoadPageFault:
    return "LoadPageFault";
  case StoreAmoPageFault:
    return "StorePageFault";
  default:
    return "Unknown exception";
  }
}
