#include "bus.h"
#include "cpu.h"
#include "exception.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <sstream>
#include <string>

int load_into_dram(const char *path, Bus *bus) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    std::cerr << "File reading failed\n";
    return -1;
  }

  file.seekg(0, std::ios::end);
  const auto size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> data(static_cast<std::size_t>(size));
  file.read(reinterpret_cast<char *>(data.data()), size);

  file.close();

  for (unsigned long i = 0; i < data.size(); i++) {
    bus->write(DRAM_BASE + i, BYTE, data[i]);
  }
  return 1;
}

int test_binary(const std::filesystem::directory_entry entry) {

  Bus *bus = new Bus();
  if (!load_into_dram(entry.path().c_str(), bus)) {
    std::cerr << "Dram loading failed\n";
    return -1;
  }

  Cpu *cpu = new Cpu(bus);
  uint64_t a0 = 10;

  bool timeout = false;
  bool failed_on_exception = true;
  auto start_time = std::chrono::high_resolution_clock::now();

  std::stringstream ss;

  while (!timeout) {
    cpu->oneTick(&ss);

    if (cpu->exception.exception == Exception::EnvironmentCallMmode ||
        cpu->exception.exception == Exception::EnvironmentCallUmode) {
      a0 = cpu->xregs->read(10);
      break;
    } else if (cpu->exception.exception != Exception::None) {
      failed_on_exception = true;
      break;
    }

    timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::high_resolution_clock::now().time_since_epoch() -
                  start_time.time_since_epoch())
                  .count() > 2000;
  }

  if (a0 == 0) {
    std::println("Passed");
    return 1;
  }

  ss << "Test failed at: " << std::hex << cpu->pc << "\n";
  cpu->dump_registers(ss);

  if (!timeout) {
    if (failed_on_exception &&
        (cpu->exception.exception != Exception::EnvironmentCallMmode &&
         cpu->exception.exception != Exception::EnvironmentCallUmode)) {
      std::println(ss, "Exception: {}",
                   cpu->exception.get_exception_str(cpu->exception.exception));
    } else {
      std::println(ss, "a0 was not equal to zero: {}", cpu->xregs->read(10));
    }
  } else {
    std::println(ss, "Timeout");
  }

  std::filesystem::create_directories("./logs");
  std::string file_path =
      std::format("./logs/{}.log", entry.path().stem().c_str());

  std::ofstream of = std::ofstream(file_path);
  of << ss.str();
  if (of) {
    std::println("Data dumbed to {}", file_path);
  }

  return -1;
}

int test_bins(std::string directory) {

  int total = 0;
  int passed = 0;
  int failed = 0;

  for (auto entry : std::filesystem::directory_iterator(directory)) {
    std::println("Running: {}", entry.path().c_str());
    if (std::filesystem::is_regular_file(entry) &&
        entry.path().extension() == ".bin") {

      total += 1;

      if (test_binary(entry) == 1) {
        std::println("Pass");
        passed += 1;
      } else {
        std::println("Fail");
        failed += 1;
      }
    }
  }
  std::println("Passed: {} / {}", passed, total);
  return failed;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::println("Error: No path passed");
    return -1;
  }

  return test_bins(argv[1]);
}
