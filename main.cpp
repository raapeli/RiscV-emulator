#include<bits/stdc++.h>
#include <iostream>
#include <vector>
#include <format>



//TODO: More instructions and also reformat whole file to a header
enum instructions {
    ADDI,
    EBREAK
};

std::array<int, 32> registers;
std::map<std::string, int32_t> register_map;

instructions str_to_instructions(const std::string& instruction) {

    if (instruction == "addi") {
        return ADDI;
    }
    return EBREAK;
}


void addi(std::vector<std::string>& args) {
    const int dest_reg = register_map[args[1].substr(0, 3)];
    const int source_reg = register_map[args[2].substr(0, 3)];
    const int immediate_reg = static_cast<int>(strtoul(args[3].c_str(), nullptr, 16));

    registers[dest_reg] =  registers.at(source_reg) + immediate_reg;
}

int main() {

    std::ifstream f("testi.txt");

    if (!f.is_open()) {
        std::cerr << "Error opening file" << std::endl;
        return 1;
    }
    std::string s;

    //TODO: Clean up
    std::vector<std::string> instructions;
    std::getline(f, s);
    std::getline(f, s);
    std::cout << s << std::endl;
    std::stringstream ss(s);
    char del = ' ';
    std::string instruction;


    registers.fill(0);

    for (int i = 0; i < 32; i++) {
        register_map[std::format("x{:02}", i)] = i;
    }

    while (getline(ss, instruction, del)) {
        if (!instruction.empty())
            instructions.push_back(instruction);
    }

    //TODO: Make an enum of instructions and a mapping function
    switch (str_to_instructions(instructions[0])) {
        case ADDI:
            addi(instructions);

        default:
            NULL;

    }

    for (int i = 0; i < registers.size(); i++) {
        std::cout << std::format("(x{:02}): {:x}\n", i, registers.at(i));
    }

    return 0;
}