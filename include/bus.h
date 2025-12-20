#pragma once

#include <cstdint>
#include <vector>

const uint64_t DRAM_BASE = 0x8000;
const uint64_t DRAM_SIZE = 1024 * 1024;

class Bus {
public:
  Bus();

  uint64_t read(uint64_t addr, uint8_t size);
  void write(uint64_t addr, uint8_t size, uint64_t value);

private:
  std::vector<uint8_t> dram;

  uint64_t read8(uint64_t addr);
  uint64_t read16(uint64_t addr);
  uint64_t read32(uint64_t addr);
  void write8(uint64_t addr, uint64_t value);
  void write16(uint64_t addr, uint64_t value);
  void write32(uint64_t addr, uint64_t value);
};
