#include "bus.h"
#include "cpu.h"
#include "exception.h"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <print>
#include <sstream>
#include <string>

constexpr uint64_t EXIT_SYSCALL = 93;

std::optional<uint64_t> find_tohost(const std::filesystem::path &binary,
                                    uint64_t image_size) {
  auto dump = binary.parent_path().parent_path() / "dumped" / binary.filename();
  dump.replace_extension(".dump");
  std::ifstream file(dump);
  std::optional<uint64_t> start;
  std::optional<uint64_t> tohost;
  for (std::string line; std::getline(file, line);) {
    const bool is_start = line.ends_with(" <_start>:");
    if (!is_start && !line.ends_with(" <tohost>:"))
      continue;

    const char *end = line.data() + line.find(' ');
    uint64_t addr;
    auto parsed = std::from_chars(line.data(), end, addr, 16);
    if (parsed.ec != std::errc{} || parsed.ptr != end)
      continue;
    (is_start ? start : tohost) = addr;
    if (start && tohost)
      break;
  }

  // The flat image is loaded at DRAM_BASE, regardless of its link address.
  if (!start || !tohost || *tohost < *start || image_size < 4 ||
      *tohost - *start > image_size - 4)
    return std::nullopt;
  return DRAM_BASE + (*tohost - *start);
}

std::size_t load_into_dram(const char *path, Bus *bus) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    std::cerr << "File reading failed\n";
    return 0;
  }

  file.seekg(0, std::ios::end);
  const auto size = file.tellg();
  if (size <= 0 || size > static_cast<std::streamoff>(bus->dram.size()))
    return 0;

  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> data(static_cast<std::size_t>(size));
  if (!file.read(reinterpret_cast<char *>(data.data()), size))
    return 0;

  for (unsigned long i = 0; i < data.size(); i++) {
    bus->write(DRAM_BASE + i, BYTE, data[i]);
  }
  return data.size();
}

int test_binary(const std::filesystem::directory_entry entry) {

  Bus *bus = new Bus();
  const auto image_size = load_into_dram(entry.path().c_str(), bus);
  if (image_size == 0) {
    std::cerr << "Dram loading failed\n";
    return -1;
  }

  Cpu *cpu = new Cpu(bus);
  auto tohost_addr = find_tohost(entry.path(), image_size);
  if (tohost_addr && bus->read(*tohost_addr, WORD) != 0)
    tohost_addr.reset();
  uint64_t a0 = 10;
  uint64_t tohost = 0;
  auto last_exception = Exception::None;

  bool timeout = false;
  bool reached_ecall = false;
  auto start_time = std::chrono::high_resolution_clock::now();

  std::stringstream ss;

  while (!timeout) {
    cpu->oneTick(&ss);

    const auto exception = cpu->exception.exception;
    if (exception != Exception::None)
      last_exception = exception;
    const bool is_ecall = exception == Exception::EnvironmentCallMmode ||
                          exception == Exception::EnvironmentCallUmode ||
                          exception == Exception::EnvironmentCallSmode;
    // The pass/fail macros set a7 to the exit syscall number.
    const bool delegated_ecall = is_ecall && cpu->mode == Mode::SUPERVISOR &&
                                 cpu->xregs->read(17) != EXIT_SYSCALL;
    const bool csr_trap = exception == Exception::IllegalInstruction &&
                          OPCODE(cpu->prev_inst) == 0x73 &&
                          (FUNCT3(cpu->prev_inst) & 3) != 0;
    const bool handled_trap = delegated_ecall || exception == Exception::Breakpoint ||
                              exception == Exception::InstructionAddressMisaligned ||
                              csr_trap;
    // Stock tests can report through tohost after executing a trap handler.
    const bool exit_ecall = is_ecall &&
                           (tohost_addr ? cpu->xregs->read(17) == EXIT_SYSCALL
                                        : !delegated_ecall);
    if (exit_ecall) {
      a0 = cpu->xregs->read(10);
      reached_ecall = true;
      break;
    } else if (exception != Exception::None &&
               ((!tohost_addr && !handled_trap) ||
                cpu->pc - DRAM_BASE >= bus->dram.size())) {
      break;
    }

    if (tohost_addr) {
      tohost = bus->read(*tohost_addr, WORD);
      if (tohost != 0)
        break;
    }

    timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::high_resolution_clock::now().time_since_epoch() -
                  start_time.time_since_epoch())
                  .count() > 2000;
  }

  if (a0 == 0 || tohost == 1) {
    std::println("Passed");
    return 1;
  }

  ss << "Test failed at: " << std::hex << cpu->pc << "\n";
  cpu->dump_registers(ss);

  if (!timeout) {
    if (tohost != 0) {
      std::println(ss, "tohost reported failure: {}", tohost);
      std::println("tohost reported failure: {}", tohost);
      if (last_exception != Exception::None)
        std::println(ss, "Last exception: {}",
                     cpu->exception.get_exception_str(last_exception));
    } else if (!reached_ecall) {
      std::println(ss, "Exception: {}",
                   cpu->exception.get_exception_str(cpu->exception.exception));
      std::println("Exception: {}",
                   cpu->exception.get_exception_str(cpu->exception.exception));
    } else {
      std::println(ss, "a0 was not equal to zero: {}", cpu->xregs->read(10));
      std::println("a0 was not equal to zero: {}", cpu->xregs->read(10));
    }
  } else {
    std::println(ss, "Timeout");
    std::println("Timeout");
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
