#include "cpu.h"
#include "csrs.h"
#include "exception.h"
#include <cstdint>
#include <sys/types.h>

#ifdef DEBUG
// ---- Bitfield extractors ----
#define OPCODE(inst) ((inst) & 0x7F)
#define RD(inst) (((inst) >> 7) & 0x1F)
#define FUNCT3(inst) (((inst) >> 12) & 0x7)
#define RS1(inst) (((inst) >> 15) & 0x1F)
#define RS2(inst) (((inst) >> 20) & 0x1F)
#define FUNCT7(inst) (((inst) >> 25) & 0x7F)

// ---- Immediates for different types ----
#define IMM_I(inst) ((int64_t)(inst) >> 20)
#define IMM_S(inst)                                                            \
  ((int64_t)(((inst) & 0xFE000000) >> 20) | (((inst) >> 7) & 0x1F))
#define IMM_B(inst)                                                            \
  ((int64_t)(((inst) >> 31) << 12) | (((inst) >> 7) & 0x1E) |                  \
   (((inst) >> 20) & 0x7E0) | (((inst) >> 20) & 0x800 ? 0xFFFFF000 : 0))
#define IMM_U(inst) ((int64_t)(inst) & 0xFFFFF000)
#define IMM_J(inst)                                                            \
  ((int64_t)(((inst) >> 31 ? 0xFFF00000 : 0) | (((inst) >> 12) & 0xFF) << 12 | \
             ((inst) >> 20 & 1) << 11 | ((inst) >> 21 & 0x3FF) << 1 |          \
             ((inst) >> 31) << 20))

#define DB(ss, inst, name)                                                     \
  do {                                                                         \
    if (ss) {                                                                  \
      *ss << std::hex << "[DEBUG] " << (name) << " Instruction 0x" << (inst)   \
          << std::dec << "\n"                                                  \
          << "  opcode: 0x" << std::hex << OPCODE(inst) << std::dec            \
          << "  rd: x" << RD(inst) << "  rs1: x" << RS1(inst) << "  rs2: x"    \
          << RS2(inst) << "\n"                                                 \
          << "  funct3: " << FUNCT3(inst) << "  funct7: 0x" << std::hex        \
          << FUNCT7(inst) << std::dec << "\n"                                  \
          << "  imm_I: " << IMM_I(inst) << "  imm_S: " << IMM_S(inst)          \
          << "  imm_B: " << IMM_B(inst) << "  imm_U: 0x" << std::hex           \
          << IMM_U(inst) << std::dec << "  imm_J: " << IMM_J(inst) << "\n"     \
          << "PC: " << std::hex << this->pc << "\n";                           \
    }                                                                          \
  } while (0)

#else
#define DB(ss, inst, x) ((void)0)
#endif // DEBUG

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
  cregs->store(csr::Address::CYCLE, cregs->load(csr::Address::CYCLE) + 1);

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
std::expected<uint64_t, Exception::ExceptionValue>
Cpu::execute(std::stringstream *ss) {
  uint64_t inst = this->fetch();
  auto result = this->executeGeneral(inst, ss);
  prev_inst = inst;
  if (!result) {
    // DB(ss, inst, "unexpected");
    return result;
  }
  this->pc += 4;

  return result;
}

std::expected<uint64_t, Exception::ExceptionValue>
Cpu::executeGeneral(uint64_t inst, std::stringstream *ss) {

  uint64_t opcode = inst & 0x7f;
  uint64_t rd = (inst >> 7) & 0x1F;
  uint64_t rs1 = (inst >> 15) & 0x1F;
  uint64_t rs2 = (inst >> 20) & 0x1F;
  uint64_t csrAddr = (inst >> 20) & 0xFFF;
  uint64_t funct3 = (inst >> 12) & 0x7;
  uint64_t funct7 = (inst >> 25) & 0x7F;

  switch (opcode) {
  case 0x1B: { // Integer register-immediate w-versions
    int64_t imm = ((int64_t)inst >> 20);
    uint64_t reg1 = this->xregs->read(rs1);
    uint64_t result;
    switch (funct3) {
    case 0x0: { // addiw
      DB(ss, inst, "addiw");
      result = SINGEXTEND_CAST(reg1 + imm, int32_t);
      break;
    }
    case 0x1: { // slliw
      DB(ss, inst, "slliw");
      result = SINGEXTEND_CAST(reg1 << (imm & 0x1F), int32_t);
      break;
    }
    case 0x5: {             // srliw or sraiw
      if (funct7 == 0x20) { // sraiw
        DB(ss, inst, "sraiw");
        result = SINGEXTEND_CAST(
            static_cast<int32_t>(reg1) >> (uint32_t)(imm & 0x1F), int64_t);
      } else if (funct7 == 0) {
        DB(ss, inst, "srliw");
        result = SINGEXTEND_CAST(
            static_cast<uint32_t>(reg1) >> (uint32_t)(imm & 0x1F), int32_t);
      } else {
        return std::unexpected(Exception::IllegalInstruction);
      }
      break;
    }
    default:
      return std::unexpected(Exception::IllegalInstruction);
    }

    this->xregs->write(rd, result);
    break;
  }
  case 0x13: { // Integer register-immediate
    uint64_t imm = ((int64_t)(int32_t)inst >> 20);
    uint64_t reg1 = this->xregs->read(rs1);
    uint64_t result;
    switch (funct3) {
    case 0x0: { // addi
      DB(ss, inst, "addi");
      result = (int64_t)(reg1) + imm;
      break;
    }
    case 0x2: { // slti
      DB(ss, inst, "slti");
      result = ((int64_t)reg1 < (int64_t)imm);
      break;
    }
    case 0x3: { // sltiu
      DB(ss, inst, "sltiu");
      result = (reg1 < (uint64_t)imm);
      break;
    }
    case 0x4: { // xori
      DB(ss, inst, "XORI");
      result = reg1 ^ imm;
      break;
    }
    case 0x6: { // ori
      DB(ss, inst, "ori");
      result = reg1 | imm;
      break;
    }
    case 0x7: { // andi
      DB(ss, inst, "andi");
      result = reg1 & imm;
      break;
    }
    case 0x1: { // slli
      DB(ss, inst, "slli");
      result = reg1 << (imm & 0x3F);
      break;
    }
    case 0x5: { // srli or srai
      if (funct7 == 0x20) {
        DB(ss, inst, "srai");
        result = (int64_t)reg1 >> (imm & 0x3F);
      } else if (funct7 == 0) {
        DB(ss, inst, "srli");
        result = reg1 >> (imm & 0x1F);
      } else {
        return std::unexpected(Exception::IllegalInstruction);
      }
      break;
    }
    default:
      return std::unexpected(Exception::IllegalInstruction);
    }
    this->xregs->write(rd, result);
    break;
  }
  case 0x3B: { // Integer register-register w-versions and RV64M
    uint64_t result;
    uint64_t reg1 = this->xregs->read(rs1);
    uint64_t reg2 = this->xregs->read(rs2);
    if (funct7 & 0x1) { // Multiplication
      switch (funct3) {
      case 0x0: { // mulw
        DB(ss, inst, "mulw");
        result = SINGEXTEND_CAST(reg1 * (int64_t)reg2, int32_t);
        break;
      }
      case 0x4: { // divw
        DB(ss, inst, "divw");
        if (reg2 == 0) {
          result = ~0ULL;
        } else if ((int32_t)reg1 == INT32_MIN && (int32_t)reg2 == -1) {
          result = INT32_MIN;
        } else {
          result = SINGEXTEND_CAST((int32_t)reg1 / (int32_t)reg2, int64_t);
        }
        break;
      }
      case 0x5: { // divuw
        DB(ss, inst, "divuw");
        if (reg2 == 0) {
          result = ~0ULL;
        } else {
          result = SINGEXTEND_CAST((uint32_t)reg1 / (uint32_t)reg2, int32_t);
        }
        break;
      }
      case 0x6: { // remw
        DB(ss, inst, "remw");
        if (reg2 == 0) {
          result = SINGEXTEND_CAST((int32_t)reg1, int32_t);
        } else {
          result = SINGEXTEND_CAST(
              (int64_t)static_cast<int32_t>(reg1) % (int64_t)reg2, int32_t);
        }
        break;
      }
      case 0x7: { // remuw
        DB(ss, inst, "remuw");
        if (reg2 == 0) {
          result = SINGEXTEND_CAST((int32_t)reg1, int32_t);
        } else {
          result = reg1 % (int64_t)reg2;
        }
        break;
      }
      }
    } else { // int register-register
      switch (funct3) {
      case 0x0: {          // addw or subw
        if (funct7 == 0) { // addw
          DB(ss, inst, "addw");
          result = SINGEXTEND_CAST(reg1 + (int64_t)reg2, int32_t);
        } else if (funct7 == 0x20) { // subw
          DB(ss, inst, "subw");
          result = SINGEXTEND_CAST(reg1 - (int64_t)reg2, int32_t);
        } else {
          return std::unexpected(Exception::IllegalInstruction);
        }
        break;
      }
      case 0x1: { // sllw
        DB(ss, inst, "sllw");
        result = SINGEXTEND_CAST(reg1 << (reg2 & 0x1F), int32_t);
        break;
      }
      case 0x5: {          // srlw or sraw
        if (funct7 == 0) { // srlw
          DB(ss, inst, "srlw");
          result = SINGEXTEND_CAST((uint32_t)reg1 >> (reg2 & 0x1F), int32_t);
        } else if (funct7 == 0x20) { // sraw
          DB(ss, inst, "sraw");
          result = SINGEXTEND_CAST((int32_t)reg1 >> (reg2 & 0x1F), int32_t);
        } else {
          return std::unexpected(Exception::IllegalInstruction);
        }
        break;
      }
      default:
        return std::unexpected(Exception::IllegalInstruction);
      }
    }
    this->xregs->write(rd, result);
    break;
  }
  case 0x33: { // Integer register-register + integer multi
    uint64_t result;
    uint64_t reg1 = this->xregs->read(rs1);
    uint64_t reg2 = this->xregs->read(rs2);
    if (funct7 & 0x1) { // Multiplication
      switch (funct3) {
      case 0x00: { // mul
        DB(ss, inst, "mul");
        result = (int64_t)((int64_t)reg1 * (int64_t)reg2);
        break;
      }
      case 0x1: { // mulh
        DB(ss, inst, "mulh");
        __int128_t val1 = static_cast<int64_t>(reg1);
        __int128_t val2 = static_cast<int64_t>(reg2);
        result = (val1 * val2) >> 64;
        break;
      }
      case 0x2: { // mulhsu
        DB(ss, inst, "mulhsu");
        __uint128_t val1 = static_cast<__int128_t>(static_cast<int64_t>(reg1));
        __uint128_t val2 = reg2;
        result = (val1 * val2) >> 64;

        break;
      }
      case 0x3: { // mulhu
        DB(ss, inst, "mulhu");
        result = ((__uint128_t)reg1 * (__uint128_t)reg2) >> 64;
        break;
      }
      case 0x4: { // div
        DB(ss, inst, "div");
        if (reg2 == 0) {
          result = -1;
          break;
        } else if ((int64_t)reg1 == INT64_MIN && (int64_t)reg2 == -1) {
          result = INT64_MIN;

        } else {
          result = (int64_t)((int64_t)reg1 / (int64_t)reg2);
        }
        break;
      }
      case 0x5: { // divu
        DB(ss, inst, "divu");
        if (reg2 == 0) {
          result = UINT64_MAX;
        } else {
          result = reg1 / reg2;
        }
        break;
      }
      case 0x6: { // rem
        DB(ss, inst, "rem");
        if (reg2 == 0) {
          result = reg1;
        } else if ((int64_t)reg1 == INT64_MIN && (int64_t)reg2 == -1) {
          result = 0;
        } else {
          result = (int64_t)reg1 % (int64_t)reg2;
        }
        break;
      }
      case 0x7: { // remu
        DB(ss, inst, "remu");
        if (reg2 == 0) {
          result = reg1;
        } else {
          result = reg1 % reg2;
        }
        break;
      }
      default:
        return std::unexpected(Exception::IllegalInstruction);
      }
    } else { // non-multiplication
      switch (funct3) {
      case 0x0: {
        if (funct7 == 0) { // add
          DB(ss, inst, "add");
          result = (int64_t)(reg1 + reg2);
        } else if (funct7) { // sub
          DB(ss, inst, "sub");
          result = (int64_t)(reg1 - reg2);
        } else {
          return std::unexpected(Exception::IllegalInstruction);
        }
        break;
      }
      case 0x1: { // sll
        DB(ss, inst, "sll");
        result = reg1 << (reg2 & 0x3f);
        break;
      }
      case 0x2: { // slt
        DB(ss, inst, "sll");
        result = ((int64_t)reg1 < (int64_t)reg2) ? 1 : 0;
        break;
      }
      case 0x3: { // sltu
        DB(ss, inst, "sll");
        result = (reg1 < reg2) ? 1 : 0;
        break;
      }
      case 0x4: { // xor
        DB(ss, inst, "xor");
        result = reg1 ^ reg2;
        break;
      }
      case 0x5: {
        if (funct7 == 0) { // srl
          DB(ss, inst, "srl");
          result = reg1 >> reg2;
        } else if (funct7 == 0x20) { // sra
          DB(ss, inst, "sra");
          result = (int32_t)reg1 >> (int64_t)(reg2);
        } else {
          return std::unexpected(Exception::IllegalInstruction);
        }
        break;
      }
      case 0x6: { // or
        DB(ss, inst, "or");
        result = reg1 | reg2;
        break;
      }
      case 0x7: { // and
        DB(ss, inst, "or");
        result = reg1 & reg2;
        break;
      }
      default:
        return std::unexpected(Exception::IllegalInstruction);
      }
    }
    // Finally write the result
    this->xregs->write(rd, result);
    break;
  }
  case 0x37: { // lui
    DB(ss, inst, "lui");
    uint64_t imm = (int64_t)(int32_t)(inst & 0xfffff000);
    this->xregs->write(rd, imm);
    break;
  }
  case 0x17: { // auipc
    DB(ss, inst, "auipc");
    uint64_t imm =
        (uint64_t)this->pc + (int64_t)(int64_t)(int32_t)(inst & 0xfffff000);
    this->xregs->write(rd, imm);
    break;
  }
  // TODO: Add exceptions for load operations
  case 0x3: { // load operations
    uint64_t reg1 = this->xregs->read(rs1);
    int64_t imm = ((int64_t)inst >> 20);
    switch (funct3) {
    case 0x0: { // lb
      DB(ss, inst, "lb");
      uint64_t addr = uint64_t(reg1 + imm);
      this->xregs->write(rd, (int64_t)(int8_t)this->bus->read(addr, BYTE));
      break;
    }
    case 0x1: { // lh
      DB(ss, inst, "lh");
      uint64_t addr = uint64_t(reg1 + imm);
      this->xregs->write(rd, (int64_t)(int16_t)this->bus->read(addr, HALFWORD));
      break;
    }
    case 0x2: { // lw
      DB(ss, inst, "lw");
      uint64_t addr = uint64_t(reg1 + imm);
      this->xregs->write(rd, (int64_t)(int32_t)this->bus->read(addr, WORD));
      break;
    }
    case 0x4: { // lbu
      DB(ss, inst, "lbu");
      uint64_t addr = uint64_t(reg1 + imm);
      this->xregs->write(rd, this->bus->read(addr, BYTE));
      break;
    }
    case 0x5: { // lhu
      DB(ss, inst, "lhu");
      uint64_t addr = uint64_t(reg1 + imm);
      this->xregs->write(rd, this->bus->read(addr, HALFWORD));
      break;
    }
    case 0x6: { // lwu
      DB(ss, inst, "lwu");
      uint64_t addr = uint64_t(reg1 + imm);
      this->xregs->write(rd, (this->bus->read(addr, WORD) & 0xFFFFFFFF));
      break;
    }
    case 0x3: { // ld
      DB(ss, inst, "ld");
      uint64_t addr = uint64_t(reg1 + imm);
      this->xregs->write(rd, this->bus->read(addr, DOUBLEWORD));
      break;
    }
    default:
      return std::unexpected(Exception::IllegalInstruction);
    }
    break;
  }
  // TODO: Add exceptions for store operations
  case 0x23: { // store operations
    int64_t imm = rd | ((((int64_t)inst) >> 25) << 5);
    uint64_t reg1 = this->xregs->read(rs1);
    uint64_t reg2 = this->xregs->read(rs2);
    switch (funct3) {
    case 0x0: { // sb
      DB(ss, inst, "sb");
      uint64_t dest = uint64_t(reg1 + imm);
      this->bus->write(dest, BYTE, reg2 & 0xff);
      break;
    }
    case 0x1: { // sh
      DB(ss, inst, "sh");
      uint64_t dest = uint64_t(reg1 + imm);
      this->bus->write(dest, HALFWORD, reg2 & 0xffff);
      break;
    }
    case 0x2: { // sw
      DB(ss, inst, "sw");
      uint64_t dest = uint64_t(reg1 + imm);
      this->bus->write(dest, WORD, reg2);
      break;
    }
    case 0x3: { // sd
      DB(ss, inst, "sd");
      uint64_t dest = uint64_t(reg1 + imm);
      this->bus->write(dest, DOUBLEWORD, reg2);
      break;
    }
    default:
      return std::unexpected(Exception::IllegalInstruction);
    }
    break;
  }
  case 0x6F: { // jal
    DB(ss, inst, "jal");
    int64_t offset =
        (((inst >> 21) & 0x3FF) | (((inst >> 20) & 0x1) << 10) |
         (((inst >> 12) & 0xFF) << 11) | ((((int64_t)inst) >> 31)) << 19)
        << 1;
    this->xregs->write(rd, this->pc + 4);
    // TODO: Handle misalinged jumps
    this->pc += offset - 4;
    break;
  }
  case 0x67: { // jalr
    DB(ss, inst, "jalr");
    int64_t imm = ((int64_t)inst >> 20);
    uint64_t addr = (((this->xregs->read(rs1) + imm) >> 1) << 1);
    this->xregs->write(rd, this->pc + 4);
    // TODO: Handle misalinged jumps
    this->pc = addr - 4;
    break;
  }
  // TODO: Handle misalinged jumps
  case 0x63: { // branches
    int64_t imm =
        (((inst >> 8) & 0xF) | (((inst >> 25) & 0x3F) << 4) |
         (((inst >> 7) & 0x1) << 10) | ((((int64_t)inst) >> 31)) << 11)
        << 1;
    uint64_t reg1 = this->xregs->read(rs1);
    uint64_t reg2 = this->xregs->read(rs2);
    switch (funct3) {
    case 0x0: { // beq
      DB(ss, inst, "beq");
      if (reg1 == reg2)
        this->pc += imm - 4;
      break;
    }
    case 0x1: { // bne
      DB(ss, inst, "bne");
      if (reg1 != reg2)
        this->pc += imm - 4;
      break;
    }
    case 0x4: { // blt
      DB(ss, inst, "blt");
      if ((int64_t)reg1 < (int64_t)reg2)
        this->pc += imm - 4;
      break;
    }
    case 0x5: { // bge
      DB(ss, inst, "bge");
      if ((int64_t)reg1 >= (int64_t)reg2)
        this->pc += imm - 4;
      break;
    }
    case 0x6: { // bltu
      DB(ss, inst, "bltu");
      if (reg1 < reg2)
        this->pc += imm - 4;
      break;
    }
    case 0x7: { // bgeu
      DB(ss, inst, "bgeu");
      if (reg1 >= reg2)
        this->pc += imm - 4;
      break;
    }
    default:
      return std::unexpected(Exception::IllegalInstruction);
    }
    break;
  } // TODO: cssrw instructions
  case 0x73: {
    switch (funct3) {
    case 0x0: {              // ecall ebreak sret mret
      if (inst >> 20 == 1) { // ebreak
        DB(ss, inst, "ebreak");
        return std::unexpected(Exception::Breakpoint);
      } else if (inst >> 20 == 0) { // ecall
        DB(ss, inst, "ecall");
        switch (mode) {
        case Mode::MACHINE:
          return std::unexpected(Exception::EnvironmentCallMmode);
        case Mode::SUPERVISOR:
          return std::unexpected(Exception::EnvironmentCallSmode);
        case Mode::USER:
          return std::unexpected(Exception::EnvironmentCallUmode);
        }

      } else if (funct7 == 0x18) { // mret
        DB(ss, inst, "mret");
        if (this->mode != Mode::MACHINE)
          return std::unexpected(Exception::IllegalInstruction);

        // Set the program counter to MEPC
        this->pc = cregs->load(csr::Address::MEPC) - 4;

        if (cregs->read_bit_mstatus(csr::Mask::MSTATUSBit::MPP) == 0b00) {
          // Set mode to User and clear MPRV;
          cregs->write_bit_mstatus(csr::Mask::MSTATUSBit::MPRV, 0);
          this->mode = Mode::USER;
        } else if (cregs->read_bit_mstatus(csr::Mask::MSTATUSBit::MPP) ==
                   0b01) {
          // Set mode to supervisor and clear MPRV
          cregs->write_bit_mstatus(csr::Mask::MSTATUSBit::MPRV, 0);
          this->mode = Mode::SUPERVISOR;
        } else if (cregs->read_bit_mstatus(csr::Mask::MSTATUSBit::MPP) ==
                   0b11) {
          // Set mode to machine but don't clear MPRV
          this->mode = Mode::MACHINE;
        }

        cregs->write_bit_mstatus(
            csr::Mask::MSTATUSBit::MIE,
            cregs->read_bit_mstatus(csr::Mask::MSTATUSBit::MPIE));

        cregs->write_bit_mstatus(csr::Mask::MSTATUSBit::MPIE, 1);

        cregs->write_bit_mstatus(csr::Mask::MSTATUSBit::MPP, Mode::USER);

      } else {
        return std::unexpected(Exception::IllegalInstruction);
      }
      break;
    }
    case 0x1: { // csrrw
      DB(ss, inst, "csrrw");
      uint64_t initialValue = xregs->read(rs1);
      if (rd != 0) {
        xregs->write(rd, cregs->load(csrAddr));
      }
      cregs->store(csrAddr, initialValue);
      break;
    }
    case 0x2: { // csrrs
      DB(ss, inst, "csrrs");
      uint64_t initialCsrValue = cregs->load(csrAddr);
      if (rs1 != 0) {
        uint64_t initialValue = xregs->read(rs1);
        cregs->store(csrAddr, initialCsrValue | initialValue);
      }
      xregs->write(rd, initialCsrValue);
      break;
    }
    case 0x3: { // csrrc
      DB(ss, inst, "csrrc");
      uint64_t initialCsrValue = cregs->load(csrAddr);
      if (rs1 != 0) {
        uint64_t initialValue = xregs->read(rs1);
        cregs->store(csrAddr, initialCsrValue & ~initialValue);
      }
      xregs->write(rd, initialCsrValue);
      break;
    }
    case 0x5: { // csrrwi
      DB(ss, inst, "csrrwi");
      if (rd != 0) {
        xregs->write(rd, cregs->load(csrAddr));
      }
      cregs->store(csrAddr, rs1);
      break;
    }
    case 0x6: { // csrrsi
      DB(ss, inst, "csrrsi");
      uint64_t initialCsrValue = cregs->load(csrAddr);
      if (rs1 != 0) {
        cregs->store(csrAddr, initialCsrValue | rs1);
      }
      xregs->write(rd, initialCsrValue);
      break;
    }
    case 0x7: { // csrrci
      DB(ss, inst, "csrrci");
      uint64_t initialCsrValue = cregs->load(csrAddr);
      if (rs1 != 0) {
        cregs->store(csrAddr, initialCsrValue & ~rs1);
      }
      xregs->write(rd, initialCsrValue);
      break;
    }

    default:
      return std::unexpected(Exception::IllegalInstruction);
    }
    break;
  }
  case 0x0f: { // fence fence.tso fence.i
    // All writes are sequential and single threaded so
    // fences don't do anything
    if (funct3 == 1) { // fence.i
      DB(ss, inst, "fence.i");
    } else if (funct3 == 0) { // fence fence.tso
      DB(ss, inst, "fence");
    } else {
      return std::unexpected(Exception::IllegalInstruction);
    }
    break;
  }
  default:
    return std::unexpected(Exception::IllegalInstruction);
  }
  return inst;
}
