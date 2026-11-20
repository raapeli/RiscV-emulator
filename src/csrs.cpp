#include "csrs.h"
#include <algorithm>
#include <bit>

namespace csr {

constexpr uint64_t STATUS_XLEN_MASK = (3ULL << 34) | Mask::UXL;
constexpr uint64_t STATUS_XLEN_64 = (2ULL << 34) | (2ULL << 32);
constexpr unsigned PMP_ENTRIES = 16;
constexpr uint64_t PHYSICAL_LIMIT = 1ULL << 56;
constexpr uint64_t PMP_ADDRESS_MASK = (PHYSICAL_LIMIT >> 2) - 1;
constexpr uint8_t PMP_MODE_MASK = 0x18;
constexpr uint8_t PMP_TOR = 0x08;
constexpr uint8_t PMP_NA4 = 0x10;
constexpr uint8_t PMP_NAPOT = 0x18;
constexpr uint8_t PMP_LOCK = 0x80;

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
  case Address::PMPCFG0:
  case Address::PMPCFG2: {
    unsigned first = (addr - Address::PMPCFG0) * 4;
    uint64_t updated = regs[addr];
    for (unsigned i = 0; i < 8; ++i) {
      if (pmp_config(first + i) & PMP_LOCK)
        continue;
      unsigned shift = i * 8;
      uint64_t config = (value >> shift) & (PMP_LOCK | PMP_MODE_MASK | 7);
      if (!(config & PMP_READ))
        config &= ~uint64_t{PMP_WRITE};
      updated = (updated & ~(0xffULL << shift)) | (config << shift);
    }
    regs[addr] = updated;
    break;
  }
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
  case Address::SATP:
    if ((value >> Satp::MODE_SHIFT) == Satp::BARE)
      regs[addr] = 0;
    else if ((value >> Satp::MODE_SHIFT) == Satp::SV39)
      regs[addr] = value;
    break;
  default:
    if (addr >= Address::PMPCFG0 && addr <= Address::PMPCFG15)
      break;
    if (addr >= Address::PMPADDR0 && addr <= Address::PMPADDR63) {
      unsigned index = addr - Address::PMPADDR0;
      if (index >= PMP_ENTRIES || (pmp_config(index) & PMP_LOCK))
        break;
      if (index + 1 < PMP_ENTRIES &&
          (pmp_config(index + 1) & (PMP_LOCK | PMP_MODE_MASK)) ==
              (PMP_LOCK | PMP_TOR))
        break;
      value &= PMP_ADDRESS_MASK;
    }
    regs[addr] = value;
  }
}

uint8_t Csr::pmp_config(unsigned index) const {
  return regs[Address::PMPCFG0 + (index / 8) * 2] >> ((index % 8) * 8);
}

bool Csr::permits_access(uint64_t address, uint8_t bytes, uint8_t permissions,
                         bool machine) const {
  if (bytes == 0 || address > UINT64_MAX - (bytes - 1))
    return false;
  const uint64_t last = address + bytes - 1;

  for (unsigned index = 0; index < PMP_ENTRIES; ++index) {
    const uint8_t config = pmp_config(index);
    const uint64_t encoded = regs[Address::PMPADDR0 + index];
    uint64_t start;
    uint64_t end;
    switch (config & PMP_MODE_MASK) {
    case PMP_TOR:
      start = index == 0 ? 0 : regs[Address::PMPADDR0 + index - 1] << 2;
      end = encoded << 2;
      break;
    case PMP_NA4:
      start = encoded << 2;
      end = start + 4;
      break;
    case PMP_NAPOT: {
      unsigned ones = std::countr_one(encoded);
      start = (encoded & ~((1ULL << ones) - 1)) << 2;
      end = std::min(start + (uint64_t{1} << (ones + 3)), PHYSICAL_LIMIT);
      break;
    }
    default:
      continue;
    }
    if (start >= end || address >= end || last < start)
      continue;
    if (address < start || last >= end)
      return false;
    return (machine && !(config & PMP_LOCK)) ||
           (config & permissions) == permissions;
  }
  return machine;
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
