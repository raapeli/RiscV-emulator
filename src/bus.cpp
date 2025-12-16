#include "bus.h"
#include "cpu.h"
#include <cstdint>

Bus::Bus() { dram = std::vector<uint8_t>(DRAM_SIZE / 8, 0); }

uint32_t Bus::read(uint32_t addr, uint8_t size) {
  if ((addr - DRAM_BASE) < DRAM_SIZE) {
    switch (size) {
    case BYTE:
      return read8(addr);
    case HALFWORD:
      return read16(addr);
    case WORD:
      return read32(addr);
    }
  }

  return -1;
}

void Bus::write(uint32_t addr, uint8_t size, uint32_t value) {
  if ((addr - DRAM_BASE) < DRAM_SIZE) {
    switch (size) {
    case BYTE:
      write8(addr, value);
    case HALFWORD:
      write16(addr, value);
    case WORD:
      write32(addr, value);
    }
  }
}

uint32_t Bus::read8(uint32_t addr) {
  return static_cast<uint32_t>(dram[addr - DRAM_BASE]);
}

uint32_t Bus::read16(uint32_t addr) {
  uint32_t index = addr - DRAM_BASE;
  return static_cast<uint32_t>(dram[index] | (dram[index + 1] << 8));
}

uint32_t Bus::read32(uint32_t addr) {
  uint32_t index = addr - DRAM_BASE;
  return static_cast<uint32_t>((dram[index]) | (dram[index + 1] << 8) |
                               (dram[index + 2] << 16) | dram[index + 3] << 24);
}

void Bus::write8(uint32_t addr, uint32_t value) {
  dram[addr - DRAM_BASE] = static_cast<uint8_t>(value);
}

void Bus::write16(uint32_t addr, uint32_t value) {
  uint32_t index = addr - DRAM_BASE;
  dram[index] = static_cast<uint8_t>(value & 0xff);
  dram[index + 1] = static_cast<uint8_t>((value >> 8) & 0xff);
}

void Bus::write32(uint32_t addr, uint32_t value) {
  uint32_t index = addr - DRAM_BASE;
  dram[index] = static_cast<uint8_t>(value & 0xff);
  dram[index + 1] = static_cast<uint8_t>((value >> 8) & 0xff);
  dram[index + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
  dram[index + 3] = static_cast<uint8_t>((value >> 24) & 0xff);
}
