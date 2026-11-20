#pragma once

#include <array>
#include <cstdint>

class Plic {
public:
  static constexpr uint64_t MMIO_SIZE = 0x04000000;
  static constexpr unsigned SOURCE_COUNT = 32;

  explicit Plic(uint64_t base) : base(base) {}
  bool contains_access(uint64_t address, uint8_t size) const;
  uint64_t read(uint64_t address, uint8_t size);
  void write(uint64_t address, uint8_t size, uint64_t value);
  uint64_t pending_interrupts() const;
  void set_external_level(unsigned source, bool asserted);

private:
  struct Context {
    uint32_t enabled = 0;
    uint32_t threshold = 0;
  };

  unsigned select_interrupt(const Context &context, uint32_t threshold) const;
  uint32_t read_plic(uint64_t offset);
  void write_plic(uint64_t offset, uint32_t value);

  uint64_t base;
  std::array<uint32_t, SOURCE_COUNT> priority{};
  std::array<Context, 2> contexts{};
  uint32_t asserted_sources = 0;
  uint32_t pending_sources = 0;
  uint32_t claimed_sources = 0;
};
