#include "instructions.h"

instructions str_to_instructions(const std::string &instruction) {

  if (instruction == "addi") {
    return ADDI;
  } else if (instruction == "sub") {
    return SUB;
  }
  return EBREAK;
}

void addi(int *regs, int dest, int reg1, int imm) {
  regs[dest] = regs[reg1] + imm;
}

void sub(int *regs, int dest, int reg1, int reg2) {
  regs[dest] = regs[reg1] - regs[reg2];
}
