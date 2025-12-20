#include "csrs.h"
#include <cstdint>

namespace csr {

Csr::Csr() {
  regs[Address::MISA] =
      Misa::XLEN_32 | Misa::RV32E | Misa::SUPERVISOR | Misa::USER;
};

uint32_t Csr::load(uint32_t addr) {
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

void Csr::store(uint32_t addr, uint32_t value) {
  switch (addr) {
  case Address::SSTATUS: {
    uint32_t val =
        (regs[Address::MSTATUS] & ~Mask::SSTATUS) | (value & Mask::SSTATUS);
    regs[Address::MSTATUS] = val;
    break;
  }
  case Address::SIE: {
    uint32_t val = (regs[Address::MIE] & ~regs[Address::MIDELEG]) |
                   (value & regs[Address::MIDELEG]);
    regs[Address::MIE] = val;
    break;
  }
  case Address::SIP: {
    uint32_t val = (regs[Address::MIP] & ~regs[Address::MIDELEG]) |
                   (value & regs[Address::MIDELEG]);
    regs[Address::MIP] = val;
    break;
  }
  default:
    regs[addr] = value;
  }
}

uint32_t Csr::read_bit(uint32_t addr, uint32_t offset) {
  if (offset >= MXLEN) {
    // TODO: How to handle?
    return 0;
  }

  return (load(addr) & (1 << offset)) != 0;
}

uint32_t Csr::read_bits(uint32_t addr, uint32_t upper_offset,
                        uint32_t lower_offset) {
  uint32_t mask = (1 << (upper_offset - lower_offset + 1)) - 1;
  return (load(addr) >> lower_offset) & mask;
}

void Csr::write_bit(uint32_t addr, uint32_t offset, uint32_t value) {
  (value == 0) ? store(addr, load(addr) | (1 << offset))
               : store(addr, load(addr) & ~(1 << offset));
}

void Csr::write_bits(uint32_t addr, uint32_t upper_offset,
                     uint32_t lower_offset, uint32_t value) {
  uint32_t mask = (1 << (upper_offset - lower_offset + 1)) - 1;
  uint32_t old_val = load(addr);
  mask <<= lower_offset;
  old_val <<= lower_offset;
  value &= ~mask;
  value |= old_val;

  store(addr, value);
}

uint32_t Csr::read_bit_mstatus(Mask::MSTATUSBit bit) {
  return read_bit(Address::MSTATUS, static_cast<uint32_t>(bit));
}

void Csr::write_bit_mstatus(Mask::MSTATUSBit bit, uint32_t value) {
  write_bit(Address::MSTATUS, static_cast<uint32_t>(bit), value);
}

uint32_t Csr::read_bit_sstatus(Mask::SSTATUSBit bit) {
  return read_bit(Address::SSTATUS, static_cast<uint32_t>(bit));
}

void Csr::write_bit_sstatus(Mask::SSTATUSBit bit, uint32_t value) {
  write_bit(Address::SSTATUS, static_cast<uint32_t>(bit), value);
}
} // namespace csr
