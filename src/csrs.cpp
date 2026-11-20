#include "csrs.h"

namespace csr {

constexpr uint64_t STATUS_XLEN_MASK = (3ULL << 34) | Mask::UXL;
constexpr uint64_t STATUS_XLEN_64 = (2ULL << 34) | (2ULL << 32);

Csr::Csr() {
  regs[Address::MISA] = Misa::XLEN_64 | Misa::SUPERVISOR | Misa::USER;
  regs[Address::MSTATUS] = STATUS_XLEN_64;
};

uint64_t Csr::load(uint64_t addr) {
  switch (addr) {
  case Address::SSTATUS:
    return regs[Address::MSTATUS] & Mask::SSTATUS;
  case Address::SIE:
    return regs[Address::MIE] & regs[Address::MIDELEG];
  case Address::SIP:
    return regs[Address::MIP] & regs[Address::MIDELEG];
  case Address::TSELECT:
    // A write of trigger index zero reads back a different index when empty.
    return 1;
  case Address::TDATA1:
  case Address::TDATA2:
  case Address::TDATA3:
    return 0;
  case Address::TINFO:
    return 1;
  default:
    return regs[addr];
  }
}

void Csr::write(uint64_t addr, uint64_t value) {
  switch (addr) {
  case Address::MISA:
    // Instruction alignment stays at four bytes until C is implemented.
    regs[Address::MISA] = value & ~Misa::C_EXT;
    break;
  case Address::MIDELEG:
    regs[Address::MIDELEG] = value & Mask::SUPERVISOR_INTERRUPTS;
    break;
  case Address::MSTATUS:
    regs[Address::MSTATUS] = (value & ~STATUS_XLEN_MASK) | STATUS_XLEN_64;
    break;
  case Address::SSTATUS:
    write(Address::MSTATUS, (regs[Address::MSTATUS] & ~Mask::SSTATUS) |
                                (value & Mask::SSTATUS));
    break;
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
  case Address::TSELECT:
  case Address::TDATA1:
  case Address::TDATA2:
  case Address::TDATA3:
  case Address::TINFO:
    break;
  default:
    regs[addr] = value;
  }
}

uint64_t Csr::read_bit(uint64_t addr, uint64_t offset) {
  return read_bits(addr, offset, offset);
}

uint64_t Csr::read_bits(uint64_t addr, uint64_t upper_offset,
                        uint64_t lower_offset) {
  if (upper_offset >= MXLEN || lower_offset > upper_offset)
    return 0;

  uint64_t width = upper_offset - lower_offset + 1;
  uint64_t mask = UINT64_MAX >> (MXLEN - width);
  return (load(addr) >> lower_offset) & mask;
}

void Csr::write_bit(uint64_t addr, uint64_t offset, uint64_t value) {
  write_bits(addr, offset, offset, value != 0);
}

void Csr::write_bits(uint64_t addr, uint64_t upper_offset,
                     uint64_t lower_offset, uint64_t value) {
  if (upper_offset >= MXLEN || lower_offset > upper_offset)
    return;

  uint64_t width = upper_offset - lower_offset + 1;
  uint64_t mask = (UINT64_MAX >> (MXLEN - width)) << lower_offset;
  write(addr, (load(addr) & ~mask) | ((value << lower_offset) & mask));
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
