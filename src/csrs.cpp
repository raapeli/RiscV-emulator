#include "csrs.h"
#include <algorithm>
#include <bit>
#include <cstdint>

namespace csr {

constexpr uint64_t STATUS_XLEN_MASK = (3ULL << 34) | Mask::UXL;
constexpr uint64_t STATUS_XLEN_64 = (2ULL << 34) | (2ULL << 32);
constexpr uint64_t MISA_ALWAYS_ENABLED = Misa::RV32I_64I_128I | Misa::M_EXT |
                                         Misa::A_EXT | Misa::C_EXT |
                                         Misa::F_EXT | Misa::D_EXT;
constexpr uint64_t PHYSICAL_LIMIT = 1ULL << 56;
constexpr uint64_t PMP_ADDRESS_MASK = (PHYSICAL_LIMIT >> 2) - 1;
constexpr uint8_t PMP_MODE_MASK = 0x18;
constexpr uint8_t PMP_TOR = 0x08;
constexpr uint8_t PMP_NA4 = 0x10;
constexpr uint8_t PMP_NAPOT = 0x18;
constexpr uint8_t PMP_LOCK = 0x80;

Csr::Csr(uint64_t device_mask) : device_mask(device_mask) {
  regs[Address::MISA] =
      Misa::XLEN_64 | MISA_ALWAYS_ENABLED | Misa::SUPERVISOR | Misa::USER;
  regs[Address::MSTATUS] = STATUS_XLEN_64;
};

static bool is_zero_csr(uint64_t addr) {
  // These configuration registers and hardware performance monitors have no
  // writable fields on this hart.
  return addr == Address::MCOUNTINHIBIT || addr == Address::MENVCFG ||
         addr == Address::SENVCFG ||
         (addr >= Address::HPMCOUNTER3 && addr <= Address::HPMCOUNTER31) ||
         (addr >= Address::MHPMCOUNTER3 && addr <= Address::MHPMCOUNTER31) ||
         (addr >= Address::MHPMEVENT3 && addr <= Address::MHPMEVENT31);
}

bool Csr::is_implemented(uint64_t addr) {
  switch (addr) {
  case Address::FFLAGS:
  case Address::FRM:
  case Address::FCSR:
  case Address::CYCLE:
  case Address::TIME:
  case Address::INSTRET:
  case Address::SSTATUS:
  case Address::SIE:
  case Address::STVEC:
  case Address::SCOUNTEREN:
  case Address::SSCRATCH:
  case Address::SEPC:
  case Address::SCAUSE:
  case Address::STVAL:
  case Address::SIP:
  case Address::SATP:
  case Address::MVENDORID:
  case Address::MARCHID:
  case Address::MIMPID:
  case Address::MHARTID:
  case Address::MCONFIGPTR:
  case Address::MSTATUS:
  case Address::MISA:
  case Address::MEDELEG:
  case Address::MIDELEG:
  case Address::MIE:
  case Address::MTVEC:
  case Address::MCOUNTEREN:
  case Address::MSCRATCH:
  case Address::MEPC:
  case Address::MCAUSE:
  case Address::MTVAL:
  case Address::MIP:
  case Address::MCYCLE:
  case Address::MINSTRET:
  case Address::TSELECT:
  case Address::TDATA1:
  case Address::TDATA2:
  case Address::TDATA3:
  case Address::TINFO:
    return true;
  default:
    return is_zero_csr(addr) ||
           (addr >= Address::PMPCFG0 && addr <= Address::PMPCFG15 &&
            !(addr & 1)) ||
           (addr >= Address::PMPADDR0 && addr <= Address::PMPADDR63);
  }
}

uint64_t Csr::load(uint64_t addr) {
  if (is_zero_csr(addr))
    return 0;

  switch (addr) {
  case Address::FFLAGS:
    return regs[Address::FCSR] & 0x1f;
  case Address::FRM:
    return regs[Address::FCSR] >> 5;
  case Address::SSTATUS:
    return regs[Address::MSTATUS] & Mask::SSTATUS;
  case Address::SIE:
    return regs[Address::MIE] & regs[Address::MIDELEG];
  case Address::SIP:
    return load(Address::MIP) & regs[Address::MIDELEG];
  case Address::MIP:
    return regs[Address::MIP] | platform_pending;
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

uint64_t Csr::value_for_update(uint64_t addr, uint64_t observed) const {
  // Reads include the device signal. CSR updates use the software pending bits.
  return addr == Address::MIP ? regs[addr] : observed;
}

void Csr::set_platform_pending(uint64_t pending) {
  platform_pending = pending & device_mask;
}

void Csr::write(uint64_t addr, uint64_t value) {
  if (is_zero_csr(addr))
    return;

  switch (addr) {
  case Address::FFLAGS:
    write(Address::FCSR, (regs[Address::FCSR] & 0xe0) | (value & 0x1f));
    break;
  case Address::FRM:
    write(Address::FCSR, ((value & 7) << 5) | (regs[Address::FCSR] & 0x1f));
    break;
  case Address::FCSR:
    regs[addr] = value & 0xff;
    mark_floating_dirty();
    break;
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
    if (regs[addr] != updated) {
      regs[addr] = updated;
      rebuild_pmp_cache();
    }
    break;
  }
  case Address::MISA:
    // The implemented ISA and privilege modes are read-only.
    break;
  case Address::MIDELEG:
    regs[Address::MIDELEG] = value & Mask::SUPERVISOR_INTERRUPTS;
    break;
  case Address::MSTATUS: {
    const bool dirty =
        (value & Mask::FS) == Mask::FS || (value & Mask::XS) == Mask::XS;
    regs[addr] = (value & ~(STATUS_XLEN_MASK | Mask::SD)) | STATUS_XLEN_64 |
                 (dirty ? static_cast<uint64_t>(Mask::SD) : 0);
    break;
  }
  case Address::SSTATUS:
    write(Address::MSTATUS,
          (regs[Address::MSTATUS] & ~Mask::SSTATUS) | (value & Mask::SSTATUS));
    break;
  case Address::SIE: {
    uint64_t val = (regs[Address::MIE] & ~regs[Address::MIDELEG]) |
                   (value & regs[Address::MIDELEG]);
    regs[Address::MIE] = val;
    break;
  }
  case Address::SIP: {
    uint64_t writable = regs[Address::MIDELEG];
    if (device_mask)
      writable &= Mask::SSIP;
    uint64_t val = (regs[Address::MIP] & ~writable) | (value & writable);
    regs[Address::MIP] = val;
    break;
  }
  case Address::MIP:
    value &= ~(device_mask & (Mask::MEIP | Mask::MSIP | Mask::MTIP));
    regs[addr] = value;
    break;
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
      if (regs[addr] != value) {
        regs[addr] = value;
        rebuild_pmp_cache();
      }
      break;
    }
    regs[addr] = value;
  }
}

bool Csr::floating_enabled() const {
  return (regs[Address::MSTATUS] & Mask::FS) != 0;
}

void Csr::mark_floating_dirty() {
  write(Address::MSTATUS, regs[Address::MSTATUS] | Mask::FS);
}

void Csr::rebuild_pmp_cache() {
  active_pmp_count = 0;

  for (unsigned i = 0; i < PMP_ENTRIES; ++i) {
    const uint8_t config = pmp_config(i);
    const uint64_t encoded = regs[Address::PMPADDR0 + i];

    uint64_t start;
    uint64_t end;

    switch (config & PMP_MODE_MASK) {
    case PMP_TOR:
      start = i == 0 ? 0 : regs[Address::PMPADDR0 + i - 1] << 2;
      end = encoded << 2;
      break;
    case PMP_NA4:
      start = encoded << 2;
      end = start + 4;
      break;

    case PMP_NAPOT: {
      // PMPADDR holds at most 54 bits, so even the largest region fits here.
      const unsigned ones = std::countr_one(encoded);
      start = (encoded & (UINT64_MAX << ones)) << 2;
      end = std::min(start + (uint64_t{8} << ones), PHYSICAL_LIMIT);
      break;
    }
    default:
      continue;
    }
    if (start < end)
      pmp_cache[active_pmp_count++] = {start, end, config};
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

  for (unsigned i = 0; i < active_pmp_count; ++i) {
    const auto &region = pmp_cache[i];

    if (address >= region.end || last < region.start)
      continue;
    if (address < region.start || last >= region.end)
      return false;
    return (machine && !(region.config & PMP_LOCK)) ||
           (region.config & permissions) == permissions;
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
