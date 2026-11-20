#include "linux_boot.h"
#include "interrupt.h"

#include <format>
#include <initializer_list>
#include <libfdt.h>
#include <stdexcept>
#include <utility>

namespace linux_boot {
namespace {

class DeviceTree {
public:
  DeviceTree() {
    check(fdt_create(blob.data(), blob.size()));
    check(fdt_finish_reservemap(blob.data()));
  }

  void begin_node(const char *name) { check(fdt_begin_node(blob.data(), name)); }
  void end_node() { check(fdt_end_node(blob.data())); }

  void property(const char *name, const void *data = nullptr, int size = 0) {
    check(fdt_property(blob.data(), name, data, size));
  }

  void string(const char *name, const char *value) {
    check(fdt_property_string(blob.data(), name, value));
  }

  void cells(const char *name, std::initializer_list<uint32_t> values) {
    std::vector<fdt32_t> encoded;
    encoded.reserve(values.size());
    for (uint32_t value : values)
      encoded.push_back(cpu_to_fdt32(value));
    property(name, encoded.data(), encoded.size() * sizeof(fdt32_t));
  }

  void address(const char *name, uint64_t value) {
    check(fdt_property_u64(blob.data(), name, value));
  }

  void region(uint64_t base, uint64_t size) {
    cells("reg", {static_cast<uint32_t>(base >> 32), static_cast<uint32_t>(base),
                  static_cast<uint32_t>(size >> 32), static_cast<uint32_t>(size)});
  }

  std::vector<uint8_t> finish() {
    check(fdt_finish(blob.data()));
    blob.resize(fdt_totalsize(blob.data()));
    return std::move(blob);
  }

private:
  static void check(int result) {
    if (result < 0)
      throw std::runtime_error(fdt_strerror(result));
  }

  std::vector<uint8_t> blob = std::vector<uint8_t>(4096);
};

} // namespace

std::vector<uint8_t> make_dtb(uint64_t initrd_size) {
  constexpr uint32_t CPU_INTC_PHANDLE = 1;
  constexpr uint32_t PLIC_PHANDLE = 2;
  constexpr char ISA_EXTENSIONS[] = "i\0m\0a\0f\0d\0c\0zicsr\0zifencei";
  constexpr char PLIC_COMPATIBLE[] = "sifive,plic-1.0.0\0riscv,plic0";
  const auto uart_node = std::format("serial@{:x}", UART.base);
  const auto uart_path = "/soc/" + uart_node;
  const auto stdout_path = uart_path + ":115200n8";

  DeviceTree dt;
  dt.begin_node("");
  dt.cells("#address-cells", {2});
  dt.cells("#size-cells", {2});
  dt.string("model", "Risc-V emulator");
  dt.string("compatible", "raapeli,riscv-emulator");

  dt.begin_node("chosen");
  dt.string("stdout-path", stdout_path.c_str());
  dt.string("bootargs", "console=ttyS0,115200 earlycon");
  dt.address("linux,initrd-start", INITRD_ADDRESS);
  dt.address("linux,initrd-end", INITRD_ADDRESS + initrd_size);
  dt.end_node();

  dt.begin_node("aliases");
  dt.string("serial0", uart_path.c_str());
  dt.end_node();

  dt.begin_node(std::format("memory@{:x}", RAM_BASE).c_str());
  dt.string("device_type", "memory");
  dt.region(RAM_BASE, RAM_SIZE);
  dt.end_node();

  dt.begin_node("cpus");
  dt.cells("#address-cells", {1});
  dt.cells("#size-cells", {0});
  dt.cells("timebase-frequency", {10000000});
  dt.begin_node("cpu@0");
  dt.string("device_type", "cpu");
  dt.string("compatible", "riscv");
  dt.string("status", "okay");
  dt.cells("reg", {0});
  dt.string("mmu-type", "riscv,sv39");
  dt.string("riscv,isa", "rv64imafdc_zicsr_zifencei");
  dt.string("riscv,isa-base", "rv64i");
  dt.property("riscv,isa-extensions", ISA_EXTENSIONS, sizeof(ISA_EXTENSIONS));
  dt.begin_node("interrupt-controller");
  dt.string("compatible", "riscv,cpu-intc");
  dt.cells("#address-cells", {0});
  dt.cells("#interrupt-cells", {1});
  dt.cells("phandle", {CPU_INTC_PHANDLE});
  dt.property("interrupt-controller");
  dt.end_node();
  dt.end_node();
  dt.end_node();

  dt.begin_node("soc");
  dt.string("compatible", "simple-bus");
  dt.cells("#address-cells", {2});
  dt.cells("#size-cells", {2});
  dt.property("ranges");

  dt.begin_node(std::format("clint@{:x}", CLINT_BASE).c_str());
  dt.string("compatible", "sifive,clint0");
  dt.region(CLINT_BASE, 0x10000);
  dt.cells("interrupts-extended",
           {CPU_INTC_PHANDLE, Interrupt::MachineSoftwareInterrupt,
            CPU_INTC_PHANDLE, Interrupt::MachineTimerInterrupt});
  dt.end_node();

  dt.begin_node(std::format("interrupt-controller@{:x}", PLIC_BASE).c_str());
  dt.property("compatible", PLIC_COMPATIBLE, sizeof(PLIC_COMPATIBLE));
  dt.region(PLIC_BASE, Plic::MMIO_SIZE);
  dt.cells("#address-cells", {0});
  dt.cells("#interrupt-cells", {1});
  dt.cells("riscv,ndev", {Plic::SOURCE_COUNT - 1});
  dt.cells("phandle", {PLIC_PHANDLE});
  dt.property("interrupt-controller");
  // The context order matches the PLIC's machine and supervisor MMIO banks.
  dt.cells("interrupts-extended",
           {CPU_INTC_PHANDLE, Interrupt::MachineExternalInterrupt,
            CPU_INTC_PHANDLE, Interrupt::SupervisorExternalInterrupt});
  dt.end_node();

  dt.begin_node(uart_node.c_str());
  dt.string("compatible", "ns16550a");
  dt.region(UART.base, 8);
  dt.cells("clock-frequency", {3686400});
  dt.cells("interrupt-parent", {PLIC_PHANDLE});
  dt.cells("interrupts", {UART.plic_source});
  dt.end_node();

  dt.end_node();
  dt.end_node();
  return dt.finish();
}

} // namespace linux_boot
