#pragma once

#include "devices/clint.h"
#include "devices/plic.h"
#include "devices/uart.h"

#include <cstdint>
#include <optional>
#include <vector>

const uint64_t DRAM_BASE = 0x8000;
const uint64_t DRAM_SIZE = 1024 * 1024;
const uint64_t CONSOLE_DATA = 0x10000000;
const uint64_t CONSOLE_STATUS = CONSOLE_DATA + 1;

class Bus {
public:
  struct UartMapping {
    uint64_t base;
    unsigned plic_source;
  };
  struct DeviceMap {
    std::optional<uint64_t> clint;
    std::optional<uint64_t> plic;
    std::optional<UartMapping> uart;
  };
  explicit Bus(bool enable_console = false, uint64_t base = DRAM_BASE,
               uint64_t size = DRAM_SIZE, DeviceMap devices = {});

  uint64_t read(uint64_t addr, uint8_t size);
  void write(uint64_t addr, uint8_t size, uint64_t value);
  bool contains_ram_access(uint64_t addr, uint8_t size) const;
  bool contains_mmio_access(uint64_t addr, uint8_t size, bool write) const;
  Clint *clint();
  Plic *plic();
  void tick_uart();
  const uint64_t ram_base;
  std::vector<uint8_t> dram;

private:
  bool contains_console_access(uint64_t addr, uint8_t size, bool write) const;
  std::optional<Clint> clint_device;
  std::optional<Plic> plic_device;
  std::optional<Uart> uart_device;
  unsigned uart_plic_source = 0;
  unsigned uart_poll_ticks = 0;
  void update_uart_interrupt();
  std::optional<uint8_t> read_console_input();
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
