#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <sstream>

#include "instructions.h"

int registers[31] = {0};
std::map<std::string, int32_t> register_map;

int main() {

  std::ifstream f("testi.txt");

  if (!f.is_open()) {
    std::cerr << "Error opening file" << std::endl;
    return 1;
  }

  // TODO: Clean up
  std::queue<std::string> instructions;
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
        instructions.push(token);
      }
    }
  }

  while (!instructions.empty()) {
    // TODO: Make an enum of instructions and a mapping function
    auto inst = str_to_instructions(instructions.front());
    instructions.pop();

    switch (inst) {

    case ADDI: {
      auto dest = register_map[instructions.front().substr(0, 3)];
      instructions.pop();
      auto reg1 = register_map[instructions.front().substr(0, 3)];
      instructions.pop();
      auto imm = std::stoi(instructions.front(), 0, 16);
      instructions.pop();

      addi(registers, dest, reg1, imm);
      break;
    }
    case SUB: {
      auto dest = register_map[instructions.front().substr(0, 3)];
      instructions.pop();
      auto reg1 = register_map[instructions.front().substr(0, 3)];
      instructions.pop();
      auto reg2 = register_map[instructions.front().substr(0, 3)];
      instructions.pop();

      sub(registers, dest, reg1, reg2);
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
