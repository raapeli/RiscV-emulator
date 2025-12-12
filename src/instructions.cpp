#include "instructions.h"
#include <cstdint>

instructions str_to_instructions(const std::string &instruction) {
  if (instruction == "and") {
    return AND;
  } else if (instruction == "andi") {
    return ANDI;
  } else if (instruction == "or") {
    return OR;
  } else if (instruction == "ori") {
    return ORI;
  } else if (instruction == "xor") {
    return XOR;
  } else if (instruction == "xori") {
    return XORI;
  } else if (instruction == "add") {
    return ADD;
  } else if (instruction == "addi") {
    return ADDI;
  } else if (instruction == "sub") {
    return SUB;
  } else if (instruction == "slt") {
    return SLT;
  } else if (instruction == "sltu") {
    return SLTU;
  } else if (instruction == "slti") {
    return SLTI;
  } else if (instruction == "sltiu") {
    return SLTIU;
  } else if (instruction == "srl") {
    return SRL;
  } else if (instruction == "sra") {
    return SRA;
  } else if (instruction == "srli") {
    return SRLI;
  } else if (instruction == "srai") {
    return SRAI;
  } else if (instruction == "ebreak") {
    return EBREAK;
  }
  return EBREAK; // Default for invalid instructions
}

// LOGICAL OPERATORS
void andf(int *regs, int dest, int reg1, int reg2) {
  regs[dest] = regs[reg1] & regs[reg2];
}
void andi(int *regs, int dest, int reg1, int imm) {
  regs[dest] = regs[reg1] & imm;
}
void orf(int *regs, int dest, int reg1, int reg2) {
  regs[dest] = regs[reg1] | regs[reg2];
}
void xorf(int *regs, int dest, int reg1, int reg2) {
  regs[dest] = regs[reg1] ^ regs[reg2];
}
void ori(int *regs, int dest, int reg1, int imm) {
  regs[dest] = regs[reg1] or imm;
}
void xori(int *regs, int dest, int reg1, int imm) {
  regs[dest] = regs[reg1] ^ imm;
}

// ARTIHMETIC OPERATORS
void add(int *regs, int dest, int reg1, int reg2) {
  regs[dest] = regs[reg1] + regs[reg2];
}
void addi(int *regs, int dest, int reg1, int imm) {
  regs[dest] = regs[reg1] + imm;
}
void sub(int *regs, int dest, int reg1, int reg2) {
  regs[dest] = regs[reg1] - regs[reg2];
}

// COMPARISONS
void slt(int *regs, int dest, int reg1, int reg2) {
  if ((int32_t)regs[reg1] < (int32_t)regs[reg2])
    regs[dest] = 1;
  else
    regs[dest] = 0;
}
void sltu(int *regs, int dest, int reg1, int reg2) {
  if ((u_int32_t)regs[reg1] < (u_int32_t)regs[reg2])
    regs[dest] = 1;
  else
    regs[dest] = 0;
}
void slti(int *regs, int dest, int reg1, int imm) {
  if ((int32_t)regs[reg1] < (int32_t)imm)
    regs[dest] = 1;
  else
    regs[dest] = 0;
}
void sltiu(int *regs, int dest, int reg1, int imm) {
  if ((u_int32_t)regs[reg1] < (u_int32_t)imm)
    regs[dest] = 1;
  else
    regs[dest] = 0;
}

// SHIFTS
void srl(int *regs, int dest, int reg1, int reg2) {
  regs[dest] = (u_int32_t)regs[reg1] >> regs[reg2];
}
void sra(int *regs, int dest, int reg1, int reg2) {
  regs[dest] = (int32_t)regs[reg1] >> regs[reg2];
}
void srli(int *regs, int dest, int reg1, int imm) {
  regs[dest] = (u_int32_t)regs[reg1] >> imm;
}
void srai(int *regs, int dest, int reg1, int imm) {
  regs[dest] = (int32_t)regs[reg1] >> imm;
}
