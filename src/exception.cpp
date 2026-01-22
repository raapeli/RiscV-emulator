#include "exception.h"
#include "cpu.h"
#include "csrs.h"

uint64_t Exception::epc(uint64_t pc) {
  if (exception == 3 || (exception <= 12 && exception >= 8))
    return pc;
  return pc + 4;
}

Trap Exception::take_trap(Cpu *cpu) {

  uint64_t ex_pc = epc(cpu->pc);
  Mode mode = cpu->mode;

  bool medeleg_f = (cpu->cregs->load(csr::Address::MEDELEG) >> exception) & 1;

  if (mode <= Mode::SUPERVISOR && medeleg_f) {

    cpu->mode = Mode::SUPERVISOR;

    cpu->pc = cpu->cregs->load(csr::Address::STVEC) & ~3;

    cpu->cregs->write(csr::Address::SEPC, ex_pc & ~1);

    cpu->cregs->write(csr::Address::SCAUSE, exception);
    cpu->cregs->write(csr::Address::STVAL, ex_pc);

    cpu->cregs->write_bit_sstatus(
        csr::Mask::SSTATUSBit::SPIE,
        cpu->cregs->read_bit_sstatus(csr::Mask::SSTATUSBit::SIE));
    cpu->cregs->write_bit_sstatus(csr::Mask::SSTATUSBit::SIE, 0);
    cpu->cregs->write_bit_sstatus(csr::Mask::SSTATUSBit::SPP, mode);
  } else {
    cpu->mode = Mode::MACHINE;

    cpu->cregs->write(csr::Address::MEPC, ex_pc & ~1);

    cpu->cregs->write(csr::Address::MCAUSE, exception);

    cpu->cregs->write(csr::Address::MTVAL, ex_pc);

    cpu->cregs->write_bit_mstatus(
        csr::Mask::MSTATUSBit::MPIE,
        cpu->cregs->read_bit_mstatus(csr::Mask::MSTATUSBit::MIE));
    cpu->cregs->write_bit_mstatus(csr::Mask::MSTATUSBit::MIE, 0);
    cpu->cregs->write_bits(csr::Address::MSTATUS, 12, 11, mode);
  }

  switch (exception) {
  case Exception::InstructionAddressMisaligned:
  case Exception::InstructionAccessFault:
    return Trap::Fatal;

  case Exception::IllegalInstruction:
    return Trap::Invisible;

  case Exception::Breakpoint:
    return Trap::Requested;

  case Exception::LoadAddressMisaligned:
  case Exception::LoadAccessFault:
  case Exception::StoreAmoAddressMisaligned:
  case Exception::StoreAmoAccessFault:
    return Trap::Fatal;

  case Exception::EnvironmentCallUmode:
  case Exception::EnvironmentCallSmode:
  case Exception::EnvironmentCallMmode:
    return Trap::Requested;

  case Exception::InstructionPageFault:
  case Exception::LoadPageFault:
  case Exception::StoreAmoPageFault:
    return Trap::Invisible;

  default:
    return Trap::Fatal; // or handle other cases
  }
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
