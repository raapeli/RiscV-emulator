#include "csrs.h"
#include <cstdint>

namespace csr {

Csr::Csr() {
  regs[Address::MISA] = Misa::XLEN_64 | Misa::SUPERVISOR | Misa::USER;
};

uint64_t Csr::load(uint64_t addr) {
  switch (addr) {
  case Address::SSTATUS:
    return regs[Address::MSTATUS] & Mask::SSTATUS;
  case Address::SIE:
    return regs[Address::MIE] & Address::MIDELEG;
  case Address::SIP:
    return regs[Address::MIP] & Address::MIDELEG;
  default:
    return regs[addr];
  }
}

void Csr::store(uint64_t addr, uint64_t value) {
  switch (addr) {
  case Address::SSTATUS: {
    uint64_t val =
        (regs[Address::MSTATUS] & ~Mask::SSTATUS) | (value & Mask::SSTATUS);
    regs[Address::MSTATUS] = val;
    break;
  }
  case Address::SIE: {
    uint64_t val = (regs[Address::MIE] & ~regs[Address::MIDELEG]) |
                   (value & regs[Address::MIDELEG]);
    regs[Address::MIE] = val;
    break;
  }
  case Address::SIP: {
    uint64_t val = (regs[Address::MIP] & ~regs[Address::MIDELEG]) |
                   (value & regs[Address::MIDELEG]);
    regs[Address::MIP] = val;
    break;
  }
  default:
    regs[addr] = value;
  }
}

uint64_t Csr::read_bit(uint64_t addr, uint64_t offset) {
  if (offset >= MXLEN) {
    // TODO: How to handle?
    return 0;
  }

  return (load(addr) & (1 << offset)) != 0;
}

uint64_t Csr::read_bits(uint64_t addr, uint64_t upper_offset,
                        uint64_t lower_offset) {
  uint64_t mask = (1 << (upper_offset - lower_offset + 1)) - 1;
  return (load(addr) >> lower_offset) & mask;
}

void Csr::write_bit(uint64_t addr, uint64_t offset, uint64_t value) {
  (value == 0) ? store(addr, load(addr) | (1 << offset))
               : store(addr, load(addr) & ~(1 << offset));
}

void Csr::write_bits(uint64_t addr, uint64_t upper_offset,
                     uint64_t lower_offset, uint64_t value) {
  uint64_t mask = (1 << (upper_offset - lower_offset + 1)) - 1;
  uint64_t old_val = load(addr);
  mask <<= lower_offset;
  old_val <<= lower_offset;
  value &= ~mask;
  value |= old_val;

  store(addr, value);
}

uint64_t Csr::read_bit_mstatus(Mask::MSTATUSBit bit) {
  return read_bit(Address::MSTATUS, static_cast<uint64_t>(bit));
}

void Csr::write_bit_mstatus(Mask::MSTATUSBit bit, uint64_t value) {
  write_bit(Address::MSTATUS, static_cast<uint64_t>(bit), value);
}

uint64_t Csr::read_bit_sstatus(Mask::SSTATUSBit bit) {
  return read_bit(Address::SSTATUS, static_cast<uint64_t>(bit));
}

void Csr::write_bit_sstatus(Mask::SSTATUSBit bit, uint64_t value) {
  write_bit(Address::SSTATUS, static_cast<uint64_t>(bit), value);
}
} // namespace csr
