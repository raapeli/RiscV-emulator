#include "cpu.h"

namespace {

constexpr unsigned PAGE_SHIFT = 12;
constexpr unsigned VPN_BITS = 9;
constexpr uint64_t PAGE_SIZE = 1ULL << PAGE_SHIFT;
constexpr uint64_t PAGE_MASK = PAGE_SIZE - 1;
constexpr uint64_t VPN_MASK = (1ULL << VPN_BITS) - 1;
constexpr uint64_t PTE_VALID = 1ULL << 0;
constexpr uint64_t PTE_USER = 1ULL << 4;
constexpr uint64_t PTE_ACCESSED = 1ULL << 6;
constexpr uint64_t PTE_DIRTY = 1ULL << 7;

bool permits_page_access(uint8_t flags, uint8_t permissions, Mode mode,
                         uint64_t status) {
  using csr::Csr;
  const bool user_page = flags & PTE_USER;
  if ((mode == Mode::USER && !user_page) ||
      (mode == Mode::SUPERVISOR && user_page &&
       (permissions == Csr::PMP_EXECUTE || !(status & csr::Mask::SUM))))
    return false;

  uint8_t allowed = (flags >> 1) & 7;
  if ((allowed & Csr::PMP_EXECUTE) && (status & csr::Mask::MXR))
    allowed |= Csr::PMP_READ;
  return (allowed & permissions) == permissions;
}

} // namespace

void Cpu::flush_tlb() {
  // Only Sv39 translations are cached, so a zero satp marks an empty entry.
  for (auto &entry : tlb)
    entry.satp = 0;
}

bool Cpu::permits_physical_access(uint64_t addr, uint8_t size,
                                  uint8_t permissions,
                                  Mode effective_mode) const {
  using csr::Csr;
  bool mapped = bus->contains_ram_access(addr, size) ||
                (permissions == Csr::PMP_READ &&
                 bus->contains_mmio_access(addr, size, false)) ||
                (permissions == Csr::PMP_WRITE &&
                 bus->contains_mmio_access(addr, size, true));
  return mapped && cregs->permits_access(addr, size / 8, permissions,
                                         effective_mode == Mode::MACHINE);
}

Cpu::AddressResult Cpu::resolve_memory_access(uint64_t addr, uint8_t size,
                                              uint8_t permissions) {
  using csr::Csr;
  exception.access_address = addr;
  const bool fetch = permissions == Csr::PMP_EXECUTE;
  const bool store = permissions & Csr::PMP_WRITE;
  const auto access_fault = fetch   ? Exception::InstructionAccessFault
                            : store ? Exception::StoreAmoAccessFault
                                    : Exception::LoadAccessFault;
  const auto page_fault = fetch   ? Exception::InstructionPageFault
                          : store ? Exception::StoreAmoPageFault
                                  : Exception::LoadPageFault;

  const uint64_t status = cregs->load(csr::Address::MSTATUS);
  Mode effective_mode = mode;
  if (mode == Mode::MACHINE && !fetch &&
      (status & (1ULL << static_cast<unsigned>(csr::Mask::MSTATUSBit::MPRV)))) {
    effective_mode = static_cast<Mode>(
        cregs->read_bits(csr::Address::MSTATUS, csr::Mask::MPP_HIGH_BIT,
                         csr::Mask::MPP_LOW_BIT));
  }

  const uint64_t satp = cregs->load(csr::Address::SATP);
  if (effective_mode == Mode::MACHINE ||
      (satp >> csr::Satp::MODE_SHIFT) == csr::Satp::BARE) {
    if (permits_physical_access(addr, size, permissions, effective_mode))
      return addr;
    return std::unexpected(access_fault);
  }

  const uint64_t sign_extension = (addr & (1ULL << 38)) ? (1ULL << 25) - 1 : 0;
  if ((addr >> 39) != sign_extension)
    return std::unexpected(page_fault);

  // A naturally aligned access fits in one page. Reject split accesses.
  const uint64_t page_offset = addr & PAGE_MASK;
  if (size / 8 > PAGE_SIZE - page_offset)
    return std::unexpected(fetch   ? Exception::InstructionAddressMisaligned
                           : store ? Exception::StoreAmoAddressMisaligned
                                   : Exception::LoadAddressMisaligned);

  const uint64_t pmp_revision = cregs->pmp_revision();
  if (tlb_pmp_revision != pmp_revision) {
    // PMP also governs accesses to the page tables skipped on a hit.
    flush_tlb();
    tlb_pmp_revision = pmp_revision;
  }

  const uint64_t vpn = addr >> PAGE_SHIFT;
  const uint64_t slot =
      (vpn ^ (vpn >> VPN_BITS) ^ (vpn >> (2 * VPN_BITS))) % tlb.size();
  auto &cached = tlb[slot];
  if (cached.satp == satp && cached.virtual_page == vpn) {
    if (!permits_page_access(cached.flags, permissions, effective_mode, status))
      return std::unexpected(page_fault);

    // A write to a clean page must update the leaf PTE through the walker.
    if (!store || (cached.flags & PTE_DIRTY)) {
      const uint64_t physical = cached.physical_page | page_offset;
      if (permits_physical_access(physical, size, permissions, effective_mode))
        return physical;
      return std::unexpected(access_fault);
    }
  }

  uint64_t table = (satp & csr::Satp::PPN_MASK) << PAGE_SHIFT;
  for (int level = 2; level >= 0; --level) {
    const unsigned shift = PAGE_SHIFT + VPN_BITS * level;
    const uint64_t index = (addr >> shift) & VPN_MASK;
    const uint64_t entry_address = table + index * 8;
    if (!bus->contains_ram_access(entry_address, DOUBLEWORD) ||
        !cregs->permits_access(entry_address, 8, Csr::PMP_READ, false))
      return std::unexpected(access_fault);

    const uint64_t entry = bus->read(entry_address, DOUBLEWORD);
    const uint8_t allowed = (entry >> 1) & 7;
    if (!(entry & PTE_VALID) || (entry >> 54) != 0 ||
        ((allowed & Csr::PMP_WRITE) && !(allowed & Csr::PMP_READ)))
      return std::unexpected(page_fault);

    const uint64_t page = ((entry >> 10) & csr::Satp::PPN_MASK) << PAGE_SHIFT;
    if (!(allowed & (Csr::PMP_READ | Csr::PMP_EXECUTE))) {
      if (entry & (PTE_USER | PTE_ACCESSED | PTE_DIRTY))
        return std::unexpected(page_fault);
      table = page;
      continue;
    }

    const uint64_t offset_mask = (1ULL << shift) - 1;
    if ((page & offset_mask) ||
        !permits_page_access(entry, permissions, effective_mode, status))
      return std::unexpected(page_fault);

    const uint64_t physical = page | (addr & offset_mask);
    if (!permits_physical_access(physical, size, permissions, effective_mode))
      return std::unexpected(access_fault);

    const uint64_t updated = entry | PTE_ACCESSED | (store ? PTE_DIRTY : 0);
    if (updated != entry) {
      if (!cregs->permits_access(entry_address, 8, Csr::PMP_WRITE, false))
        return std::unexpected(access_fault);
      // Page tables and guest execution share one hart, so this write is
      // atomic.
      bus->write(entry_address, DOUBLEWORD, updated);
    }
    // A superpage also contributes a single 4 KiB slice to the cache.
    cached = {satp, vpn, physical & ~PAGE_MASK, static_cast<uint8_t>(updated)};
    return physical;
  }
  return std::unexpected(page_fault);
}
