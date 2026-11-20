#pragma once

#include <cstdint>

class Clint {
public:
  explicit Clint(uint64_t base) : base(base) {}
  bool contains_access(uint64_t address, uint8_t size) const;
  uint64_t read(uint64_t address, uint8_t size) const;
  void write(uint64_t address, uint8_t size, uint64_t value);
  void tick() { ++mtime; }
  uint64_t time() const { return mtime; }
  uint64_t pending_interrupts() const;

private:
  uint64_t base;
  uint64_t mtime = 0;
  uint64_t mtimecmp = UINT64_MAX;
  bool msip = false;
};
