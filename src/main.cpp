#include "bus.h"
#include "cpu.h"
#include "parser.h"
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iostream>
#include <vector>
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

  file.seekg(0, std::ios::end);
  const auto size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> data(static_cast<std::size_t>(size));
  file.read(reinterpret_cast<char *>(data.data()), size);

  std::cout << "Parsed succesfully\n";

  file.close();

  Bus *bus = new Bus();
  Cpu *cpu = new Cpu(bus);

  for (unsigned long i = 0; i < data.size(); i++) {
    bus->write(DRAM_BASE + i, BYTE, data[i]);
  }

  cpu->oneTick();
  return 0;
}
