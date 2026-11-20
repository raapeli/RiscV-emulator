#include "cpu.h"

namespace {

constexpr uint64_t NAN_BOX = 0xffffffff00000000ULL;
constexpr uint32_t SINGLE_NAN = 0x7fc00000;

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
      return 1ULL << (negative ? 0 : 7);
    return 1ULL << ((fraction >> (fraction_width - 1)) ? 9 : 8);
  }
  if (exponent == 0)
    return 1ULL << (fraction == 0 ? (negative ? 3 : 4) : (negative ? 2 : 5));
  return 1ULL << (negative ? 1 : 6);
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
  switch (funct7 >> 2) {
  case 0x04: { // FSGNJ, FSGNJN, FSGNJX
    if (funct3 > 2)
      return std::unexpected(Exception::IllegalInstruction);
    DB(ss, inst, wide ? "fsgnj.d" : "fsgnj.s");
    const uint64_t lhs = wide ? fregs[rs1] : single_operand(fregs[rs1]);
    const uint64_t rhs = wide ? fregs[rs2] : single_operand(fregs[rs2]);
    const uint64_t sign_mask = 1ULL << (wide ? 63 : 31);
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
  case 0x1c: // FMV.X.W, FMV.X.D, FCLASS
    if (rs2 != 0 || funct3 > 1)
      return std::unexpected(Exception::IllegalInstruction);
    if (funct3 == 1) {
      DB(ss, inst, wide ? "fclass.d" : "fclass.s");
      xregs->write(rd, classify(wide ? fregs[rs1] : single_operand(fregs[rs1]), wide));
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
