#include "parser.h"
#include <fstream>
#include <iostream>
int main(int argc, char *argv[]) {

  if (argc != 2) {
    std::cout << "Needs at least one filepath\n";
    return -1;
  }

  std::ifstream file(argv[1], std::ios::binary);
  if (!file) {
    std::cerr << "File reading failed\n";
    return -1;
  }
  std::string source((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
  source += '\n';

  std::cout << "Parsed succesfully\n";

  return 0;
}
