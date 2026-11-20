#include "bus.h"
#include "cpu.h"
#include <cstdint>
#include <iostream>
#include <poll.h>
#include <unistd.h>

Bus::Bus(bool enable_console, uint64_t base, uint64_t size,
         DeviceMap devices)
    : ram_base(base), dram(size, 0), console_enabled(enable_console) {
  if (devices.clint)
    clint_device.emplace(*devices.clint);
  if (devices.plic)
    plic_device.emplace(*devices.plic);
  if (devices.uart) {
    uart_device.emplace(devices.uart->base);
    uart_plic_source = devices.uart->plic_source;
  }
}

Clint *Bus::clint() { return clint_device ? &*clint_device : nullptr; }

Plic *Bus::plic() { return plic_device ? &*plic_device : nullptr; }

bool Bus::contains_ram_access(uint64_t addr, uint8_t size) const {
  if (size != BYTE && size != HALFWORD && size != WORD && size != DOUBLEWORD)
    return false;
  const uint64_t offset = addr - ram_base;
  return addr >= ram_base && offset < dram.size() &&
         size / 8 <= dram.size() - offset && addr <= UINT64_MAX - (size / 8 - 1);
}

bool Bus::contains_console_access(uint64_t addr, uint8_t size, bool write) const {
  return console_enabled && !uart_device && size == BYTE &&
         (addr == CONSOLE_DATA || (!write && addr == CONSOLE_STATUS));
}

bool Bus::contains_mmio_access(uint64_t addr, uint8_t size, bool write) const {
  return contains_console_access(addr, size, write) ||
         (clint_device && clint_device->contains_access(addr, size)) ||
         (plic_device && plic_device->contains_access(addr, size)) ||
         (uart_device && uart_device->contains_access(addr, size));
}

std::optional<uint8_t> Bus::read_console_input() {
  if (console_input_closed)
    return std::nullopt;

  pollfd input = {STDIN_FILENO, POLLIN, 0};
  if (poll(&input, 1, 0) <= 0)
    return std::nullopt;

  if (input.revents & (POLLERR | POLLNVAL)) {
    console_input_closed = true;
  } else if (input.revents & (POLLIN | POLLHUP)) {
    uint8_t byte;
    const auto count = ::read(STDIN_FILENO, &byte, sizeof(byte));
    if (count == 1)
      return byte;
    if (count == 0)
      console_input_closed = true;
  }
  return std::nullopt;
}

void Bus::update_uart_interrupt() {
  if (plic_device)
    plic_device->set_external_level(uart_plic_source,
                                    uart_device->interrupt_pending());
}

void Bus::tick_uart() {
  if (!uart_device)
    return;
  uart_device->tick();
  // Avoid a host syscall on every instruction while still polling an idle UART.
  constexpr unsigned INPUT_POLL_INTERVAL = 256;
  if (uart_poll_ticks == 0 && console_enabled && uart_device->can_receive()) {
    if (auto byte = read_console_input())
      uart_device->receive(*byte);
  }
  uart_poll_ticks = (uart_poll_ticks + 1) % INPUT_POLL_INTERVAL;
  update_uart_interrupt();
}

uint64_t Bus::read(uint64_t addr, uint8_t size) {
  if (clint_device && clint_device->contains_access(addr, size))
    return clint_device->read(addr, size);
  if (plic_device && plic_device->contains_access(addr, size))
    return plic_device->read(addr, size);
  if (uart_device && uart_device->contains_access(addr, size)) {
    const uint64_t value = uart_device->read(addr, size);
    update_uart_interrupt();
    return value;
  }
  if (contains_console_access(addr, size, false)) {
    if (!console_input)
      console_input = read_console_input();
    if (addr == CONSOLE_STATUS)
      return console_input.has_value();

    const auto byte = console_input.value_or(0);
    console_input.reset();
    return byte;
  }

  if (contains_ram_access(addr, size)) {
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
  if (clint_device && clint_device->contains_access(addr, size)) {
    clint_device->write(addr, size, value);
    return;
  }
  if (plic_device && plic_device->contains_access(addr, size)) {
    plic_device->write(addr, size, value);
    return;
  }
  if (uart_device && uart_device->contains_access(addr, size)) {
    if (auto byte = uart_device->write(addr, size, value); byte && console_enabled)
      std::cout.put(static_cast<char>(*byte)).flush();
    update_uart_interrupt();
    return;
  }
  if (contains_console_access(addr, size, true)) {
    std::cout.put(static_cast<char>(value & 0xFF)).flush();
    return;
  }

  if (contains_ram_access(addr, size)) {
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
  return static_cast<uint64_t>(dram[addr - ram_base]);
}

uint64_t Bus::read16(uint64_t addr) {
  uint64_t index = addr - ram_base;
  return static_cast<uint64_t>(dram[index] | (dram[index + 1] << 8));
}

uint64_t Bus::read32(uint64_t addr) {
  uint64_t index = addr - ram_base;
  return static_cast<uint64_t>((dram[index]) | (dram[index + 1] << 8) |
                               (dram[index + 2] << 16) |
                               (dram[index + 3] << 24));
}

uint64_t Bus::read64(uint64_t addr) {
  uint64_t index = addr - ram_base;
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
  dram[addr - ram_base] = static_cast<uint8_t>(value);
}

void Bus::write16(uint64_t addr, uint64_t value) {
  uint64_t index = addr - ram_base;
  dram[index] = static_cast<uint8_t>(value & 0xFF);
  dram[index + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void Bus::write32(uint64_t addr, uint64_t value) {
  uint64_t index = addr - ram_base;
  dram[index] = static_cast<uint8_t>(value & 0xFF);
  dram[index + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  dram[index + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
  dram[index + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}
void Bus::write64(uint64_t addr, uint64_t value) {
  uint64_t index = addr - ram_base;
  dram[index] = static_cast<uint8_t>(value & 0xFF);
  dram[index + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  dram[index + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
  dram[index + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
  dram[index + 4] = static_cast<uint8_t>((value >> 32) & 0xFF);
  dram[index + 5] = static_cast<uint8_t>((value >> 40) & 0xFF);
  dram[index + 6] = static_cast<uint8_t>((value >> 48) & 0xFF);
  dram[index + 7] = static_cast<uint8_t>((value >> 56) & 0xFF);
}
