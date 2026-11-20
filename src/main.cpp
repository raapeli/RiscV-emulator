#include "bus.h"
#include "cpu.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr uint64_t LINUX_RAM_BASE = 0x80000000;
constexpr uint64_t LINUX_RAM_SIZE = 128 * 1024 * 1024;
constexpr uint64_t LINUX_KERNEL = 0x80200000;
constexpr uint64_t LINUX_INITRD = 0x84000000;
constexpr uint64_t LINUX_DTB = 0x87e00000;
constexpr Bus::DeviceMap LINUX_DEVICES{
    0x02000000, 0x0c000000, Bus::UartMapping{0x10000000, 10}};

struct BootImage {
  std::string_view option;
  uint64_t address;
  uint64_t limit;
  const char *path = nullptr;
  std::vector<uint8_t> bytes{};
};

std::optional<std::vector<uint8_t>>
read_image(const char *path, uint64_t capacity, std::string_view option = {}) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const auto size = file.tellg();
  if (size < 0) {
    std::cerr << "File reading failed for " << path << '\n';
    return std::nullopt;
  }
  if (size > static_cast<std::streamoff>(capacity)) {
    if (option.empty())
      std::cerr << "Binary exceeds guest RAM\n";
    else
      std::cerr << option << " image exceeds its guest RAM region\n";
    return std::nullopt;
  }
  if (size == 0 && !option.empty()) {
    std::cerr << option << " image is empty\n";
    return std::nullopt;
  }

  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> bytes(size);
  if (!file.read(reinterpret_cast<char *>(bytes.data()), size)) {
    std::cerr << "File reading failed for " << path << '\n';
    return std::nullopt;
  }
  return bytes;
}

int run_flat_binary(const char *path) {
  auto bytes = read_image(path, DRAM_SIZE);
  if (!bytes)
    return 1;

  Bus bus(true);
  std::ranges::copy(*bytes, bus.dram.begin());
  std::cerr << "Parsed successfully\n";
  Cpu cpu(&bus);
  cpu.start();
  return 0;
}

int run_linux(int argc, char *argv[]) {
  // Each image is confined to the region before the next image.
  std::array images{
      BootImage{"--firmware", LINUX_RAM_BASE, LINUX_KERNEL},
      BootImage{"--kernel", LINUX_KERNEL, LINUX_INITRD},
      BootImage{"--initrd", LINUX_INITRD, LINUX_DTB},
      BootImage{"--dtb", LINUX_DTB, LINUX_RAM_BASE + LINUX_RAM_SIZE},
  };
  for (int i = 2; i < argc; i += 2) {
    const std::string_view option = argv[i];
    auto image = std::ranges::find(images, option, &BootImage::option);
    if (image == images.end() || image->path || i + 1 == argc ||
        std::string_view(argv[i + 1]).starts_with("--")) {
      std::cerr << "Invalid Linux boot argument: " << option << '\n';
      return 1;
    }
    image->path = argv[i + 1];
  }

  for (auto &image : images) {
    if (!image.path) {
      std::cerr << "Missing Linux boot argument: " << image.option << '\n';
      return 1;
    }
    auto bytes =
        read_image(image.path, image.limit - image.address, image.option);
    if (!bytes)
      return 1;
    image.bytes = std::move(*bytes);
  }

  Bus bus(true, LINUX_RAM_BASE, LINUX_RAM_SIZE, LINUX_DEVICES);
  for (const auto &image : images)
    std::ranges::copy(image.bytes,
                      bus.dram.begin() + (image.address - bus.ram_base));

  Cpu cpu(&bus);
  cpu.xregs->write(10, 0);         // a0: hart ID
  cpu.xregs->write(11, LINUX_DTB); // a1: physical DTB address
  cpu.start();
  return 0;
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc >= 2 && std::string_view(argv[1]) == "--linux")
    return run_linux(argc, argv);
  if (argc == 2)
    return run_flat_binary(argv[1]);

  std::cerr << "Usage: RiscV BINARY\n"
               "       RiscV --linux --firmware FILE --kernel FILE --dtb FILE "
               "--initrd FILE\n";
  return 1;
}
