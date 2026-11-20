#include "devices/uart.h"
#include "cpu.h"

#include <algorithm>
#include <array>

namespace {

enum Register : uint8_t {
  DATA, INTERRUPT_ENABLE, INTERRUPT_ID_OR_FIFO_CONTROL, LINE_CONTROL,
  MODEM_CONTROL, LINE_STATUS, MODEM_STATUS, SCRATCH,
};

constexpr uint8_t DIVISOR_LATCH_ACCESS = 0x80;
constexpr uint8_t FIFO_ENABLE = 0x01;
constexpr uint8_t FIFO_RESET_RX = 0x02;
constexpr uint8_t FIFO_RESET_TX = 0x04;
constexpr uint8_t MODEM_LOOPBACK = 0x10;
constexpr uint8_t RX_READY = 0x01;
constexpr uint8_t RX_OVERRUN = 0x02;
constexpr uint8_t TX_EMPTY = 0x60;
constexpr uint8_t ENABLE_RX_INTERRUPT = 0x01;
constexpr uint8_t ENABLE_TX_INTERRUPT = 0x02;
constexpr uint8_t ENABLE_LINE_INTERRUPT = 0x04;
constexpr uint8_t NO_INTERRUPT = 0x01;
constexpr uint8_t TX_INTERRUPT = 0x02;
constexpr uint8_t RX_INTERRUPT = 0x04;
constexpr uint8_t LINE_INTERRUPT = 0x06;
constexpr uint8_t RX_TIMEOUT_INTERRUPT = 0x0c;
constexpr std::array RX_TRIGGER_LEVELS{1U, 4U, 8U, 14U};

} // namespace

bool Uart::contains_access(uint64_t address, uint8_t size) const {
  return size == BYTE && address >= base && address - base <= SCRATCH;
}

unsigned Uart::receive_capacity() const {
  return (fifo_control & FIFO_ENABLE) ? 16 : 1;
}

bool Uart::can_receive() const {
  return !(modem_control & MODEM_LOOPBACK) &&
         receive_buffer.size() < receive_capacity();
}

void Uart::reset_receive_timeout() {
  // Approximate four 10-bit characters at 16 input clocks per bit.
  receive_timeout = 640U * std::max<unsigned>(divisor, 1);
}

void Uart::receive(uint8_t byte) {
  if (receive_buffer.size() == receive_capacity()) {
    overrun = true;
    return;
  }
  receive_buffer.push_back(byte);
  reset_receive_timeout();
}

void Uart::tick() {
  if (receive_timeout)
    --receive_timeout;
}

uint8_t Uart::interrupt_id() const {
  if ((interrupt_enable & ENABLE_LINE_INTERRUPT) && overrun)
    return LINE_INTERRUPT;
  if ((interrupt_enable & ENABLE_RX_INTERRUPT) && !receive_buffer.empty()) {
    if (!(fifo_control & FIFO_ENABLE) ||
        receive_buffer.size() >= RX_TRIGGER_LEVELS[fifo_control >> 6])
      return RX_INTERRUPT;
    if (receive_timeout == 0)
      return RX_TIMEOUT_INTERRUPT;
  }
  if ((interrupt_enable & ENABLE_TX_INTERRUPT) && transmit_pending)
    return TX_INTERRUPT;
  return NO_INTERRUPT;
}

bool Uart::interrupt_pending() const { return interrupt_id() != NO_INTERRUPT; }

uint64_t Uart::read(uint64_t address, uint8_t size) {
  if (!contains_access(address, size))
    return UINT64_MAX;

  switch (address - base) {
  case DATA: {
    if (line_control & DIVISOR_LATCH_ACCESS)
      return divisor & 0xff;
    if (receive_buffer.empty())
      return 0;
    const uint8_t byte = receive_buffer.front();
    receive_buffer.pop_front();
    reset_receive_timeout();
    return byte;
  }
  case INTERRUPT_ENABLE:
    return (line_control & DIVISOR_LATCH_ACCESS) ? divisor >> 8
                                                 : interrupt_enable;
  case INTERRUPT_ID_OR_FIFO_CONTROL: {
    const uint8_t id = interrupt_id();
    if (id == TX_INTERRUPT)
      transmit_pending = false;
    return id | ((fifo_control & FIFO_ENABLE) ? 0xc0 : 0);
  }
  case LINE_CONTROL:
    return line_control;
  case MODEM_CONTROL:
    return modem_control;
  case LINE_STATUS: {
    const uint8_t status = TX_EMPTY | (receive_buffer.empty() ? 0 : RX_READY) |
                           (overrun ? RX_OVERRUN : 0);
    overrun = false;
    return status;
  }
  case MODEM_STATUS:
    if (modem_control & MODEM_LOOPBACK)
      // Loop RTS, DTR, OUT1 and OUT2 back to CTS, DSR, RI and DCD.
      return ((modem_control & 1) << 5) | ((modem_control & 2) << 3) |
             ((modem_control & 0x0c) << 4);
    return 0xb0; // CTS, DSR and DCD asserted for the local console.
  case SCRATCH:
    return scratch;
  }
  return UINT64_MAX;
}

std::optional<uint8_t> Uart::write(uint64_t address, uint8_t size,
                                   uint64_t value) {
  if (!contains_access(address, size))
    return std::nullopt;

  const auto byte = static_cast<uint8_t>(value);
  switch (address - base) {
  case DATA:
    if (line_control & DIVISOR_LATCH_ACCESS) {
      divisor = (divisor & 0xff00) | byte;
      break;
    }
    transmit_pending = true;
    if (!(modem_control & MODEM_LOOPBACK))
      return byte;
    receive(byte);
    break;
  case INTERRUPT_ENABLE:
    if (line_control & DIVISOR_LATCH_ACCESS) {
      divisor = (divisor & 0xff) | (byte << 8);
    } else {
      if ((byte & ENABLE_TX_INTERRUPT) &&
          !(interrupt_enable & ENABLE_TX_INTERRUPT))
        transmit_pending = true;
      interrupt_enable = byte & 0x0f;
    }
    break;
  case INTERRUPT_ID_OR_FIFO_CONTROL:
    if (((fifo_control ^ byte) & FIFO_ENABLE) || (byte & FIFO_RESET_RX))
      receive_buffer.clear();
    if (byte & FIFO_RESET_TX)
      transmit_pending = true;
    fifo_control = byte & 0xc1;
    break;
  case LINE_CONTROL:
    line_control = byte;
    break;
  case MODEM_CONTROL:
    modem_control = byte & 0x1f;
    break;
  case SCRATCH:
    scratch = byte;
    break;
  }
  return std::nullopt;
}
