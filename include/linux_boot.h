#pragma once

#include "bus.h"

#include <cstdint>
#include <vector>

namespace linux_boot {

inline constexpr uint64_t RAM_BASE = 0x80000000;
inline constexpr uint64_t RAM_SIZE = 128 * 1024 * 1024;
inline constexpr uint64_t KERNEL_ADDRESS = 0x80200000;
inline constexpr uint64_t INITRD_ADDRESS = 0x84000000;
inline constexpr uint64_t DTB_ADDRESS = 0x87e00000;
inline constexpr uint64_t CLINT_BASE = 0x02000000;
inline constexpr uint64_t PLIC_BASE = 0x0c000000;
inline constexpr Bus::UartMapping UART{0x10000000, 10};
inline constexpr uint64_t DISK_BASE = 0x10001000;
inline constexpr unsigned DISK_IRQ = 1;
inline constexpr Bus::DeviceMap DEVICES{CLINT_BASE, PLIC_BASE, UART};

std::vector<uint8_t> make_dtb(uint64_t initrd_size, bool has_disk);

} // namespace linux_boot
