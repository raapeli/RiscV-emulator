#include "devices/clint.h"
#include "cpu.h"

namespace {

constexpr uint64_t MSIP = 0;
constexpr uint64_t MTIMECMP = 0x4000;
constexpr uint64_t MTIME = 0xbff8;

uint64_t read_timer(uint64_t value, uint64_t offset, uint8_t size) {
  return size == DOUBLEWORD ? value : static_cast<uint32_t>(value >> (offset * 8));
}

void write_timer(uint64_t &reg, uint64_t offset, uint8_t size, uint64_t value) {
  if (size == DOUBLEWORD) {
    reg = value;
  } else {
    const unsigned shift = offset * 8;
    const uint64_t mask = uint64_t{UINT32_MAX} << shift;
    reg = (reg & ~mask) | ((value << shift) & mask);
  }
}

} // namespace

bool Clint::contains_access(uint64_t address, uint8_t size) const {
  if (address < base)
    return false;
  const uint64_t offset = address - base;
  if (size == DOUBLEWORD)
    return offset == MTIME || offset == MTIMECMP;
  if (size != WORD)
    return false;
  return offset == MSIP || offset == MTIME || offset == MTIME + 4 ||
         offset == MTIMECMP || offset == MTIMECMP + 4;
}

uint64_t Clint::read(uint64_t address, uint8_t size) const {
  if (!contains_access(address, size))
    return UINT64_MAX;
  const uint64_t offset = address - base;
  if (offset == MSIP)
    return msip;
  if (offset >= MTIME)
    return read_timer(mtime, offset - MTIME, size);
  return read_timer(mtimecmp, offset - MTIMECMP, size);
}

void Clint::write(uint64_t address, uint8_t size, uint64_t value) {
  if (!contains_access(address, size))
    return;
  const uint64_t offset = address - base;
  if (offset == MSIP)
    msip = value & 1;
  else if (offset >= MTIME)
    write_timer(mtime, offset - MTIME, size, value);
  else
    write_timer(mtimecmp, offset - MTIMECMP, size, value);
}

uint64_t Clint::pending_interrupts() const {
  return (msip ? uint64_t{csr::Mask::MSIP} : 0) |
         (mtime >= mtimecmp ? uint64_t{csr::Mask::MTIP} : 0);
}
