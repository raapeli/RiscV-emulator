#include "cpu.h"
#include "exception.h"
#include <print>

void XRegisters::write(uint64_t dest, uint64_t value) {
  if (dest != 0) {
    this->registers[dest] = value;
  }
}

uint64_t XRegisters::read(uint64_t reg) { return this->registers[reg]; }

Cpu::Cpu(Bus *bus) {
  this->xregs = new XRegisters();
  this->bus = bus;
  this->cregs = new csr::Csr();
}

uint64_t Cpu::fetch() { return bus->read(pc, WORD); }

std::optional<Interrupt::InterruptValue> Cpu::check_pending_interrupt() {
  if (this->mode == Mode::MACHINE) {
    if (cregs->read_bit_mstatus(csr::Mask::MSTATUSBit::MIE) == 0)
      return std::nullopt;
  } else if (this->mode == Mode::SUPERVISOR) {
    if (cregs->read_bit_sstatus(csr::Mask::SSTATUSBit::SIE) == 0)
      return std::nullopt;
  }

  uint64_t mie = cregs->load(csr::Address::MIE);
  uint64_t mip = cregs->load(csr::Address::MIP);

  uint64_t pending = mie & mip;

  if (pending == 0) {
    return std::nullopt;
  }
  if (pending & csr::Mask::MEIP) {
    cregs->write_bit(csr::Address::MIP, csr::Mask::MEIP_BIT, 0);
    return Interrupt::MachineExternalInterrupt;
  } else if (pending & csr::Mask::MSIP) {
    cregs->write_bit(csr::Address::MIP, csr::Mask::MSIP_BIT, 0);
    return Interrupt::MachineSoftwareInterrupt;
  } else if (pending & csr::Mask::MTIP) {
    cregs->write_bit(csr::Address::MIP, csr::Mask::MTIP_BIT, 0);
    return Interrupt::MachineTimerInterrupt;
  } else if (pending & csr::Mask::SEIP_BIT) {
    cregs->write_bit(csr::Address::MIP, csr::Mask::SEIP_BIT, 0);
    return Interrupt::SupervisorExternalInterrupt;
  } else if (pending & csr::Mask::SSIP) {
    cregs->write_bit(csr::Address::MIP, csr::Mask::SEIP_BIT, 0);
    return Interrupt::SupervisorExternalInterrupt;
  } else if (pending & csr::Mask::STIP) {
    cregs->write_bit(csr::Address::MIP, csr::Mask::STIP_BIT, 0);
    return Interrupt::SupervisorTimerInterrupt;
  }

  return std::nullopt;
}

void Cpu::dump_registers(std::ostream &stream) {
  stream << "Registers:\n\n";

  for (int i = 0; i < 32; i++) {
    stream << std::format("x{}: 0x{:0>8x}\n", i, xregs->read(i));
  }
}

void Cpu::start() {
  while (1) {
    oneTick();
  }
}

void Cpu::oneTick(std::stringstream *debug_str) {

  // Tick time
  cregs->write(csr::Address::CYCLE, cregs->load(csr::Address::CYCLE) + 1);

  // Take an interrupt
  auto interrupt = this->check_pending_interrupt();
  if (interrupt.has_value()) {
    state.cause = interrupt.value();
    state.take_trap(this);
  }

  Trap trap;

  if (const auto exc_val = this->execute(debug_str); exc_val.has_value()) {
    trap = Trap::Requested;
  } else {
    exception.exception = exc_val.error();
    trap = exception.take_trap(this);
  }
  // if (trap != Trap::Requested)
  //   std::cin.ignore(10, '\n');
  if (trap == Trap::Fatal) {
    if (debug_str)
      *debug_str << "Fatal trap";
    return;
  }

  //  exception.exception = Exception::None;
}
Cpu::ExecResult Cpu::execute(std::stringstream *ss) {
  uint64_t inst = this->fetch();
  auto result = this->executeGeneral(inst, ss);
  prev_inst = inst;
  if (!result.has_value()) {
    if (exception.exception == Exception::IllegalInstruction) {
      std::println(*ss, "IllegalInstruction: 0x{:X}", inst);
      std::println("IllegalInstruction: 0x{:X}", inst);
    }
    return result;
  }
  this->pc += 4;

  return result;
}

Cpu::ExecResult Cpu::executeGeneral(uint64_t inst, std::stringstream *ss) {
  uint64_t opcode = OPCODE(inst);
  ExecResult res;

  switch (opcode) {
  case 0x1B:
    res = exec_OP_IMM_32(inst, ss);
    break;
  case 0x13:
    res = exec_OP_IMM(inst, ss);
    break;
  case 0x3B:
    res = exec_OP_32(inst, ss);
    break;
  case 0x33:
    res = exec_OP(inst, ss);
    break;
  case 0x37:
    res = exec_LUI(inst, ss);
    break;
  case 0x17:
    res = exec_AUIPC(inst, ss);
    break;
  case 0x03:
    res = exec_LOAD(inst, ss);
    break;
  case 0x23:
    res = exec_STORE(inst, ss);
    break;
  case 0x6F:
    res = exec_JAL(inst, ss);
    break;
  case 0x67:
    res = exec_JALR(inst, ss);
    break;
  case 0x63:
    res = exec_BRANCH(inst, ss);
    break;
  case 0x73:
    res = exec_SYSTEM(inst, ss);
    break;
  case 0x0F:
    res = exec_FENCE(inst, ss);
    break;
  case 0x2F:
    res = exec_ATOMIC(inst, ss);
    break;
  default:
    return std::unexpected(Exception::IllegalInstruction);
  }

  if (!res.has_value()) {
    return std::unexpected(res.error());
  }

  return {};
}
