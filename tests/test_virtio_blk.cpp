#include "cpu.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <unistd.h>

namespace {

constexpr uint64_t DISK = 0x20000000;
constexpr uint64_t PLIC = 0x0c000000;
constexpr unsigned DISK_IRQ = 5;
constexpr uint64_t DESC = DRAM_BASE;
constexpr uint64_t AVAIL = DRAM_BASE + 0x100;
constexpr uint64_t USED = DRAM_BASE + 0x200;
constexpr uint64_t HEADER = DRAM_BASE + 0x300;
constexpr uint64_t STATUS = DRAM_BASE + 0x310;
constexpr uint64_t DATA1 = DRAM_BASE + 0x400;
constexpr uint64_t DATA2 = DRAM_BASE + 0x800;

void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

void descriptor(Bus &bus, unsigned index, uint64_t address, uint32_t length,
                uint16_t flags, uint16_t next = 0) {
  const uint64_t entry = DESC + 16 * index;
  bus.write(entry, DOUBLEWORD, address);
  bus.write(entry + 8, WORD, length);
  bus.write(entry + 12, HALFWORD, flags);
  bus.write(entry + 14, HALFWORD, next);
}

} // namespace

int main() {
  char image[] = "/tmp/riscv-virtio-blk-XXXXXX";
  const int fd = mkstemp(image);
  require(fd >= 0 && ftruncate(fd, 1024) == 0, "Could not create test disk");
  Bus bus(false, DRAM_BASE, DRAM_SIZE, {std::nullopt, PLIC, std::nullopt});
  bus.attach_disk(DISK, DISK_IRQ, image);
  unlink(image);

  auto reg = [&](unsigned offset) { return bus.read(DISK + offset, WORD); };
  auto set = [&](unsigned offset, uint32_t value) {
    bus.write(DISK + offset, WORD, value);
  };
  require(
      reg(0) == 0x74726976 && reg(4) == 2 && reg(8) == 2 && reg(0x100) == 2,
      "Virtio discovery must report a modern block device with two sectors");
  set(0x70, 11);
  require(!(reg(0x70) & 8), "Negotiation must require VERSION_1");
  set(0x70, 0);
  set(0x70, 3);
  set(0x20, 1 << 9); // FLUSH in the low feature word.
  set(0x24, 1);
  set(0x20, 1); // VERSION_1 in the high feature word.
  set(0x70, 11);
  require(reg(0x70) == 11, "The supported feature set must be accepted");
  set(0x38, 8);
  set(0x80, DESC);
  set(0x90, AVAIL);
  set(0xa0, USED);
  set(0x44, 1);
  set(0x70, 15);

  bus.write(PLIC + 4 * DISK_IRQ, WORD, 1);
  bus.write(PLIC + 0x2000, WORD, 1U << DISK_IRQ);

  auto request = [&](uint16_t index, uint32_t type, uint64_t sector,
                     uint16_t data_flags) {
    bus.write(HEADER, WORD, type);
    bus.write(HEADER + 8, DOUBLEWORD, sector);
    bus.write(STATUS, BYTE, 0xff);
    descriptor(bus, 0, HEADER, 16, 1, type == 4 ? 3 : 1);
    descriptor(bus, 1, DATA1, 256, data_flags | 1, 2);
    descriptor(bus, 2, DATA2, 256, data_flags | 1, 3);
    descriptor(bus, 3, STATUS, 1, 2);
    bus.write(AVAIL + 4 + 2 * (index - 1), HALFWORD, 0);
    bus.write(AVAIL + 2, HALFWORD, index);
    set(0x50, 0);
    require(bus.read(USED + 2, HALFWORD) == index,
            "The device must complete each request on the used ring");
    return bus.read(STATUS, BYTE);
  };

  std::fill_n(bus.dram.begin() + (DATA1 - DRAM_BASE), 256, 0x5a);
  std::fill_n(bus.dram.begin() + (DATA2 - DRAM_BASE), 256, 0xa5);
  require(request(1, 1, 1, 0) == 0 && bus.read(USED + 8, WORD) == 1,
          "A write must complete and report its one-byte status");
  require(reg(0x60) == 1 && bus.read(PLIC + 0x200004, WORD) == DISK_IRQ,
          "A completion must assert the configured PLIC interrupt");
  set(0x64, 1);
  bus.write(PLIC + 0x200004, WORD, DISK_IRQ);
  require(reg(0x60) == 0 && bus.read(PLIC + 0x200004, WORD) == 0,
          "Acknowledgment must deassert the interrupt");

  require(request(2, 4, 0, 0) == 0, "A negotiated flush must complete");
  std::array<uint8_t, 512> disk_data{};
  require(pread(fd, disk_data.data(), disk_data.size(), 512) == 512 &&
              disk_data.front() == 0x5a && disk_data.back() == 0xa5,
          "The write must be visible in the backing file");
  close(fd);

  bus.write(DATA1, BYTE, 0);
  bus.write(DATA2, BYTE, 0);
  require(request(3, 0, 1, 2) == 0 && bus.read(DATA1, BYTE) == 0x5a &&
              bus.read(DATA2, BYTE) == 0xa5 && bus.read(USED + 24, WORD) == 513,
          "A read must fill both data descriptors and report its used length");
  require(request(4, 1, 2, 0) == 1,
          "A request beyond capacity must fail with an I/O error");
  require(request(5, 8, 0, 2) == 2,
          "An unsupported operation must return the unsupported status");

  set(0x64, 1);
  descriptor(bus, 0, HEADER, 16, 1, 0); // A cyclic descriptor chain.
  bus.write(AVAIL + 14, HALFWORD, 0);
  bus.write(AVAIL + 2, HALFWORD, 6);
  set(0x50, 0);
  require((reg(0x70) & 64) && reg(0x60) == 2 &&
              bus.read(USED + 2, HALFWORD) == 5,
          "A malformed queue must signal a reset without completing the chain");
  set(0x70, 0);
  require(reg(0x70) == 0 && reg(0x60) == 0 && reg(0x44) == 0,
          "A device reset must clear the queue and interrupt");
}
