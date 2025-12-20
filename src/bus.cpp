#include "bus.h"
#include "cpu.h"
#include <cstdint>
Bus::Bus() { dram = std::vector<uint8_t>(DRAM_SIZE / 8, 0); }

uint64_t Bus::read(uint64_t addr, uint8_t size) {
  if ((addr - DRAM_BASE) < DRAM_SIZE) {
    switch (size) {
    case BYTE:
      return read8(addr);
    case HALFWORD:
      return read16(addr);
    case WORD:
      return read32(addr);
    case DOUBLEWORD:
      return read64(addr);
    }
  }

  return -1;
}

void Bus::write(uint64_t addr, uint8_t size, uint64_t value) {
  if ((addr - DRAM_BASE) < DRAM_SIZE) {
    switch (size) {
    case BYTE:
      write8(addr, value);
      break;
    case HALFWORD:
      write16(addr, value);
      break;
    case WORD:
      write32(addr, value);
      break;
    case DOUBLEWORD:
      write64(addr, value);
    }
  }
}

uint64_t Bus::read8(uint64_t addr) {
  return static_cast<uint64_t>(dram[addr - DRAM_BASE]);
}

uint64_t Bus::read16(uint64_t addr) {
  uint64_t index = addr - DRAM_BASE;
  return static_cast<uint64_t>(dram[index] | (dram[index + 1] << 8));
}

uint64_t Bus::read32(uint64_t addr) {
  uint64_t index = addr - DRAM_BASE;
  return static_cast<uint64_t>((dram[index]) | (dram[index + 1] << 8) |
                               (dram[index + 2] << 16) |
                               (dram[index + 3] << 24));
}

uint64_t Bus::read64(uint64_t addr) {
  uint64_t index = addr - DRAM_BASE;
  return static_cast<uint64_t>((dram[index]) |
                               (static_cast<uint64_t>(dram[index + 1]) << 8) |
                               (static_cast<uint64_t>(dram[index + 2]) << 16) |
                               (static_cast<uint64_t>(dram[index + 3]) << 24) |
                               (static_cast<uint64_t>(dram[index + 4]) << 32) |
                               (static_cast<uint64_t>(dram[index + 5]) << 40) |
                               (static_cast<uint64_t>(dram[index + 6]) << 48) |
                               (static_cast<uint64_t>(dram[index + 7]) << 56));
}

void Bus::write8(uint64_t addr, uint64_t value) {
  dram[addr - DRAM_BASE] = static_cast<uint8_t>(value);
}

void Bus::write16(uint64_t addr, uint64_t value) {
  uint64_t index = addr - DRAM_BASE;
  dram[index] = static_cast<uint8_t>(value & 0xFF);
  dram[index + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void Bus::write32(uint64_t addr, uint64_t value) {
  uint64_t index = addr - DRAM_BASE;
  dram[index] = static_cast<uint8_t>(value & 0xFF);
  dram[index + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  dram[index + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
  dram[index + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}
void Bus::write64(uint64_t addr, uint64_t value) {
  uint64_t index = addr - DRAM_BASE;
  dram[index] = static_cast<uint8_t>(value & 0xFF);
  dram[index + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  dram[index + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
  dram[index + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
  dram[index + 4] = static_cast<uint8_t>((value >> 32) & 0xFF);
  dram[index + 5] = static_cast<uint8_t>((value >> 40) & 0xFF);
  dram[index + 6] = static_cast<uint8_t>((value >> 48) & 0xFF);
  dram[index + 7] = static_cast<uint8_t>((value >> 56) & 0xFF);
}
