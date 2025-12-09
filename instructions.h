#include <string>

// TODO: Add more instructions
enum instructions {
  AND,
  ANDI,
  OR,
  ORI,
  XOR,
  XORI,
  ADD,
  ADDI,
  SUB,
  SLT,
  SLTU,
  SLTI,
  SLTIU,
  SRL,
  SRA,
  SRLI,
  SRAI,
  EBREAK
};

instructions str_to_instructions(const std::string &instruction);

void andf(int *regs, int dest, int reg1, int reg2);
void orf(int *regs, int dest, int reg1, int reg2);
void xorf(int *regs, int dest, int reg1, int reg2);
void andi(int *regs, int dest, int reg1, int imm);
void ori(int *regs, int dest, int reg1, int reg2);
void xori(int *regs, int dest, int reg1, int reg2);

void addi(int *regs, int dest, int reg1, int imm);
void add(int *regs, int dest, int reg1, int reg2);
void sub(int *regs, int dest, int reg1, int reg2);

void slt(int *regs, int dest, int reg1, int reg2);
void sltu(int *regs, int dest, int reg1, int reg2);
void slti(int *regs, int dest, int reg1, int reg2);
void sltiu(int *regs, int dest, int reg1, int reg2);

void srl(int *regs, int dest, int reg1, int reg2);
void srli(int *regs, int dest, int reg1, int imm);
void sra(int *regs, int dest, int reg1, int reg2);
void srai(int *regs, int dest, int reg1, int imm);
