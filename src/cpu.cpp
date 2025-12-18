#include "cpu.h"
#include <iostream>

#ifdef DEBUG
// ---- Bitfield extractors ----
#define OPCODE(inst) ((inst) & 0x7F)
#define RD(inst) (((inst) >> 7) & 0x1F)
#define FUNCT3(inst) (((inst) >> 12) & 0x7)
#define RS1(inst) (((inst) >> 15) & 0x1F)
#define RS2(inst) (((inst) >> 20) & 0x1F)
#define FUNCT7(inst) (((inst) >> 25) & 0x7F)

// ---- Immediates for different types ----
#define IMM_I(inst) ((int32_t)(inst) >> 20)
#define IMM_S(inst)                                                            \
  ((int32_t)(((inst) & 0xFE000000) >> 20) | (((inst) >> 7) & 0x1F))
#define IMM_B(inst)                                                            \
  ((int32_t)(((inst) >> 31) << 12) | (((inst) >> 7) & 0x1E) |                  \
   (((inst) >> 20) & 0x7E0) | (((inst) >> 20) & 0x800 ? 0xFFFFF000 : 0))
#define IMM_U(inst) ((int32_t)(inst) & 0xFFFFF000)
#define IMM_J(inst)                                                            \
  ((int32_t)(((inst) >> 31 ? 0xFFF00000 : 0) | (((inst) >> 12) & 0xFF) << 12 | \
             ((inst) >> 20 & 1) << 11 | ((inst) >> 21 & 0x3FF) << 1 |          \
             ((inst) >> 31) << 20))

#define DB(inst, name)                                                         \
  do {                                                                         \
    std::cout << std::hex << "[DEBUG] " << (name) << " Instruction 0x"         \
              << (inst) << std::dec << "\n"                                    \
              << "  opcode: 0x" << std::hex << OPCODE(inst) << std::dec        \
              << "  rd: x" << RD(inst) << "  rs1: x" << RS1(inst)              \
              << "  rs2: x" << RS2(inst) << "\n"                               \
              << "  funct3: " << FUNCT3(inst) << "  funct7: 0x" << std::hex    \
              << FUNCT7(inst) << std::dec << "\n"                              \
              << "  imm_I: " << IMM_I(inst) << "  imm_S: " << IMM_S(inst)      \
              << "  imm_B: " << IMM_B(inst) << "  imm_U: 0x" << std::hex       \
              << IMM_U(inst) << std::dec << "  imm_J: " << IMM_J(inst)         \
              << "\n";                                                         \
  } while (0)

#else
#define DB(inst, x) ((void)0)
#endif // DEBUG

void XRegisters::write(uint32_t dest, uint32_t value) {
  if (dest != 0) {
    this->registers[dest] = value;
  }
}

uint32_t XRegisters::read(uint32_t reg) { return this->registers[reg]; }

Cpu::Cpu(XRegisters *xregs, Bus *bus) {
  this->xregs = xregs;
  this->bus = bus;
}

uint32_t Cpu::fetch() { return bus->read(pc, WORD); }

std::expected<uint32_t, Exception> Cpu::execute() {
  uint32_t inst = this->fetch();
  auto result = this->executeGeneral(inst);
  if (!result)
    return result;
  this->pc += 4;

  return result;
}

std::expected<uint32_t, Exception> Cpu::executeGeneral(uint32_t inst) {

  uint32_t opcode = inst & 0x7f;
  uint32_t rd = (inst >> 7) & 0x1F;
  uint32_t rs1 = (inst >> 15) & 0x1F;
  uint32_t rs2 = (inst >> 20) & 0x1F;
  uint32_t funct3 = (inst >> 12) & 0x7;
  uint32_t funct7 = (inst >> 25) & 0x7F;

  switch (opcode) {
  case 0x13: { // Integer register-immediate
    int32_t imm = ((int32_t)inst >> 20);
    uint32_t reg1 = this->xregs->read(rs1);
    uint32_t result;
    switch (funct3) {
    case 0x0: { // addi
      DB(inst, "addi");
      result = (int32_t)(reg1) + imm;
      break;
    }
    case 0x2: { // slti
      DB(inst, "slti");
      result = ((int32_t)reg1 < imm);
      break;
    }
    case 0x3: { // sltiu
      DB(inst, "sltiu");
      result = (reg1 < (uint32_t)imm);
      break;
    }
    case 0x4: { // xori
      DB(inst, "XORI");
      result = reg1 ^ imm;
      break;
    }
    case 0x6: { // ori
      DB(inst, "ori");
      result = reg1 | imm;
      break;
    }
    case 0x7: { // andi
      DB(inst, "andi");
      result = reg1 & imm;
      break;
    }
    case 0x1: { // slli
      DB(inst, "slli");
      result = reg1 << (imm & 0x3F);
      break;
    }
    case 0x5: { // srli or srai
      if (funct7 == 0x20) {
        DB(inst, "srai");
        result = reg1 >> (uint32_t)(imm & 0x3f);
      } else if (funct7 == 0) {
        DB(inst, "srli");
        result = (int32_t)reg1 >> (int32_t)(imm & 0x3F);
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
  case 0x33: { // Integer register-register + integer multi
    uint32_t result;
    uint32_t reg1 = this->xregs->read(rs1);
    uint32_t reg2 = this->xregs->read(rs2);
    if (funct7 & 0x1) { // Multiplication
      switch (funct3) {
      case 0x00: { // mul
        DB(inst, "mul");
        result = (int32_t)((int32_t)reg1 * (int32_t)reg2);
        break;
      }
      case 0x1: { // mulh
        DB(inst, "mulh");
        result =
            (int32_t)(((uint64_t)(int32_t)reg1 * (uint64_t)(int32_t)reg2) >>
                      32);
        break;
      }
      case 0x2: { // mulhsu
        DB(inst, "mulhsu");
        result = (int32_t)(((uint64_t)(int32_t)reg1 * (uint64_t)reg2) >> 32);
        break;
      }
      case 0x3: { // mulhu
        DB(inst, "mulhu");
        result = (int32_t)((uint64_t)reg1 * (uint64_t)reg2 >> 32);
        break;
      }
      case 0x4: { // div
        DB(inst, "div");
        if (reg2 == 0) {
          result = -1;
          break;
        } else if ((int32_t)reg1 == INT32_MIN && (int32_t)reg2 == -1) {
          result = INT32_MIN;

        } else {
          result = (int32_t)((int32_t)reg1 / (int32_t)reg2);
        }
        break;
      }
      case 0x5: { // divu
        DB(inst, "divu");
        if (reg2 == 0) {
          result = UINT32_MAX;
        } else {
          result = reg1 / reg2;
        }
        break;
      }
      case 0x6: { // rem
        DB(inst, "rem");
        if (reg2 == 0) {
          result = reg1;
        } else if ((int32_t)reg1 == INT32_MIN && (int32_t)reg2 == -1) {
          result = 0;
        } else {
          result = (int32_t)reg1 % (int32_t)reg2;
        }
        break;
      }
      case 0x7: { // remu
        DB(inst, "remu");
        result = reg1 % reg2;
        break;
      }
      default:
        return std::unexpected(Exception::IllegalInstruction);
      }
    } else { // non-multiplication
      switch (funct3) {
      case 0x0: {
        // TODO: Add checks for illegal instructions
        if (funct7 == 0) { // add
          DB(inst, "add");
          result = (int32_t)(reg1 + reg2);
        } else if (funct7) { // sub
          DB(inst, "sub");
          result = (int32_t)(reg1 - reg2);
        } else {
          return std::unexpected(Exception::IllegalInstruction);
        }
        break;
      }
      case 0x1: { // sll
        DB(inst, "sll");
        result = reg1 << (reg2 & 0x3f);
        break;
      }
      case 0x2: { // slt
        DB(inst, "sll");
        result = ((int32_t)reg1 < (int32_t)reg2) ? 1 : 0;
        break;
      }
      case 0x3: { // sltu
        DB(inst, "sll");
        result = (reg1 < reg2) ? 1 : 0;
        break;
      }
      case 0x4: { // xor
        DB(inst, "xor");
        result = reg1 ^ reg2;
        break;
      }
      case 0x5: {
        if (funct7 == 0) { // srl
          DB(inst, "srl");
          result = (int32_t)reg1 >> (int32_t)(reg2 & 0x3f);
        } else if (funct7 == 0x20) {
          DB(inst, "sra");
          result = reg1 >> (reg2 & 0x3f);
        } else {
          return std::unexpected(Exception::IllegalInstruction);
        }
        break;
      }
      case 0x6: { // or
        DB(inst, "or");
        result = reg1 | reg2;
        break;
      }
      case 0x7: { // and
        DB(inst, "or");
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
  case 0x37: { // LUI
    DB(inst, "lui");
    int32_t imm = (inst & 0xfffff000);
    this->xregs->write(rd, imm);
    break;
  }
  case 0x17: { // auipc
    DB(inst, "auipc");
    int32_t imm = (int32_t)((uint32_t)this->pc + (uint32_t)(inst & 0xfffff000));
    this->xregs->write(rd, imm);
    break;
  }
    // TODO: Add exceptions for load operations
  case 0x3: { // load operations
    uint32_t reg1 = this->xregs->read(rs1);
    int32_t imm = ((int32_t)inst >> 20);
    switch (funct3) {
    case 0x0: { // lb
      DB(inst, "lb");
      uint32_t addr = uint32_t(reg1 + imm);
      this->xregs->write(rd, (int32_t)(int8_t)this->bus->read(addr, BYTE));
      break;
    }
    case 0x1: { // lh
      DB(inst, "lh");
      uint32_t addr = uint32_t(reg1 + imm);
      this->xregs->write(rd, (int32_t)(int16_t)this->bus->read(addr, HALFWORD));
      break;
    }
    case 0x2: { // lw
      DB(inst, "lw");
      uint32_t addr = uint32_t(reg1 + imm);
      this->xregs->write(rd, (int32_t)this->bus->read(addr, WORD));
      break;
    }
    case 0x4: { // lbu
      DB(inst, "lbu");
      uint32_t addr = uint32_t(reg1 + imm);
      this->xregs->write(rd, this->bus->read(addr, BYTE));
      break;
    }
    case 0x5: { // lhu
      DB(inst, "lhu");
      uint32_t addr = uint32_t(reg1 + imm);
      this->xregs->write(rd, this->bus->read(addr, HALFWORD));
      break;
    }
    default:
      return std::unexpected(Exception::IllegalInstruction);
    }
    break;
  }
  // TODO: Add exceptions for store operations
  case 0x23: { // store operations
    int32_t imm = rd | ((((int32_t)inst) >> 25) << 5);
    uint32_t reg1 = this->xregs->read(rs1);
    uint32_t reg2 = this->xregs->read(rs2);
    switch (funct3) {
    case 0x0: { // sb
      DB(inst, "sb");
      uint32_t dest = uint32_t(reg1 + imm);
      this->bus->write(dest, BYTE, reg2 & 0xff);
      break;
    }
    case 0x1: { // sh
      DB(inst, "sh");
      uint32_t dest = uint32_t(reg1 + imm);
      this->bus->write(dest, HALFWORD, reg2 & 0xffff);
      break;
    }
    case 0x2: { // sw
      DB(inst, "sw");
      uint32_t dest = uint32_t(reg1 + imm);
      this->bus->write(dest, WORD, reg2);
      break;
    }
    default:
      return std::unexpected(Exception::IllegalInstruction);
    }
    break;
  }
  case 0x6F: { // jal
    DB(inst, "jal");
    int32_t offset =
        (((inst >> 21) & 0x3FF) | (((inst >> 20) & 0x1) << 10) |
         (((inst >> 12) & 0xFF) << 11) | ((((int32_t)inst) >> 31)) << 19)
        << 1;
    this->xregs->write(rd, this->pc + 4);
    // TODO: Handle misalinged jumps
    this->pc += offset - 4;
    break;
  }
  case 0x67: { // jalr
    DB(inst, "jalr");
    int32_t imm = ((int32_t)inst >> 20);
    uint32_t addr = (((this->xregs->read(rs1) + imm) >> 1) << 1);
    this->xregs->write(rd, this->pc + 4);
    // TODO: Handle misalinged jumps
    this->pc = addr - 4;
    break;
  }
  // TODO: Handle misalinged jumps
  case 0x63: { // branches
    int32_t imm =
        (((inst >> 8) & 0xF) | (((inst >> 25) & 0x3F) << 4) |
         (((inst >> 7) & 0x1) << 10) | ((((int32_t)inst) >> 31)) << 11)
        << 1;
    uint32_t reg1 = this->xregs->read(rs1);
    uint32_t reg2 = this->xregs->read(rs2);
    switch (funct3) {
    case 0x0: { // beq
      DB(inst, "beq");
      if (reg1 == reg2)
        this->pc += imm - 4;
      break;
    }
    case 0x1: { // bne
      DB(inst, "bne");
      if (reg1 != reg2)
        this->pc += imm - 4;
      break;
    }
    case 0x4: { // blt
      DB(inst, "blt");
      if ((int32_t)reg1 < (int32_t)reg2)
        this->pc += imm - 4;
      break;
    }
    case 0x5: { // bge
      DB(inst, "bge");
      if ((int32_t)reg1 >= (int32_t)reg2)
        this->pc += imm - 4;
      break;
    }
    case 0x6: { // bltu
      DB(inst, "bltu");
      if (reg1 < reg2)
        this->pc += imm - 4;
      break;
    }
    case 0x7: { // bgeu
      DB(inst, "bgeu");
      if (reg1 >= reg2)
        this->pc += imm - 4;
      break;
    }
    default:
      return std::unexpected(Exception::IllegalInstruction);
    }
    break;
  } // TODO: FENCE FENCE.TSO PAUSE ECALL EBREAK
  default:
    return std::unexpected(Exception::IllegalInstruction);
  }
  return inst;
}
