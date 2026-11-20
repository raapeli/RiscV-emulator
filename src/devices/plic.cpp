#include "devices/plic.h"
#include "cpu.h"

namespace {

constexpr uint64_t PLIC_PENDING = 0x1000;
constexpr uint64_t PLIC_ENABLE = 0x2000;
constexpr uint64_t PLIC_ENABLE_STRIDE = 0x80;
constexpr uint64_t PLIC_CONTEXT = 0x200000;
constexpr uint64_t PLIC_CONTEXT_STRIDE = 0x1000;

} // namespace

bool Plic::contains_access(uint64_t address, uint8_t size) const {
  const uint64_t offset = address - base;
  return size == WORD && address >= base && (offset & 3) == 0 &&
         offset < MMIO_SIZE;
}

uint64_t Plic::read(uint64_t address, uint8_t size) {
  return contains_access(address, size) ? read_plic(address - base) : UINT64_MAX;
}

void Plic::write(uint64_t address, uint8_t size, uint64_t value) {
  if (contains_access(address, size))
    write_plic(address - base, value);
}

void Plic::set_external_level(unsigned source, bool asserted) {
  if (source == 0 || source >= SOURCE_COUNT)
    return;
  const uint32_t bit = 1U << source;
  if (asserted) {
    asserted_sources |= bit;
    if (!(claimed_sources & bit))
      pending_sources |= bit;
  } else {
    asserted_sources &= ~bit;
  }
}

unsigned Plic::select_interrupt(const Context &context,
                                uint32_t threshold) const {
  const uint32_t eligible = pending_sources & context.enabled;
  if (!eligible)
    return 0;
  unsigned selected = 0;
  for (unsigned source = 1; source < SOURCE_COUNT; ++source) {
    if ((eligible & (1U << source)) && priority[source] > threshold) {
      selected = source;
      threshold = priority[source];
    }
  }
  return selected;
}

uint64_t Plic::pending_interrupts() const {
  using csr::Mask;
  uint64_t pending = 0;
  if (select_interrupt(contexts[0], contexts[0].threshold))
    pending |= Mask::MEIP;
  if (select_interrupt(contexts[1], contexts[1].threshold))
    pending |= Mask::SEIP;
  return pending;
}

uint32_t Plic::read_plic(uint64_t offset) {
  if (offset / 4 < SOURCE_COUNT)
    return priority[offset / 4];
  if (offset == PLIC_PENDING)
    return pending_sources;

  for (unsigned i = 0; i < contexts.size(); ++i) {
    auto &context = contexts[i];
    const uint64_t control = PLIC_CONTEXT + i * PLIC_CONTEXT_STRIDE;
    if (offset == PLIC_ENABLE + i * PLIC_ENABLE_STRIDE)
      return context.enabled;
    if (offset == control)
      return context.threshold;
    if (offset == control + 4) {
      // The threshold gates notifications, while a claim can also poll.
      const unsigned source = select_interrupt(context, 0);
      if (source) {
        pending_sources &= ~(1U << source);
        claimed_sources |= 1U << source;
      }
      return source;
    }
  }
  return 0;
}

void Plic::write_plic(uint64_t offset, uint32_t value) {
  if (offset / 4 < SOURCE_COUNT) {
    if (offset != 0)
      priority[offset / 4] = value & 7;
    return;
  }

  for (unsigned i = 0; i < contexts.size(); ++i) {
    auto &context = contexts[i];
    const uint64_t control = PLIC_CONTEXT + i * PLIC_CONTEXT_STRIDE;
    if (offset == PLIC_ENABLE + i * PLIC_ENABLE_STRIDE)
      context.enabled = value & ~1U;
    if (offset == control)
      context.threshold = value & 7;
    if (offset == control + 4 && value > 0 && value < SOURCE_COUNT) {
      const uint32_t bit = 1U << value;
      if ((context.enabled & claimed_sources) & bit) {
        claimed_sources &= ~bit;
        pending_sources |= asserted_sources & bit;
      }
    }
  }
}
