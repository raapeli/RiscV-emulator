#include "cpu.h"

#include <algorithm>
#include <bit>
#include <mpfr.h>

namespace {

constexpr uint64_t NAN_BOX = 0xffffffff00000000ULL;
constexpr uint32_t SINGLE_NAN = 0x7fc00000;
constexpr uint64_t DOUBLE_NAN = 0x7ff8000000000000ULL;
constexpr uint64_t SIGNALING_NAN_CLASS = 1ULL << 8;
constexpr uint64_t QUIET_NAN_CLASS = 1ULL << 9;
constexpr uint64_t NAN_CLASSES = SIGNALING_NAN_CLASS | QUIET_NAN_CLASS;
constexpr uint64_t INFINITY_CLASSES = (1ULL << 0) | (1ULL << 7);
constexpr uint64_t ZERO_CLASSES = (1ULL << 3) | (1ULL << 4);
constexpr uint64_t INVALID_FLAG = 1ULL << 4;
constexpr uint64_t DIVIDE_BY_ZERO_FLAG = 1ULL << 3;
constexpr uint64_t OVERFLOW_FLAG = 1ULL << 2;
constexpr uint64_t UNDERFLOW_FLAG = 1ULL << 1;
constexpr uint64_t INEXACT_FLAG = 1ULL;

enum RoundingMode : unsigned { RNE, RTZ, RDN, RUP, RMM, DYNAMIC = 7 };
enum ArithmeticOperation : unsigned { ADD, SUB, MUL, DIV, SQRT = 0x0b, FUSED };

struct FloatResult {
  uint64_t bits;
  uint64_t flags;
};

class MpfrNumber {
public:
  explicit MpfrNumber(mpfr_prec_t precision) { mpfr_init2(value, precision); }
  ~MpfrNumber() { mpfr_clear(value); }

  MpfrNumber(const MpfrNumber &) = delete;
  MpfrNumber &operator=(const MpfrNumber &) = delete;

  mpfr_t value;
};

class MpfrFlags {
public:
  ~MpfrFlags() { mpfr_flags_restore(saved, MPFR_FLAGS_ALL); }

private:
  const mpfr_flags_t saved = mpfr_flags_save();
};

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
  // A zero exponent encodes signed zero or a subnormal, depending on the
  // fraction.
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

// The result is rounded on a grid whose spacing stops shrinking at subnormals.
FloatResult round_result(mpfr_ptr number, bool wide, unsigned mode,
                         bool inexact) {
  const unsigned fraction_width = wide ? 52 : 23;
  const int max_exponent = wide ? 1023 : 127;
  const int min_exponent = 1 - max_exponent;
  const uint64_t hidden_bit = 1ULL << fraction_width;
  const uint64_t infinity = (2ULL * max_exponent + 1) << fraction_width;
  const bool negative = mpfr_signbit(number);
  const uint64_t sign = negative ? 1ULL << (wide ? 63 : 31) : 0;

  if (mpfr_inf_p(number))
    return {sign | infinity, 0};
  if (mpfr_zero_p(number))
    return {sign, 0};

  int exponent = mpfr_get_exp(number) - 1;
  uint64_t significand = 0;
  if (exponent <= max_exponent) {
    exponent = std::max(exponent, min_exponent);
    mpfr_abs(number, number, MPFR_RNDN);
    mpfr_div_2si(number, number, exponent - static_cast<int>(fraction_width),
                 MPFR_RNDN);
    inexact |= !mpfr_integer_p(number);
    if (mode == RMM)
      mpfr_round(number, number); // Round to nearest integer, ties away.
    const bool away = (mode == RDN && negative) || (mode == RUP && !negative);
    const mpfr_rnd_t rounding = mode == RNE ? MPFR_RNDN
                                : away      ? MPFR_RNDU
                                            : MPFR_RNDZ;
    significand = mpfr_get_uj(number, rounding);
    if (significand == 2 * hidden_bit) {
      significand >>= 1;
      ++exponent;
    }
  }

  if (exponent > max_exponent) {
    const bool to_infinity = mode == RNE || mode == RMM ||
                             (mode == RDN && negative) ||
                             (mode == RUP && !negative);
    return {sign | (to_infinity ? infinity : infinity - 1),
            OVERFLOW_FLAG | INEXACT_FLAG};
  }

  if (significand < hidden_bit)
    return {sign | significand, inexact ? UNDERFLOW_FLAG | INEXACT_FLAG : 0};
  const uint64_t biased = static_cast<uint64_t>(exponent + max_exponent);
  return {sign | (biased << fraction_width) | (significand - hidden_bit),
          inexact ? INEXACT_FLAG : 0};
}

void set_operand(mpfr_ptr dest, uint64_t bits, bool wide) {
  if (wide)
    mpfr_set_d(dest, std::bit_cast<double>(bits), MPFR_RNDN);
  else
    mpfr_set_flt(dest, std::bit_cast<float>(static_cast<uint32_t>(bits)),
                 MPFR_RNDN);
}

FloatResult convert_precision(uint64_t bits, bool source_wide, bool wide,
                              unsigned rounding) {
  const uint64_t classification = classify(bits, source_wide);
  if (classification & NAN_CLASSES)
    return {wide ? DOUBLE_NAN : SINGLE_NAN,
            classification == SIGNALING_NAN_CLASS ? INVALID_FLAG : 0};

  MpfrFlags flags;
  MpfrNumber value(53);
  set_operand(value.value, bits, source_wide);
  return round_result(value.value, wide, rounding, false);
}

FloatResult convert_from_integer(uint64_t bits, unsigned type, bool wide,
                                 unsigned rounding) {
  const bool unsigned_integer = type & 1;
  const bool word = type < 2;
  MpfrFlags flags;
  MpfrNumber value(64);
  if (unsigned_integer)
    mpfr_set_uj(value.value, word ? static_cast<uint32_t>(bits) : bits, MPFR_RNDN);
  else
    mpfr_set_sj(value.value, word ? static_cast<int32_t>(bits)
                                : static_cast<int64_t>(bits), MPFR_RNDN);
  return round_result(value.value, wide, rounding, false);
}

FloatResult convert_to_integer(uint64_t bits, unsigned type, bool wide,
                               unsigned rounding) {
  const bool unsigned_integer = type & 1;
  const bool word = type < 2;
  const unsigned width = word ? 32 : 64;
  const unsigned magnitude_bits = width - (unsigned_integer ? 0 : 1);
  const uint64_t maximum = UINT64_MAX >> (64 - magnitude_bits);
  const uint64_t minimum = unsigned_integer ? 0 : 1ULL << (width - 1);
  auto extend = [word](uint64_t value) {
    return word ? SIGNEXTEND_CAST(value, int32_t) : value;
  };
  if (classify(bits, wide) & NAN_CLASSES)
    return {extend(maximum), INVALID_FLAG};

  MpfrFlags flags;
  MpfrNumber value(64);
  set_operand(value.value, bits, wide);
  constexpr mpfr_rnd_t modes[] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDD, MPFR_RNDU};
  const int inexact = rounding == RMM ? mpfr_round(value.value, value.value)
                                       : mpfr_rint(value.value, value.value,
                                                   modes[rounding]);

  const bool below_minimum = unsigned_integer ? mpfr_sgn(value.value) < 0
      : mpfr_cmp_si_2exp(value.value, -1, magnitude_bits) < 0;
  if (below_minimum)
    return {extend(minimum), INVALID_FLAG};
  if (mpfr_cmp_ui_2exp(value.value, 1, magnitude_bits) >= 0)
    return {extend(maximum), INVALID_FLAG};

  const uint64_t result = unsigned_integer ? mpfr_get_uj(value.value, MPFR_RNDZ)
      : static_cast<uint64_t>(mpfr_get_sj(value.value, MPFR_RNDZ));
  return {extend(result), inexact ? INEXACT_FLAG : 0};
}

FloatResult arithmetic(unsigned operation, unsigned mode, bool wide,
                       uint64_t lhs, uint64_t rhs, uint64_t addend = 0) {
  const auto canonical_nan = wide ? DOUBLE_NAN : SINGLE_NAN;
  const uint64_t lhs_class = classify(lhs, wide);
  const uint64_t rhs_class = operation == SQRT ? 0 : classify(rhs, wide);
  const uint64_t addend_class = operation == FUSED ? classify(addend, wide) : 0;
  const uint64_t classes = lhs_class | rhs_class | addend_class;
  // Infinity times zero raises Invalid even if the addend is a quiet NaN.
  if (operation == FUSED && ((lhs_class | rhs_class) & INFINITY_CLASSES) &&
      ((lhs_class | rhs_class) & ZERO_CLASSES))
    return {canonical_nan, INVALID_FLAG};
  if (classes & NAN_CLASSES)
    return {canonical_nan, classes & SIGNALING_NAN_CLASS ? INVALID_FLAG : 0};

  MpfrFlags flags;
  MpfrNumber a(53), b(53);
  // This covers exact fused products and sums across binary64's exponent span.
  // It also distinguishes division and square-root rounding midpoints.
  MpfrNumber result(3328);
  set_operand(a.value, lhs, wide);
  if (operation != SQRT)
    set_operand(b.value, rhs, wide);
  mpfr_clear_divby0();
  const mpfr_rnd_t intermediate = mode == RDN ? MPFR_RNDD : MPFR_RNDN;
  int rounded;
  switch (operation) {
  case ADD:
    rounded = mpfr_add(result.value, a.value, b.value, intermediate);
    break;
  case SUB:
    rounded = mpfr_sub(result.value, a.value, b.value, intermediate);
    break;
  case MUL:
    rounded = mpfr_mul(result.value, a.value, b.value, intermediate);
    break;
  case DIV:
    rounded = mpfr_div(result.value, a.value, b.value, intermediate);
    break;
  case FUSED: {
    MpfrNumber c(53);
    set_operand(c.value, addend, wide);
    rounded = mpfr_fma(result.value, a.value, b.value, c.value, intermediate);
    break;
  }
  default: // SQRT
    rounded = mpfr_sqrt(result.value, a.value, intermediate);
    break;
  }
  if (mpfr_nan_p(result.value))
    return {canonical_nan, INVALID_FLAG};
  auto output = round_result(result.value, wide, mode, rounded != 0);
  if (mpfr_divby0_p())
    output.flags |= DIVIDE_BY_ZERO_FLAG;
  return output;
}

} // namespace

void Cpu::accrue_floating_flags(uint64_t flags) {
  if (flags)
    cregs->write(csr::Address::FFLAGS,
                 cregs->load(csr::Address::FFLAGS) | flags);
}

void Cpu::write_floating_result(uint64_t rd, bool wide, uint64_t bits,
                                uint64_t flags) {
  fregs[rd] = wide ? bits : box_single(bits);
  accrue_floating_flags(flags);
  cregs->mark_floating_dirty();
}

Cpu::ExecResult Cpu::exec_FLOAT_FUSED(uint64_t inst, std::stringstream *ss) {
  const auto format = FUNCT7(inst) & 3;
  const auto rm = FUNCT3(inst);
  const unsigned rounding = rm == DYNAMIC ? cregs->load(csr::Address::FRM) : rm;
  if (!cregs->floating_enabled() || format > 1 || rounding > RMM)
    return std::unexpected(Exception::IllegalInstruction);

  const bool wide = format == 1;
  const auto opcode = OPCODE(inst);
  [[maybe_unused]] constexpr const char *mnemonics[][2] = {
      {"fmadd.s", "fmadd.d"},
      {"fmsub.s", "fmsub.d"},
      {"fnmsub.s", "fnmsub.d"},
      {"fnmadd.s", "fnmadd.d"},
  };
  DB(ss, inst, mnemonics[(opcode - 0x43) / 4][wide]);

  auto operand = [this, wide](uint64_t reg) -> uint64_t {
    return wide ? fregs[reg] : single_operand(fregs[reg]);
  };
  const uint64_t sign = 1ULL << (wide ? 63 : 31);
  uint64_t lhs = operand(RS1(inst));
  uint64_t addend = operand((inst >> 27) & 31);
  if (opcode == 0x4b || opcode == 0x4f)
    lhs ^= sign;
  if (opcode == 0x47 || opcode == 0x4f)
    addend ^= sign;
  const auto result =
      arithmetic(FUSED, rounding, wide, lhs, operand(RS2(inst)), addend);
  write_floating_result(RD(inst), wide, result.bits, result.flags);
  return {};
}

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
  const unsigned rounding =
      funct3 == DYNAMIC ? cregs->load(csr::Address::FRM) : funct3;
  const auto rs1 = RS1(inst);
  const auto rs2 = RS2(inst);
  const auto rd = RD(inst);
  const uint64_t sign_mask = 1ULL << (wide ? 63 : 31);
  auto operand = [this, wide](uint64_t reg) -> uint64_t {
    return wide ? fregs[reg] : single_operand(fregs[reg]);
  };
  const unsigned operation = funct7 >> 2;
  switch (operation) {
  case ADD:
  case SUB:
  case MUL:
  case DIV:
  case SQRT: {
    if (rounding > RMM || (operation == SQRT && rs2 != 0))
      return std::unexpected(Exception::IllegalInstruction);
    [[maybe_unused]] constexpr const char *mnemonics[][2] = {
        {"fadd.s", "fadd.d"},
        {"fsub.s", "fsub.d"},
        {"fmul.s", "fmul.d"},
        {"fdiv.s", "fdiv.d"},
    };
    DB(ss, inst,
       operation == SQRT ? (wide ? "fsqrt.d" : "fsqrt.s")
                         : mnemonics[operation][wide]);
    const auto result =
        arithmetic(operation, rounding, wide, operand(rs1), operand(rs2));
    write_floating_result(rd, wide, result.bits, result.flags);
    return {};
  }
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
    DB(ss, inst,
       funct3 == 0 ? (wide ? "fmin.d" : "fmin.s")
                   : (wide ? "fmax.d" : "fmax.s"));
    const uint64_t lhs = operand(rs1);
    const uint64_t rhs = operand(rs2);
    const uint64_t lhs_class = classify(lhs, wide);
    const uint64_t rhs_class = classify(rhs, wide);
    if ((lhs_class | rhs_class) & SIGNALING_NAN_CLASS)
      accrue_floating_flags(INVALID_FLAG);

    uint64_t result = lhs;
    if (lhs_class & NAN_CLASSES)
      result =
          (rhs_class & NAN_CLASSES) ? (wide ? DOUBLE_NAN : SINGLE_NAN) : rhs;
    else if (!(rhs_class & NAN_CLASSES))
      result = less_than(lhs, rhs, sign_mask) == (funct3 == 0) ? lhs : rhs;

    fregs[rd] = wide ? result : box_single(result);
    cregs->mark_floating_dirty();
    return {};
  }
  case 0x08: { // FCVT.S.D and FCVT.D.S
    if (rounding > RMM || rs2 > 1 || rs2 == format)
      return std::unexpected(Exception::IllegalInstruction);
    DB(ss, inst, wide ? "fcvt.d.s" : "fcvt.s.d");
    const uint64_t source = wide ? single_operand(fregs[rs1]) : fregs[rs1];
    const auto result = convert_precision(source, !wide, wide, rounding);
    write_floating_result(rd, wide, result.bits, result.flags);
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
        accrue_floating_flags(INVALID_FLAG);
      xregs->write(rd, 0);
      return {};
    }

    const bool equal = lhs == rhs || ((lhs | rhs) & ~sign_mask) == 0;
    const bool less = !equal && less_than(lhs, rhs, sign_mask);
    xregs->write(rd, funct3 == 2 ? equal : less || (funct3 == 0 && equal));
    return {};
  }
  case 0x18: // FCVT from floating-point to integer
  case 0x1a: { // FCVT from integer to floating-point
    if (rounding > RMM || rs2 > 3)
      return std::unexpected(Exception::IllegalInstruction);
    if (operation == 0x18) {
      [[maybe_unused]] constexpr const char *mnemonics[][4] = {
          {"fcvt.w.s", "fcvt.wu.s", "fcvt.l.s", "fcvt.lu.s"},
          {"fcvt.w.d", "fcvt.wu.d", "fcvt.l.d", "fcvt.lu.d"},
      };
      DB(ss, inst, mnemonics[wide][rs2]);
      const auto result = convert_to_integer(operand(rs1), rs2, wide, rounding);
      xregs->write(rd, result.bits);
      accrue_floating_flags(result.flags);
    } else {
      [[maybe_unused]] constexpr const char *mnemonics[][4] = {
          {"fcvt.s.w", "fcvt.s.wu", "fcvt.s.l", "fcvt.s.lu"},
          {"fcvt.d.w", "fcvt.d.wu", "fcvt.d.l", "fcvt.d.lu"},
      };
      DB(ss, inst, mnemonics[wide][rs2]);
      const auto result = convert_from_integer(xregs->read(rs1), rs2, wide, rounding);
      write_floating_result(rd, wide, result.bits, result.flags);
    }
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
      xregs->write(rd,
                   wide ? fregs[rs1] : SIGNEXTEND_CAST(fregs[rs1], int32_t));
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
