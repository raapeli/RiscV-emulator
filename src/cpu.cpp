#include "cpu.h"
#include <cstdint>
#include <iostream>

#define DEBUG

#ifdef DEBUG
#define DP(x) std::cerr << x << std::endl
#else
#define DP(x) ((void)0)
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
  // ADD, SUB, SLL, SLT, SLTU
  // XOR, SRL, SRA, OR, AND
  case 0x33: { // Integer register-register + integer multi
    uint32_t result;
    uint32_t reg1 = this->xregs->read(rs1);
    uint32_t reg2 = this->xregs->read(rs2);
    if (funct7 & 0x1) { // Multiplication
      switch (funct3) {
      case 0x00: { // mul
        DP("mul");
        result = (int32_t)((int32_t)reg1 * (int32_t)reg2);
        break;
      }
      case 0x1: { // mulh
        DP("mulh");
        result =
            (int32_t)(((uint64_t)(int32_t)reg1 * (uint64_t)(int32_t)reg2) >>
                      32);
        break;
      }
      case 0x2: { // mulhsu
        DP("mulhsu");
        result = (int32_t)(((uint64_t)(int32_t)reg1 * (uint64_t)reg2) >> 32);
        break;
      }
      case 0x3: { // mulhu
        DP("mulhu");
        result = (int32_t)((uint64_t)reg1 * (uint64_t)reg2 >> 32);
        break;
      }
      case 0x4: { // div
        DP("div");
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
        DP("divu");
        if (reg2 == 0) {
          result = UINT32_MAX;
        } else {
          result = reg1 / reg2;
        }
        break;
      }
      case 0x6: { // rem
        DP("rem");
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
        DP("remu");
        result = reg1 % reg2;
        break;
      }
      }
    } else { // non-multiplication
      switch (funct3) {
      case 0x0: {
        if (funct7 == 0) { // add
          DP("add");
          result = (int32_t)(reg1 + reg2);
        } else { // sub
          DP("sub");
          result = (int32_t)(reg1 - reg2);
        }
        break;
      }
      case 0x1: { // sll
        DP("sll");
        result = reg1 << (reg2 & 0x3f);
        break;
      }
      case 0x2: { // slt
        DP("sll");
        result = ((int32_t)reg1 < (int32_t)reg2) ? 1 : 0;
        break;
      }
      case 0x3: { // sltu
        DP("sll");
        result = (reg1 < reg2) ? 1 : 0;
        break;
      }
      case 0x4: { // xor
        DP("xor");
        result = reg1 ^ reg2;
        break;
      }
      case 0x5: {
        if (funct7 == 0) { // srl
          DP("srl");
          result = (int32_t)reg1 >> (int32_t)(reg2 & 0x3f);
        } else {
          DP("sra");
          result = reg1 >> (reg2 & 0x3f);
        }
        break;
      }
      case 0x6: { // or
        DP("or");
        result = reg1 | reg2;
        break;
      }
      case 0x7: { // and
        DP("or");
        result = reg1 & reg2;
        break;
      }
      }
    }
    // Finally write the result
    this->xregs->write(rd, result);
  }
  }
  return 1;
}
