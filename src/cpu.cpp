#include "cpu.h"
#include <cstdint>
#include <iostream>
#include <sys/types.h>

#define DEBUG

#ifdef DEBUG
#define DB(x) std::cerr << x << std::endl
#else
#define DB(x) ((void)0)
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

int Cpu::execute() {
  uint32_t inst = this->fetch();
  this->executeGeneral(inst);
  this->pc += 4;

  return 0;
}

int Cpu::executeGeneral(uint32_t inst) {

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
      DB("addi");
      result = (int32_t)(reg1) + imm;
      break;
    }
    case 0x2: { // slti
      DB("slti");
      result = ((int32_t)reg1 < imm);
      break;
    }
    case 0x3: { // sltiu
      DB("sltiu");
      result = (reg1 < imm);
      break;
    }
    case 0x4: { // xori
      DB("XORI");
      result = reg1 ^ imm;
      break;
    }
    case 0x6: { // ori
      DB("ori");
      result = reg1 | imm;
      break;
    }
    case 0x7: { // andi
      DB("andi");
      result = reg1 & imm;
      break;
    }
    case 0x1: { // slli
      DB("slli");
      result = reg1 << (imm & 0x3F);
      break;
    }
    case 0x5: { // srli or srai
      // TODO: Add checks for illegal instructions
      if (funct7 >> 1) {
        DB("srai");
        result = reg1 >> (uint32_t)(imm & 0x3f);
      } else {
        DB("srli");
        result = (int32_t)reg1 >> (int32_t)(imm & 0x3F);
      }
      break;
    }
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
        DB("mul");
        result = (int32_t)((int32_t)reg1 * (int32_t)reg2);
        break;
      }
      case 0x1: { // mulh
        DB("mulh");
        result =
            (int32_t)(((uint64_t)(int32_t)reg1 * (uint64_t)(int32_t)reg2) >>
                      32);
        break;
      }
      case 0x2: { // mulhsu
        DB("mulhsu");
        result = (int32_t)(((uint64_t)(int32_t)reg1 * (uint64_t)reg2) >> 32);
        break;
      }
      case 0x3: { // mulhu
        DB("mulhu");
        result = (int32_t)((uint64_t)reg1 * (uint64_t)reg2 >> 32);
        break;
      }
      case 0x4: { // div
        DB("div");
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
        DB("divu");
        if (reg2 == 0) {
          result = UINT32_MAX;
        } else {
          result = reg1 / reg2;
        }
        break;
      }
      case 0x6: { // rem
        DB("rem");
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
        DB("remu");
        result = reg1 % reg2;
        break;
      }
      }
    } else { // non-multiplication
      switch (funct3) {
      case 0x0: {
        // TODO: Add checks for illegal instructions
        if (funct7 == 0) { // add
          DB("add");
          result = (int32_t)(reg1 + reg2);
        } else { // sub
          DB("sub");
          result = (int32_t)(reg1 - reg2);
        }
        break;
      }
      case 0x1: { // sll
        DB("sll");
        result = reg1 << (reg2 & 0x3f);
        break;
      }
      case 0x2: { // slt
        DB("sll");
        result = ((int32_t)reg1 < (int32_t)reg2) ? 1 : 0;
        break;
      }
      case 0x3: { // sltu
        DB("sll");
        result = (reg1 < reg2) ? 1 : 0;
        break;
      }
      case 0x4: { // xor
        DB("xor");
        result = reg1 ^ reg2;
        break;
      }
      case 0x5: {
        if (funct7 == 0) { // srl
          DB("srl");
          result = (int32_t)reg1 >> (int32_t)(reg2 & 0x3f);
        } else {
          DB("sra");
          result = reg1 >> (reg2 & 0x3f);
        }
        break;
      }
      case 0x6: { // or
        DB("or");
        result = reg1 | reg2;
        break;
      }
      case 0x7: { // and
        DB("or");
        result = reg1 & reg2;
        break;
      }
      }
    }
    // Finally write the result
    this->xregs->write(rd, result);
    break;
  }
  case 0x37: { // LUI
    DB("lui");
    int32_t imm = (inst & 0xfffff000);
    this->xregs->write(rd, imm);
    break;
  }
  case 0x17: { // auipc
    DB("auipc");
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
      DB("lb");
      uint32_t addr = uint32_t(reg1 + imm);
      this->xregs->write(rd, (int32_t)(int8_t)this->bus->read(addr, BYTE));
      break;
    }
    case 0x1: { // lh
      DB("lh");
      uint32_t addr = uint32_t(reg1 + imm);
      this->xregs->write(rd, (int32_t)(int16_t)this->bus->read(addr, HALFWORD));
      break;
    }
    case 0x2: { // lw
      DB("lw");
      uint32_t addr = uint32_t(reg1 + imm);
      this->xregs->write(rd, (int32_t)this->bus->read(addr, WORD));
      break;
    }
    case 0x4: { // lbu
      DB("lbu");
      uint32_t addr = uint32_t(reg1 + imm);
      this->xregs->write(rd, this->bus->read(addr, BYTE));
      break;
    }
    case 0x5: { // lhu
      DB("lhu");
      uint32_t addr = uint32_t(reg1 + imm);
      this->xregs->write(rd, this->bus->read(addr, HALFWORD));
      break;
    }
    }
    break;
  }
  case 0x23: { // store operations
    int32_t imm = rd | ((((int32_t)inst) >> 25) << 5);
    uint32_t reg1 = this->xregs->read(rs1);
    uint32_t reg2 = this->xregs->read(rs2);
    switch (funct3) {
    case 0x0: { // sb
      DB("sb");
      uint32_t dest = uint32_t(reg1 + imm);
      this->bus->write(dest, BYTE, reg2 & 0xff);
      break;
    }
    case 0x1: { // sh
      DB("sh");
      uint32_t dest = uint32_t(reg1 + imm);
      this->bus->write(dest, HALFWORD, reg2 & 0xffff);
      break;
    }
    case 0x2: { // sw
      DB("sw");
      uint32_t dest = uint32_t(reg1 + imm);
      this->bus->write(dest, WORD, reg2);
      break;
    }
    }
    break;
  }
  case 0x6F: { // jal
    DB("jal");
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
    DB("jalr");
    int32_t imm = ((int32_t)inst >> 20);
    uint32_t addr = (((this->xregs->read(rs1) + imm) >> 1) << 1);
    this->xregs->write(rd, this->pc + 4);
    // TODO: Handle misalinged jumps
    this->pc = addr - 4;
    break;
  }
  }
  return 1;
}
