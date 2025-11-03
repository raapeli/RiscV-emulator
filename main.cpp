#include<bits/stdc++.h>
#include <iostream>
#include <vector>


std::vector<int32_t> registers;
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
    while (getline(ss, instruction, del)) {
        instruction.append(instruction);
    }

    //TODO: Make an enum of instructions and a mapping function
    switch (instructions[0]) {
        case 'addi':


        default: ;
    }

    registers.reserve(32);

    return 0;
}