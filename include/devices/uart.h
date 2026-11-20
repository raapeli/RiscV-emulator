#pragma once

#include <cstdint>
#include <deque>
#include <optional>

class Uart {
public:
  explicit Uart(uint64_t base) : base(base) {}
  bool contains_access(uint64_t address, uint8_t size) const;
  uint64_t read(uint64_t address, uint8_t size);
  std::optional<uint8_t> write(uint64_t address, uint8_t size, uint64_t value);
  bool can_receive() const;
  void receive(uint8_t byte);
  void tick();
  bool interrupt_pending() const;

private:
  uint8_t interrupt_id() const;
  unsigned receive_capacity() const;
  void reset_receive_timeout();

  uint64_t base;
  std::deque<uint8_t> receive_buffer;
  unsigned receive_timeout = 0;
  uint16_t divisor = 0;
  uint8_t interrupt_enable = 0;
  uint8_t fifo_control = 0;
  uint8_t line_control = 0;
  uint8_t modem_control = 0;
  uint8_t scratch = 0;
  bool overrun = false;
  bool transmit_pending = true;
};
