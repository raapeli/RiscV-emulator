#include "cpu.h"
#include "exception.h"
#include <print>

void XRegisters::write(uint64_t dest, uint64_t value) {
  if (dest != 0) {
    this->registers[dest] = value;
  }
}

uint64_t XRegisters::read(uint64_t reg) { return this->registers[reg]; }

Cpu::Cpu(Bus *bus) : pc(bus->ram_base) {
  this->xregs = new XRegisters();
  this->bus = bus;
  this->cregs = new csr::Csr();
}

std::optional<Interrupt::InterruptValue> Cpu::check_pending_interrupt() {
  uint64_t pending = cregs->load(csr::Address::MIE) &
                     cregs->load(csr::Address::MIP) &
                     csr::Mask::SUPPORTED_INTERRUPTS;
  uint64_t delegated = cregs->load(csr::Address::MIDELEG);
  uint64_t machine_pending = pending & ~delegated;
  bool machine_enabled = mode < Mode::MACHINE ||
                         cregs->read_bit_mstatus(csr::Mask::MSTATUSBit::MIE);
  bool supervisor_enabled =
      mode < Mode::SUPERVISOR ||
      (mode == Mode::SUPERVISOR &&
       cregs->read_bit_sstatus(csr::Mask::SSTATUSBit::SIE));

  if (machine_enabled && machine_pending)
    pending = machine_pending;
  else
    pending = supervisor_enabled ? pending & delegated : 0;

  if (pending & csr::Mask::MEIP)
    return Interrupt::MachineExternalInterrupt;
  if (pending & csr::Mask::MSIP)
    return Interrupt::MachineSoftwareInterrupt;
  if (pending & csr::Mask::MTIP)
    return Interrupt::MachineTimerInterrupt;
  if (pending & csr::Mask::SEIP)
    return Interrupt::SupervisorExternalInterrupt;
  if (pending & csr::Mask::SSIP)
    return Interrupt::SupervisorSoftwareInterrupt;
  if (pending & csr::Mask::STIP)
    return Interrupt::SupervisorTimerInterrupt;

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
    exception.exception = Exception::None;
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
}
Cpu::ExecResult Cpu::execute(std::stringstream *ss) {
  exception.access_address = 0;
  prev_inst = 0;
  if (pc & 1)
    return std::unexpected(Exception::InstructionAddressMisaligned);

  auto low = resolve_memory_access(pc, HALFWORD, csr::Csr::PMP_EXECUTE);
  if (!low)
    return std::unexpected(low.error());

  uint32_t inst = bus->read(*low, HALFWORD);
  const bool compressed = (inst & 3) != 3;
  if (!compressed) {
    // A word starting on a halfword boundary can span two virtual pages.
    const bool split = (pc & 2) != 0;
    auto next = resolve_memory_access(
        split ? pc + 2 : pc, split ? HALFWORD : WORD, csr::Csr::PMP_EXECUTE);
    if (!next)
      return std::unexpected(next.error());
    if (split)
      inst |= bus->read(*next, HALFWORD) << 16;
    else
      inst = bus->read(*next, WORD);
  }

  prev_inst = inst;
  next_pc = pc + (compressed ? 2 : 4);
  auto decoded = compressed ? expand_compressed(inst) : std::optional{inst};

  ExecResult result = std::unexpected(Exception::IllegalInstruction);
  if (decoded)
    result = executeGeneral(*decoded, ss);
  if (!result.has_value()) {
    if (result.error() == Exception::IllegalInstruction) {
      if (ss)
        std::println(*ss, "IllegalInstruction: 0x{:X}", inst);
      std::println("IllegalInstruction: 0x{:X}", inst);
    }
    return result;
  }
  pc = next_pc;

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
  case 0x07:
    res = exec_FLOAT_LOAD(inst, ss);
    break;
  case 0x23:
    res = exec_STORE(inst, ss);
    break;
  case 0x27:
    res = exec_FLOAT_STORE(inst, ss);
    break;
  case 0x53:
    res = exec_FLOAT(inst, ss);
    break;
  case 0x43:
  case 0x47:
  case 0x4b:
  case 0x4f:
    res = exec_FLOAT_FUSED(inst, ss);
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
