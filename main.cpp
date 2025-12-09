#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <sstream>

#include "instructions.h"

int32_t registers[31] = {0};
std::map<std::string, int32_t> register_map;

void prepImm(std::queue<std::string> *q, int32_t *dest, int32_t *reg1,
             int32_t *imm) {
  *dest = register_map[q->front().substr(0, 3)];
  q->pop();
  *reg1 = register_map[q->front().substr(0, 3)];
  q->pop();
  *imm = std::stoi(q->front(), 0, 16);
  q->pop();
}

void prep2reg(std::queue<std::string> *q, int32_t *dest, int32_t *reg1,
              int32_t *reg2) {
  *dest = register_map[q->front().substr(0, 3)];
  q->pop();
  *reg1 = register_map[q->front().substr(0, 3)];
  q->pop();
  *reg2 = register_map[q->front().substr(0, 3)];
  q->pop();
}

int main() {

  std::ifstream f("testi.txt");

  if (!f.is_open()) {
    std::cerr << "Error opening file" << std::endl;
    return 1;
  }

  // TODO: Clean up
  std::queue<std::string> inst_q;
  char del = ' ';

  for (int i = 0; i < 32; i++) {
    register_map[std::format("x{:02}", i)] = i;
  }

  std::stringstream ss;
  ss << f.rdbuf();
  f.close();
  std::string line;
  while (std::getline(ss, line)) {
    std::stringstream line_ss(line);
    std::string token;
    while (std::getline(line_ss, token, del)) {
      if (!token.empty()) {
        inst_q.push(token);
      }
    }
  }

  int32_t dest, reg1, reg2, imm = 0;
  while (!inst_q.empty()) {
    // TODO: Make an enum of instructions and a mapping function
    auto inst = str_to_instructions(inst_q.front());
    inst_q.pop();

    dest = 0;
    reg1 = 0;
    reg2 = 0;
    imm = 0;
    switch (inst) {
    case AND: {
      prep2reg(&inst_q, &dest, &reg1, &reg2);
      andf(registers, dest, reg1, reg2);
      break;
    }
    case ANDI: {
      prepImm(&inst_q, &dest, &reg1, &imm);
      andi(registers, dest, reg1, imm);
      break;
    }
    case OR: {
      prep2reg(&inst_q, &dest, &reg1, &reg2);
      orf(registers, dest, reg1, reg2);
      break;
    }
    case ORI: {
      prepImm(&inst_q, &dest, &reg1, &imm);
      ori(registers, dest, reg1, reg2);
      break;
    }
    case XOR: {
      prep2reg(&inst_q, &dest, &reg1, &reg2);
      xorf(registers, dest, reg1, reg2);
      break;
    }
    case XORI: {
      prepImm(&inst_q, &dest, &reg1, &imm);
      xori(registers, dest, reg1, reg2);
      break;
    }

    case ADD: {
      prep2reg(&inst_q, &dest, &reg1, &reg2);
      add(registers, dest, reg1, imm);
      break;
    }
    case ADDI: {
      prepImm(&inst_q, &dest, &reg1, &imm);
      addi(registers, dest, reg1, imm);
      break;
    }
    case SUB: {
      prep2reg(&inst_q, &dest, &reg1, &reg2);
      sub(registers, dest, reg1, reg2);
      break;
    }
    case SLT: {
      prep2reg(&inst_q, &dest, &reg1, &reg2);
      slt(registers, dest, reg1, reg2);
      break;
    }
    case SLTU: {
      prep2reg(&inst_q, &dest, &reg1, &reg2);
      sltu(registers, dest, reg1, reg2);
      break;
    }
    case SLTI: {
      prepImm(&inst_q, &dest, &reg1, &imm);
      slti(registers, dest, reg1, imm);
      break;
    }
    case SLTIU: {
      prepImm(&inst_q, &dest, &reg1, &imm);
      sltiu(registers, dest, reg1, imm);
      break;
    }
    case SRL: {
      prep2reg(&inst_q, &dest, &reg1, &reg2);
      srl(registers, dest, reg1, reg2);
      break;
    }
    case SRA: {
      prep2reg(&inst_q, &dest, &reg1, &reg2);
      sra(registers, dest, reg1, reg2);
      break;
    }
    case SRLI: {
      prepImm(&inst_q, &dest, &reg1, &imm);
      srli(registers, dest, reg1, imm);
      break;
    }
    case SRAI: {
      prepImm(&inst_q, &dest, &reg1, &imm);
      srai(registers, dest, reg1, imm);
      break;
    }

    default:
      NULL;
    }
  }
  for (int i = 0; i < 32; i++) {
    std::cout << std::format("(x{:02}): {:x}\n", i, registers[i]);
  }

  return 0;
}
