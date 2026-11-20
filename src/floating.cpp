#include "cpu.h"

namespace {

constexpr uint64_t NAN_BOX = 0xffffffff00000000ULL;
constexpr uint32_t SINGLE_NAN = 0x7fc00000;
constexpr uint64_t DOUBLE_NAN = 0x7ff8000000000000ULL;
constexpr uint64_t SIGNALING_NAN_CLASS = 1ULL << 8;
constexpr uint64_t QUIET_NAN_CLASS = 1ULL << 9;
constexpr uint64_t NAN_CLASSES = SIGNALING_NAN_CLASS | QUIET_NAN_CLASS;
constexpr uint64_t INVALID_FLAG = 1ULL << 4;

uint64_t box_single(uint64_t bits) {
  return NAN_BOX | static_cast<uint32_t>(bits);
}

uint32_t single_operand(uint64_t bits) {
  return (bits & NAN_BOX) == NAN_BOX ? static_cast<uint32_t>(bits) : SINGLE_NAN;
}

uint64_t classify(uint64_t bits, bool wide) {
  const unsigned fraction_width = wide ? 52 : 23;
  const unsigned sign_bit = wide ? 63 : 31;
  const uint64_t max_exponent = wide ? 0x7ff : 0xff;
  const uint64_t exponent = (bits >> fraction_width) & max_exponent;
  const uint64_t fraction = bits & ((1ULL << fraction_width) - 1);
  const bool negative = (bits >> sign_bit) & 1;

  if (exponent == max_exponent) {
    if (fraction == 0)
      return 1ULL << (negative ? 0 : 7); // Negative or positive infinity.
    // The top fraction bit distinguishes quiet NaNs from signaling NaNs.
    return (fraction >> (fraction_width - 1)) ? QUIET_NAN_CLASS
                                               : SIGNALING_NAN_CLASS;
  }
  // A zero exponent encodes signed zero or a subnormal, depending on the fraction.
  if (exponent == 0)
    return 1ULL << (fraction == 0 ? (negative ? 3 : 4) : (negative ? 2 : 5));
  return 1ULL << (negative ? 1 : 6); // Negative or positive normal value.
}

bool less_than(uint64_t lhs, uint64_t rhs, uint64_t sign_mask) {
  // With NaNs excluded, IEEE encodings are ordered by magnitude within a sign.
  if ((lhs ^ rhs) & sign_mask)
    return (lhs & sign_mask) != 0;
  return (lhs & sign_mask) ? lhs > rhs : lhs < rhs;
}

} // namespace

Cpu::ExecResult Cpu::exec_FLOAT_LOAD(uint64_t inst, std::stringstream *ss) {
  const auto funct3 = FUNCT3(inst);
  if (!cregs->floating_enabled() || (funct3 != 2 && funct3 != 3))
    return std::unexpected(Exception::IllegalInstruction);

  const bool wide = funct3 == 3;
  DB(ss, inst, wide ? "fld" : "flw");
  const uint8_t size = wide ? DOUBLEWORD : WORD;
  uint64_t addr = xregs->read(RS1(inst)) + IMM_I(inst);
  auto physical = resolve_memory_access(addr, size, csr::Csr::PMP_READ);
  if (!physical)
    return std::unexpected(physical.error());

  const uint64_t bits = bus->read(*physical, size);
  fregs[RD(inst)] = wide ? bits : box_single(bits);
  cregs->mark_floating_dirty();
  return {};
}

Cpu::ExecResult Cpu::exec_FLOAT_STORE(uint64_t inst, std::stringstream *ss) {
  const auto funct3 = FUNCT3(inst);
  if (!cregs->floating_enabled() || (funct3 != 2 && funct3 != 3))
    return std::unexpected(Exception::IllegalInstruction);

  const bool wide = funct3 == 3;
  DB(ss, inst, wide ? "fsd" : "fsw");
  const uint8_t size = wide ? DOUBLEWORD : WORD;
  const int64_t offset = IMM_S(inst);
  auto physical = resolve_memory_access(xregs->read(RS1(inst)) + offset, size,
                                        csr::Csr::PMP_WRITE);
  if (!physical)
    return std::unexpected(physical.error());

  bus->write(*physical, size, fregs[RS2(inst)]);
  return {};
}

Cpu::ExecResult Cpu::exec_FLOAT(uint64_t inst, std::stringstream *ss) {
  const auto funct7 = FUNCT7(inst);
  const auto format = funct7 & 3;
  if (!cregs->floating_enabled() || format > 1)
    return std::unexpected(Exception::IllegalInstruction);

  const bool wide = format == 1;
  const auto funct3 = FUNCT3(inst);
  const auto rs1 = RS1(inst);
  const auto rs2 = RS2(inst);
  const auto rd = RD(inst);
  const uint64_t sign_mask = 1ULL << (wide ? 63 : 31);
  auto operand = [this, wide](uint64_t reg) -> uint64_t {
    return wide ? fregs[reg] : single_operand(fregs[reg]);
  };
  auto raise_invalid = [this] {
    cregs->write(csr::Address::FFLAGS,
                 cregs->load(csr::Address::FFLAGS) | INVALID_FLAG);
  };

  switch (funct7 >> 2) {
  case 0x04: { // FSGNJ, FSGNJN, FSGNJX
    if (funct3 > 2)
      return std::unexpected(Exception::IllegalInstruction);
    DB(ss, inst, wide ? "fsgnj.d" : "fsgnj.s");
    const uint64_t lhs = operand(rs1);
    const uint64_t rhs = operand(rs2);
    uint64_t sign = rhs & sign_mask;
    if (funct3 == 1)
      sign ^= sign_mask;
    else if (funct3 == 2)
      sign ^= lhs & sign_mask;
    const uint64_t result = (lhs & ~sign_mask) | sign;
    fregs[rd] = wide ? result : box_single(result);
    cregs->mark_floating_dirty();
    return {};
  }
  case 0x05: { // FMIN and FMAX
    if (funct3 > 1)
      return std::unexpected(Exception::IllegalInstruction);
    DB(ss, inst, funct3 == 0 ? (wide ? "fmin.d" : "fmin.s")
                             : (wide ? "fmax.d" : "fmax.s"));
    const uint64_t lhs = operand(rs1);
    const uint64_t rhs = operand(rs2);
    const uint64_t lhs_class = classify(lhs, wide);
    const uint64_t rhs_class = classify(rhs, wide);
    if ((lhs_class | rhs_class) & SIGNALING_NAN_CLASS)
      raise_invalid();

    uint64_t result = lhs;
    if (lhs_class & NAN_CLASSES)
      result = (rhs_class & NAN_CLASSES) ? (wide ? DOUBLE_NAN : SINGLE_NAN) : rhs;
    else if (!(rhs_class & NAN_CLASSES))
      result = less_than(lhs, rhs, sign_mask) == (funct3 == 0) ? lhs : rhs;

    fregs[rd] = wide ? result : box_single(result);
    cregs->mark_floating_dirty();
    return {};
  }
  case 0x14: { // FLE, FLT, FEQ
    if (funct3 > 2)
      return std::unexpected(Exception::IllegalInstruction);
    DB(ss, inst, wide ? "fcmp.d" : "fcmp.s");
    const uint64_t lhs = operand(rs1);
    const uint64_t rhs = operand(rs2);
    const uint64_t classes = classify(lhs, wide) | classify(rhs, wide);
    if (classes & NAN_CLASSES) {
      if (funct3 != 2 || (classes & SIGNALING_NAN_CLASS))
        raise_invalid();
      xregs->write(rd, 0);
      return {};
    }

    const bool equal = lhs == rhs || ((lhs | rhs) & ~sign_mask) == 0;
    const bool less = !equal && less_than(lhs, rhs, sign_mask);
    xregs->write(rd, funct3 == 2 ? equal : less || (funct3 == 0 && equal));
    return {};
  }
  case 0x1c: // FMV.X.W, FMV.X.D, FCLASS
    if (rs2 != 0 || funct3 > 1)
      return std::unexpected(Exception::IllegalInstruction);
    if (funct3 == 1) {
      DB(ss, inst, wide ? "fclass.d" : "fclass.s");
      xregs->write(rd, classify(operand(rs1), wide));
    } else {
      DB(ss, inst, wide ? "fmv.x.d" : "fmv.x.w");
      xregs->write(rd, wide ? fregs[rs1] : SIGNEXTEND_CAST(fregs[rs1], int32_t));
    }
    return {};
  case 0x1e: // FMV.W.X, FMV.D.X
    if (rs2 != 0 || funct3 != 0)
      return std::unexpected(Exception::IllegalInstruction);
    DB(ss, inst, wide ? "fmv.d.x" : "fmv.w.x");
    fregs[rd] = wide ? xregs->read(rs1) : box_single(xregs->read(rs1));
    cregs->mark_floating_dirty();
    return {};
  default:
    return std::unexpected(Exception::IllegalInstruction);
  }
}
