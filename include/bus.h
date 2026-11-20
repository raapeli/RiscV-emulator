#pragma once

#include <cstdint>
#include <optional>
#include <vector>

const uint64_t DRAM_BASE = 0x8000;
const uint64_t DRAM_SIZE = 1024 * 1024;
const uint64_t CONSOLE_DATA = 0x10000000;
const uint64_t CONSOLE_STATUS = CONSOLE_DATA + 1;

class Bus {
public:
  explicit Bus(bool enable_console = false);

  uint64_t read(uint64_t addr, uint8_t size);
  void write(uint64_t addr, uint8_t size, uint64_t value);
  std::vector<uint8_t> dram;

private:
  void poll_console_input();
  bool console_enabled;
  bool console_input_closed = false;
  std::optional<uint8_t> console_input;

  uint64_t read8(uint64_t addr);
  uint64_t read16(uint64_t addr);
  uint64_t read32(uint64_t addr);
  uint64_t read64(uint64_t addr);
  void write8(uint64_t addr, uint64_t value);
  void write16(uint64_t addr, uint64_t value);
  void write32(uint64_t addr, uint64_t value);
  void write64(uint64_t addr, uint64_t value);
};
