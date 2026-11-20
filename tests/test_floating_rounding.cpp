#include "cpu.h"

#include <cstdlib>
#include <iostream>

static void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

int main() {
  using csr::Address;
  Bus bus;
  Cpu cpu(&bus);
  auto &regs = *cpu.cregs;
  regs.write(Address::MSTATUS, csr::Mask::FS);

  auto execute = [&](uint32_t inst) {
    cpu.pc = DRAM_BASE;
    bus.write(cpu.pc, WORD, inst);
    require(cpu.execute().has_value(), "Floating-point instruction must execute");
  };
  auto operands = [&](uint64_t lhs, uint64_t rhs, bool wide = false) {
    cpu.xregs->write(10, lhs);
    cpu.xregs->write(11, rhs);
    const uint32_t format = wide ? 1U << 25 : 0;
    execute(0xf0050053 | format); // fmv.w.x f0, a0 (or fmv.d.x)
    execute(0xf00580d3 | format); // fmv.w.x f1, a1 (or fmv.d.x)
    regs.write(Address::FFLAGS, 0);
  };
  auto compute = [&](uint32_t inst, bool wide = false) {
    execute(inst);
    execute((wide ? 0xe2000553 : 0xe0000553) | (2U << 15)); // fmv.x.w a0, f2
    return cpu.xregs->read(10);
  };

  operands(0x3f800000, 0x33800000); // 1 + 2^-24, halfway between two floats.
  require(compute(0x00100153) == 0x3f800000 && regs.load(Address::FFLAGS) == 1,
          "RNE must round the halfway sum to even and set Inexact");
  regs.write(Address::FRM, 4);
  require(compute(0x00107153) == 0x3f800001 && regs.load(Address::FFLAGS) == 1,
          "Dynamic RMM must round the halfway sum away from zero");

  operands(0x3ff0000000000000ULL, 0x3ca0000000000000ULL, true);
  require(compute(0x02104153, true) == 0x3ff0000000000001ULL,
          "Double-precision RMM must round ties away from zero");

  operands(1, 0x40000000); // Smallest subnormal divided by two.
  require(compute(0x18100153) == 0 && regs.load(Address::FFLAGS) == 3,
          "RNE must round the halfway subnormal to zero and report underflow");
  regs.write(Address::FFLAGS, 0);
  require(compute(0x18104153) == 1 && regs.load(Address::FFLAGS) == 3,
          "RMM must round the halfway subnormal away from zero");

  operands(0x7f7fffff, 0x7f7fffff);
  require(compute(0x00102153) == 0x7f7fffff && regs.load(Address::FFLAGS) == 5,
          "Positive overflow under RDN must saturate and report overflow");

  operands(0x3f800000, 0);
  regs.write(Address::FFLAGS, 1);
  require(compute(0x18100153) == 0x7f800000 && regs.load(Address::FFLAGS) == 9,
          "Divide by zero must preserve previously accrued flags");

  auto addend = [&](uint64_t bits, bool wide = false) {
    cpu.xregs->write(12, bits);
    execute(wide ? 0xf20601d3 : 0xf00601d3); // fmv.w.x f3, a2 (or fmv.d.x)
  };

  operands(0x3f800001, 0x3f7ffffe); // (1 + 2^-23) * (1 - 2^-23).
  addend(0xbf800000);
  require(compute(0x18100143) == 0xffffffffa8800000ULL &&
              regs.load(Address::FFLAGS) == 0,
          "Single-precision FMA must retain the exact product through cancellation");

  operands(0x3ff0000000000001ULL, 0x3feffffffffffffeULL, true);
  addend(0xbff0000000000000ULL, true);
  require(compute(0x1a100143, true) == 0xb970000000000000ULL &&
              regs.load(Address::FFLAGS) == 0,
          "Double-precision FMA must retain the exact product through cancellation");

  operands(0x7f800000, 0);
  addend(0x7fffffff);
  require(compute(0x18100143) == 0x7fc00000 && regs.load(Address::FFLAGS) == 16,
          "FMA must signal Invalid for infinity times zero with a quiet NaN addend");
}
