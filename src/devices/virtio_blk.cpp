#include "devices/virtio_blk.h"
#include "cpu.h"

#include <array>
#include <bit>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <optional>
#include <stdexcept>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

namespace {

enum Register : uint64_t {
  MAGIC = 0x000,
  VERSION = 0x004,
  DEVICE_ID = 0x008,
  VENDOR_ID = 0x00c,
  DEVICE_FEATURES = 0x010,
  DEVICE_FEATURES_SELECT = 0x014,
  DRIVER_FEATURES = 0x020,
  DRIVER_FEATURES_SELECT = 0x024,
  QUEUE_SELECT = 0x030,
  QUEUE_NUM_MAX = 0x034,
  QUEUE_NUM = 0x038,
  QUEUE_READY = 0x044,
  QUEUE_NOTIFY = 0x050,
  INTERRUPT_STATUS = 0x060,
  INTERRUPT_ACK = 0x064,
  DEVICE_STATUS = 0x070,
  QUEUE_DESC_LOW = 0x080,
  QUEUE_DESC_HIGH = 0x084,
  QUEUE_AVAIL_LOW = 0x090,
  QUEUE_AVAIL_HIGH = 0x094,
  QUEUE_USED_LOW = 0x0a0,
  QUEUE_USED_HIGH = 0x0a4,
  CONFIG = 0x100,
};

constexpr uint64_t SECTOR_SIZE = 512;
constexpr unsigned QUEUE_MAX = 128;
constexpr unsigned DESC_SIZE = 16;
constexpr unsigned RING_HEADER_SIZE = 4;
constexpr uint16_t DESC_NEXT = 1;
constexpr uint16_t DESC_WRITE = 2;
constexpr uint16_t AVAIL_NO_INTERRUPT = 1;
constexpr uint32_t USED_INTERRUPT = 1;
constexpr uint32_t CONFIG_INTERRUPT = 2;
constexpr uint32_t DRIVER_OK = 4;
constexpr uint32_t FEATURES_OK = 8;
constexpr uint32_t NEEDS_RESET = 64;
constexpr uint32_t FAILED = 128;
constexpr uint64_t VERSION_1_FEATURE = uint64_t{1} << 32;
constexpr uint64_t FLUSH_FEATURE = uint64_t{1} << 9;
constexpr uint64_t DEVICE_FEATURE_BITS = VERSION_1_FEATURE | FLUSH_FEATURE;

constexpr uint32_t READ_REQUEST = 0;
constexpr uint32_t WRITE_REQUEST = 1;
constexpr uint32_t FLUSH_REQUEST = 4;
constexpr uint8_t STATUS_OK = 0;
constexpr uint8_t STATUS_IOERR = 1;
constexpr uint8_t STATUS_UNSUPPORTED = 2;

template <typename T> T load_le(const uint8_t *data) {
  T value;
  std::memcpy(&value, data, sizeof(value));
  if constexpr (std::endian::native == std::endian::big)
    value = std::byteswap(value);
  return value;
}

template <typename T> void store_le(uint8_t *data, T value) {
  if constexpr (std::endian::native == std::endian::big)
    value = std::byteswap(value);
  std::memcpy(data, &value, sizeof(value));
}

struct GuestRam {
  uint64_t base;
  std::span<uint8_t> ram;

  std::span<uint8_t> bytes(uint64_t address, uint64_t length) const {
    const uint64_t offset = address - base;
    if (address < base || offset > ram.size() || length > ram.size() - offset ||
        (length && address > UINT64_MAX - (length - 1)))
      return {};
    return ram.subspan(offset, length);
  }
};

struct Buffer {
  std::span<uint8_t> bytes;
  bool writable;
};

size_t transfer(int fd, std::span<uint8_t> bytes, uint64_t offset, bool write) {
  const size_t length = bytes.size();
  while (!bytes.empty()) {
    const ssize_t count = write ? pwrite(fd, bytes.data(), bytes.size(), offset)
                                : pread(fd, bytes.data(), bytes.size(), offset);
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0)
      break;
    bytes = bytes.subspan(count);
    offset += count;
  }
  return length - bytes.size();
}

std::optional<uint32_t> process_request(int fd, uint64_t capacity, bool flush,
                                        std::span<Buffer> chain) {
  if (chain.size() < 2 || chain.front().writable ||
      chain.front().bytes.size() != 16 || !chain.back().writable ||
      chain.back().bytes.size() != 1)
    return std::nullopt;

  const uint8_t *header = chain.front().bytes.data();
  const uint32_t type = load_le<uint32_t>(header);
  const uint64_t sector = load_le<uint64_t>(header + 8);
  uint8_t &result = chain.back().bytes.front();
  const auto data = chain.subspan(1, chain.size() - 2);

  if (type == FLUSH_REQUEST && flush) {
    int rc = -1;
    if (data.empty()) {
      do {
        rc = fsync(fd);
      } while (rc < 0 && errno == EINTR);
    }
    result = rc == 0 ? STATUS_OK : STATUS_IOERR;
    return 1;
  }
  if (type != READ_REQUEST && type != WRITE_REQUEST) {
    result = STATUS_UNSUPPORTED;
    return 1;
  }

  const bool write = type == WRITE_REQUEST;
  uint64_t length = 0;
  for (const auto &buffer : data) {
    if (buffer.writable == write) {
      result = STATUS_IOERR;
      return 1;
    }
    length += buffer.bytes.size();
  }
  if (sector >= capacity || length == 0 || length % SECTOR_SIZE != 0 ||
      length / SECTOR_SIZE > capacity - sector || length >= UINT32_MAX) {
    result = STATUS_IOERR;
    return 1;
  }

  uint64_t offset = sector * SECTOR_SIZE;
  uint32_t used_length = 1;
  for (const auto &buffer : data) {
    const auto transferred = transfer(fd, buffer.bytes, offset, write);
    if (!write)
      used_length += transferred;
    if (transferred != buffer.bytes.size()) {
      result = STATUS_IOERR;
      return used_length;
    }
    offset += buffer.bytes.size();
  }
  result = STATUS_OK;
  return used_length;
}

void write_half(uint64_t &address, uint32_t value, unsigned shift) {
  const uint64_t mask = uint64_t{UINT32_MAX} << shift;
  address = (address & ~mask) | (uint64_t{value} << shift);
}

} // namespace

VirtioBlk::VirtioBlk(uint64_t base, const char *image) : base(base) {
  fd = open(image, O_RDWR | O_CLOEXEC | O_NONBLOCK);
  if (fd < 0)
    throw std::system_error(errno, std::generic_category(), image);

  struct stat info{};
  if (fstat(fd, &info) != 0) {
    const int error = errno;
    close(fd);
    throw std::system_error(error, std::generic_category(), image);
  }
  if (!S_ISREG(info.st_mode) || info.st_size <= 0 ||
      info.st_size % SECTOR_SIZE != 0) {
    close(fd);
    throw std::runtime_error("Disk image must be a nonempty regular file with "
                             "a size divisible by 512");
  }
  capacity = info.st_size / SECTOR_SIZE;
}

VirtioBlk::~VirtioBlk() { close(fd); }

bool VirtioBlk::contains_access(uint64_t address, uint8_t size) const {
  const uint64_t offset = address - base;
  if (address < base || offset >= MMIO_SIZE)
    return false;
  if (offset < CONFIG)
    return size == WORD && offset % 4 == 0;
  return (size == BYTE || size == HALFWORD || size == WORD ||
          size == DOUBLEWORD) &&
         size / 8 <= MMIO_SIZE - offset;
}

uint64_t VirtioBlk::read(uint64_t address, uint8_t size) const {
  if (!contains_access(address, size))
    return UINT64_MAX;

  const uint64_t offset = address - base;
  switch (offset) {
  case MAGIC:
    return 0x74726976;
  case VERSION:
    return 2;
  case DEVICE_ID:
    return 2;
  case VENDOR_ID:
    return 0x56435352;
  case DEVICE_FEATURES:
    return device_features_select < 2
               ? static_cast<uint32_t>(DEVICE_FEATURE_BITS >>
                                       (32 * device_features_select))
               : 0;
  case QUEUE_NUM_MAX:
    return queue_select == 0 ? QUEUE_MAX : 0;
  case QUEUE_READY:
    return queue_select == 0 && queue.ready;
  case INTERRUPT_STATUS:
    return interrupts;
  case DEVICE_STATUS:
    return status;
  default:
    if (offset >= CONFIG && offset < CONFIG + sizeof(capacity)) {
      const uint64_t mask = UINT64_MAX >> (64 - size);
      return (capacity >> (8 * (offset - CONFIG))) & mask;
    }
    return 0;
  }
}

void VirtioBlk::write(uint64_t address, uint8_t size, uint64_t value,
                      uint64_t ram_base, std::span<uint8_t> ram) {
  if (!contains_access(address, size))
    return;

  const uint32_t word = value;
  const uint64_t offset = address - base;
  switch (offset) {
  case DEVICE_FEATURES_SELECT:
    device_features_select = word;
    return;
  case DRIVER_FEATURES_SELECT:
    driver_features_select = word;
    return;
  case DRIVER_FEATURES:
    if (driver_features_select < 2 && !(status & FEATURES_OK))
      write_half(driver_features, word, 32 * driver_features_select);
    return;
  case QUEUE_SELECT:
    queue_select = word;
    return;
  case QUEUE_NOTIFY:
    if (word == 0)
      notify(ram_base, ram);
    return;
  case INTERRUPT_ACK:
    interrupts &= ~word;
    return;
  case DEVICE_STATUS:
    if (!word) {
      status = interrupts = queue_select = 0;
      device_features_select = driver_features_select = 0;
      driver_features = 0;
      queue = {};
      return;
    }
    status |= word & ~NEEDS_RESET;
    if (!(driver_features & VERSION_1_FEATURE) ||
        (driver_features & ~DEVICE_FEATURE_BITS))
      status &= ~(FEATURES_OK | DRIVER_OK);
    return;
  case QUEUE_READY:
    if (queue_select == 0) {
      if (!queue.ready && word == 1)
        queue.last_available = 0;
      queue.ready = word == 1;
    }
    return;
  }

  if (queue_select != 0 || queue.ready)
    return;

  switch (offset) {
  case QUEUE_NUM:
    queue.size = word <= QUEUE_MAX && std::has_single_bit(word) ? word : 0;
    break;
  case QUEUE_DESC_LOW:
    write_half(queue.descriptors, word, 0);
    break;
  case QUEUE_DESC_HIGH:
    write_half(queue.descriptors, word, 32);
    break;
  case QUEUE_AVAIL_LOW:
    write_half(queue.available, word, 0);
    break;
  case QUEUE_AVAIL_HIGH:
    write_half(queue.available, word, 32);
    break;
  case QUEUE_USED_LOW:
    write_half(queue.used, word, 0);
    break;
  case QUEUE_USED_HIGH:
    write_half(queue.used, word, 32);
    break;
  }
}

void VirtioBlk::fail_queue() {
  status |= NEEDS_RESET;
  interrupts |= CONFIG_INTERRUPT;
}

void VirtioBlk::notify(uint64_t ram_base, std::span<uint8_t> ram) {
  if (!queue.ready || !(status & DRIVER_OK) || !(status & FEATURES_OK) ||
      (status & (NEEDS_RESET | FAILED)))
    return;

  const GuestRam memory{ram_base, ram};
  auto descriptors = memory.bytes(queue.descriptors, DESC_SIZE * queue.size);
  auto available =
      memory.bytes(queue.available, RING_HEADER_SIZE + 2 * queue.size);
  auto used = memory.bytes(queue.used, RING_HEADER_SIZE + 8 * queue.size);
  if (!queue.size || (queue.descriptors % 16) || (queue.available % 2) ||
      (queue.used % 4) || descriptors.empty() || available.empty() ||
      used.empty()) {
    fail_queue();
    return;
  }

  const uint16_t available_index = load_le<uint16_t>(available.data() + 2);
  const uint16_t pending = available_index - queue.last_available;
  if (pending > queue.size) {
    fail_queue();
    return;
  }

  for (unsigned request = 0; request < pending; ++request) {
    const unsigned slot = queue.last_available % queue.size;
    const uint16_t head =
        load_le<uint16_t>(available.data() + RING_HEADER_SIZE + 2 * slot);
    std::array<Buffer, QUEUE_MAX> chain;
    unsigned count = 0;
    uint16_t index = head;
    for (;;) {
      if (index >= queue.size || count == queue.size) {
        fail_queue();
        return;
      }
      const uint8_t *desc = descriptors.data() + DESC_SIZE * index;
      const uint32_t length = load_le<uint32_t>(desc + 8);
      const uint16_t flags = load_le<uint16_t>(desc + 12);
      auto bytes = memory.bytes(load_le<uint64_t>(desc), length);
      if (!length || bytes.empty() || (flags & ~(DESC_NEXT | DESC_WRITE))) {
        fail_queue();
        return;
      }
      chain[count++] = {bytes, (flags & DESC_WRITE) != 0};
      if (!(flags & DESC_NEXT))
        break;
      index = load_le<uint16_t>(desc + 14);
    }

    auto length = process_request(fd, capacity, driver_features & FLUSH_FEATURE,
                                  std::span(chain).first(count));
    if (!length) {
      fail_queue();
      return;
    }
    uint8_t *entry = used.data() + RING_HEADER_SIZE + 8 * slot;
    store_le<uint32_t>(entry, head);
    store_le<uint32_t>(entry + 4, *length);
    store_le<uint16_t>(used.data() + 2, ++queue.last_available);
  }
  if (pending && !(load_le<uint16_t>(available.data()) & AVAIL_NO_INTERRUPT))
    interrupts |= USED_INTERRUPT;
}
