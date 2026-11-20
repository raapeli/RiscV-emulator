#include "cpu.h"

namespace {

// Pick a field from a compressed instruction and place it in an immediate.
constexpr uint32_t field(uint16_t inst, unsigned high, unsigned low,
                         unsigned dest = 0) {
  return ((inst >> low) & ((1U << (high - low + 1)) - 1)) << dest;
}

constexpr uint32_t sign_extend(uint32_t value, unsigned width) {
  const uint32_t sign = 1U << (width - 1);
  return (value ^ sign) - sign;
}

constexpr uint32_t i_type(uint32_t opcode, uint32_t funct3, uint32_t rd,
                          uint32_t rs1, uint32_t imm) {
  return ((imm & 0xfff) << 20) | (rs1 << 15) | (funct3 << 12) |
         (rd << 7) | opcode;
}

constexpr uint32_t store(uint32_t funct3, uint32_t rs1, uint32_t rs2,
                         uint32_t imm) {
  return ((imm >> 5) << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) |
         ((imm & 31) << 7) | 0x23;
}

constexpr uint32_t r_type(uint32_t opcode, uint32_t funct7, uint32_t funct3,
                          uint32_t rd, uint32_t rs1, uint32_t rs2) {
  return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) |
         (rd << 7) | opcode;
}

constexpr uint32_t jump(uint32_t imm) {
  return ((imm & 0x100000) << 11) | ((imm & 0x7fe) << 20) |
         ((imm & 0x800) << 9) | (imm & 0xff000) | 0x6f;
}

constexpr uint32_t branch_zero(uint32_t funct3, uint32_t rs1, uint32_t imm) {
  return ((imm & 0x1000) << 19) | ((imm & 0x7e0) << 20) | (rs1 << 15) |
         (funct3 << 12) | ((imm & 0x1e) << 7) | ((imm & 0x800) >> 4) | 0x63;
}

} // namespace

std::optional<uint32_t> Cpu::expand_compressed(uint16_t inst) {
  const uint32_t funct3 = field(inst, 15, 13);
  const uint32_t rd = field(inst, 11, 7);
  const uint32_t rs2 = field(inst, 6, 2);
  const uint32_t short_rd = 8 + field(inst, 4, 2);
  const uint32_t short_rs1 = 8 + field(inst, 9, 7);
  const uint32_t imm6 = field(inst, 12, 12, 5) | rs2;

  switch (inst & 3) {
  case 0: {
    const uint32_t offset = field(inst, 12, 10, 3);
    const uint32_t word_offset = offset | field(inst, 6, 6, 2) |
                                 field(inst, 5, 5, 6);
    const uint32_t double_offset = offset | field(inst, 6, 5, 6);
    switch (funct3) {
    case 0: { // C.ADDI4SPN
      uint32_t imm = field(inst, 12, 11, 4) | field(inst, 10, 7, 6) |
                     field(inst, 6, 6, 2) | field(inst, 5, 5, 3);
      if (imm == 0)
        return std::nullopt;
      return i_type(0x13, 0, short_rd, 2, imm);
    }
    case 2: // C.LW
      return i_type(0x03, 2, short_rd, short_rs1, word_offset);
    case 3: // C.LD
      return i_type(0x03, 3, short_rd, short_rs1, double_offset);
    case 6: // C.SW
      return store(2, short_rs1, short_rd, word_offset);
    case 7: // C.SD
      return store(3, short_rs1, short_rd, double_offset);
    default:
      return std::nullopt;
    }
  }
  case 1:
    switch (funct3) {
    case 0: // C.ADDI and C.NOP
      return i_type(0x13, 0, rd, rd, sign_extend(imm6, 6));
    case 1: // C.ADDIW
      if (rd == 0)
        return std::nullopt;
      return i_type(0x1b, 0, rd, rd, sign_extend(imm6, 6));
    case 2: // C.LI
      return i_type(0x13, 0, rd, 0, sign_extend(imm6, 6));
    case 3:
      if (imm6 == 0)
        return std::nullopt;
      if (rd == 2) { // C.ADDI16SP
        const uint32_t imm = field(inst, 12, 12, 9) | field(inst, 6, 6, 4) |
                             field(inst, 5, 5, 6) | field(inst, 4, 3, 7) |
                             field(inst, 2, 2, 5);
        return i_type(0x13, 0, 2, 2, sign_extend(imm, 10));
      }
      return (sign_extend(imm6, 6) << 12) | (rd << 7) | 0x37; // C.LUI
    case 4:
      switch (field(inst, 11, 10)) {
      case 0: // C.SRLI
        return i_type(0x13, 5, short_rs1, short_rs1, imm6);
      case 1: // C.SRAI
        return i_type(0x13, 5, short_rs1, short_rs1, 0x400 | imm6);
      case 2: // C.ANDI
        return i_type(0x13, 7, short_rs1, short_rs1, sign_extend(imm6, 6));
      default: {
        const uint32_t operation = field(inst, 6, 5);
        if (field(inst, 12, 12)) { // C.SUBW and C.ADDW
          if (operation > 1)
            return std::nullopt;
          return r_type(0x3b, operation == 0 ? 0x20 : 0, 0, short_rs1,
                        short_rs1, short_rd);
        }
        constexpr uint32_t alu_funct3[] = {0, 4, 6, 7}; // SUB, XOR, OR, AND
        return r_type(0x33, operation == 0 ? 0x20 : 0, alu_funct3[operation],
                      short_rs1, short_rs1, short_rd);
      }
      }
    case 5: { // C.J
      const uint32_t imm = field(inst, 12, 12, 11) | field(inst, 11, 11, 4) |
                           field(inst, 10, 9, 8) | field(inst, 8, 8, 10) |
                           field(inst, 7, 7, 6) | field(inst, 6, 6, 7) |
                           field(inst, 5, 3, 1) | field(inst, 2, 2, 5);
      return jump(sign_extend(imm, 12));
    }
    default: { // C.BEQZ and C.BNEZ
      const uint32_t imm = field(inst, 12, 12, 8) | field(inst, 11, 10, 3) |
                           field(inst, 6, 5, 6) | field(inst, 4, 3, 1) |
                           field(inst, 2, 2, 5);
      return branch_zero(funct3 - 6, short_rs1, sign_extend(imm, 9));
    }
    }
  case 2:
    switch (funct3) {
    case 0: // C.SLLI
      return i_type(0x13, 1, rd, rd, imm6);
    case 2: { // C.LWSP
      if (rd == 0)
        return std::nullopt;
      uint32_t offset = field(inst, 12, 12, 5) | field(inst, 6, 4, 2) |
                        field(inst, 3, 2, 6);
      return i_type(0x03, 2, rd, 2, offset);
    }
    case 3: { // C.LDSP
      if (rd == 0)
        return std::nullopt;
      uint32_t offset = field(inst, 12, 12, 5) | field(inst, 6, 5, 3) |
                        field(inst, 4, 2, 6);
      return i_type(0x03, 3, rd, 2, offset);
    }
    case 4: {
      const bool high = field(inst, 12, 12);
      if (rs2 != 0) // C.MV and C.ADD
        return r_type(0x33, 0, 0, rd, high ? rd : 0, rs2);
      if (rd != 0) // C.JR and C.JALR
        return i_type(0x67, 0, high ? 1 : 0, rd, 0);
      if (high) // C.EBREAK
        return i_type(0x73, 0, 0, 0, 1);
      return std::nullopt;
    }
    case 6: // C.SWSP
      return store(2, 2, rs2, field(inst, 12, 9, 2) | field(inst, 8, 7, 6));
    case 7: // C.SDSP
      return store(3, 2, rs2, field(inst, 12, 10, 3) | field(inst, 9, 7, 6));
    default:
      return std::nullopt;
    }
  default:
    return std::nullopt;
  }
}
