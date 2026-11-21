#include "bus.h"
#include "cpu.h"
#include "linux_boot.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <new>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {

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

int run_linux(int argc, char *argv[]) try {
  using namespace linux_boot;
  constexpr uint64_t MIB = 1024 * 1024;
  constexpr uint64_t MIN_RAM_MIB = (DTB_ADDRESS - RAM_BASE) / MIB + 1;
  const uint64_t max_ram_mib =
      std::min<uint64_t>(std::vector<uint8_t>{}.max_size(),
                         UINT64_MAX - RAM_BASE) / MIB;

  // Each image is confined to the region before the next image.
  std::array images{
      BootImage{"--firmware", RAM_BASE, KERNEL_ADDRESS},
      BootImage{"--kernel", KERNEL_ADDRESS, INITRD_ADDRESS},
      BootImage{"--initrd", INITRD_ADDRESS, DTB_ADDRESS},
      BootImage{"--dtb", DTB_ADDRESS, 0},
  };
  auto &[firmware, kernel, initrd, dtb] = images;
  const char *disk_path = nullptr;
  std::optional<uint64_t> configured_ram_size;
  for (int i = 2; i < argc; i += 2) {
    const std::string_view option = argv[i];
    auto image = std::ranges::find(images, option, &BootImage::option);
    if (i + 1 == argc ||
        std::string_view(argv[i + 1]).starts_with("--")) {
      std::cerr << "Invalid Linux boot argument: " << option << '\n';
      return 1;
    }
    if (option == "--disk" && !disk_path) {
      disk_path = argv[i + 1];
    } else if (option == "--ram-size" && !configured_ram_size) {
      const std::string_view value = argv[i + 1];
      uint64_t size_mib;
      const auto [end, error] =
          std::from_chars(value.data(), value.data() + value.size(), size_mib);
      if (error != std::errc{} || end != value.data() + value.size() ||
          size_mib > max_ram_mib) {
        std::cerr << "Invalid --ram-size: expected a RAM size in MiB\n";
        return 1;
      }
      if (size_mib < MIN_RAM_MIB) {
        std::cerr << "--ram-size must be at least " << MIN_RAM_MIB << " MiB\n";
        return 1;
      }
      configured_ram_size = size_mib * MIB;
    } else if (image != images.end() && !image->path) {
      image->path = argv[i + 1];
    } else {
      std::cerr << "Invalid Linux boot argument: " << option << '\n';
      return 1;
    }
  }

  const uint64_t ram_size = configured_ram_size.value_or(DEFAULT_RAM_SIZE);
  dtb.limit = RAM_BASE + ram_size;
  for (auto &image : images) {
    if (!image.path) {
      if (&image == &dtb)
        continue;
      std::cerr << "Missing Linux boot argument: " << image.option << '\n';
      return 1;
    }
    auto bytes =
        read_image(image.path, image.limit - image.address, image.option);
    if (!bytes)
      return 1;
    image.bytes = std::move(*bytes);
  }

  if (!dtb.path) {
    try {
      dtb.bytes = make_dtb(ram_size, initrd.bytes.size(), disk_path != nullptr);
    } catch (const std::runtime_error &error) {
      std::cerr << "Could not generate Linux device tree: " << error.what()
                << '\n';
      return 1;
    }
  }

  Bus bus(true, RAM_BASE, ram_size, DEVICES);
  if (disk_path) {
    try {
      bus.attach_disk(DISK_BASE, DISK_IRQ, disk_path);
    } catch (const std::runtime_error &error) {
      std::cerr << "Could not open Linux disk image: " << error.what() << '\n';
      return 1;
    }
  }
  for (const auto &image : images)
    std::ranges::copy(image.bytes,
                      bus.dram.begin() + (image.address - bus.ram_base));

  Cpu cpu(&bus);
  cpu.xregs->write(10, 0);           // a0: hart ID
  cpu.xregs->write(11, DTB_ADDRESS); // a1: physical DTB address
  cpu.start();
  return 0;
} catch (const std::bad_alloc &) {
  std::cerr << "Not enough host memory for Linux boot\n";
  return 1;
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc >= 2 && std::string_view(argv[1]) == "--linux")
    return run_linux(argc, argv);
  if (argc == 2)
    return run_flat_binary(argv[1]);

  std::cerr << "Usage: RiscV BINARY\n"
               "       RiscV --linux --firmware FILE --kernel FILE "
               "--initrd FILE [--ram-size MIB] [--dtb FILE] [--disk FILE]\n";
  return 1;
}
