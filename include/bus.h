#pragma once

#include <cstdint>
#include <vector>

const uint32_t DRAM_BASE = 0x8000;
const uint32_t DRAM_SIZE = 1024 * 1024;

class Bus {
public:
  Bus();

  uint32_t read(uint32_t addr, uint8_t size);
  void write(uint32_t addr, uint8_t size, uint32_t value);

private:
  std::vector<uint8_t> dram;

  uint32_t read8(uint32_t addr);
  uint32_t read16(uint32_t addr);
  uint32_t read32(uint32_t addr);
  void write8(uint32_t addr, uint32_t value);
  void write16(uint32_t addr, uint32_t value);
  void write32(uint32_t addr, uint32_t value);
};
