#include <string>

// TODO: Add more instructions
enum instructions { ADDI, SUB, EBREAK };

instructions str_to_instructions(const std::string &instruction);

void addi(int *regs, int dest, int reg1, int imm);
void sub(int *regs, int dest, int reg1, int reg2);
