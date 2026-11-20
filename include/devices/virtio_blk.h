#pragma once

#include <cstdint>
#include <span>

class VirtioBlk {
public:
  static constexpr uint64_t MMIO_SIZE = 0x1000;

  VirtioBlk(uint64_t base, const char *image);
  ~VirtioBlk();
  VirtioBlk(const VirtioBlk &) = delete;
  VirtioBlk &operator=(const VirtioBlk &) = delete;

  bool contains_access(uint64_t address, uint8_t size) const;
  uint64_t read(uint64_t address, uint8_t size) const;
  void write(uint64_t address, uint8_t size, uint64_t value, uint64_t ram_base,
             std::span<uint8_t> ram);
  bool interrupt_pending() const { return interrupts != 0; }

private:
  struct Queue {
    uint64_t descriptors = 0;
    uint64_t available = 0;
    uint64_t used = 0;
    uint32_t size = 0;
    uint16_t last_available = 0;
    bool ready = false;
  };

  void notify(uint64_t ram_base, std::span<uint8_t> ram);
  void fail_queue();

  uint64_t base;
  int fd;
  uint64_t capacity;
  uint64_t driver_features = 0;
  uint32_t device_features_select = 0;
  uint32_t driver_features_select = 0;
  uint32_t queue_select = 0;
  uint32_t interrupts = 0;
  uint32_t status = 0;
  Queue queue;
};
