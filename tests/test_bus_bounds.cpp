#include "bus.h"
#include "cpu.h"

#include <iostream>

int main() {
  Bus bus;
  const uint64_t end = DRAM_BASE + DRAM_SIZE;

  for (uint8_t size : {BYTE, HALFWORD, WORD, DOUBLEWORD}) {
    const uint64_t last = end - size / 8;
    bus.write(last, size, 0x5a);
    bus.write(last + 1, size, 0xff);

    if (bus.read(last, size) != 0x5a ||
        bus.read(last + 1, size) != UINT64_MAX ||
        bus.read(DRAM_BASE - 1, size) != UINT64_MAX) {
      std::cerr << "Incorrect RAM bounds for " << unsigned(size)
                << "-bit access\n";
      return 1;
    }
  }
}
