#include "cpu.h"
#include <cstdint>

// 0x1B: Integer register-immediate w-versions (addiw, slliw, etc.)
Cpu::ExecResult Cpu::exec_OP_IMM_32(uint64_t inst, std::stringstream *ss) {
  int64_t imm = IMM_I(inst);
  uint64_t reg1 = this->xregs->read(RS1(inst));
  uint64_t result;
  uint64_t funct3 = FUNCT3(inst);
  uint64_t funct7 = FUNCT7(inst);

  switch (funct3) {
  case 0x0: { // addiw
    DB(ss, inst, "addiw");
    result = SIGNEXTEND_CAST(reg1 + imm, int32_t);
    break;
  }
  case 0x1: { // slliw
    DB(ss, inst, "slliw");
    result = SIGNEXTEND_CAST(reg1 << (uint64_t)(imm & 0x1F), int32_t);
    break;
  }
  case 0x5: {             // srliw or sraiw
    if (funct7 == 0x20) { // sraiw
      DB(ss, inst, "sraiw");
      result = SIGNEXTEND_CAST(
          static_cast<int32_t>(reg1) >> (uint32_t)(imm & 0x1F), int64_t);
    } else if (funct7 == 0) {
      DB(ss, inst, "srliw");
      result = SIGNEXTEND_CAST(
          static_cast<uint32_t>(reg1) >> (uint32_t)(imm & 0x1F), int32_t);
    } else {
      return std::unexpected(Exception::IllegalInstruction);
    }
    break;
  }
  default:
    return std::unexpected(Exception::IllegalInstruction);
  }
  this->xregs->write(RD(inst), result);
  return {};
}

// 0x13: Integer register-immediate (addi, slti, etc.)
Cpu::ExecResult Cpu::exec_OP_IMM(uint64_t inst, std::stringstream *ss) {
  uint64_t imm = IMM_I(inst);
  uint64_t reg1 = this->xregs->read(RS1(inst));
  uint64_t result;
  uint64_t funct3 = FUNCT3(inst);
  uint64_t funct7 = FUNCT7(inst);

  switch (funct3) {
  case 0x0: { // addi
    DB(ss, inst, "addi");
    result = reg1 + static_cast<int64_t>(imm);
    break;
  }
  case 0x2: { // slti
    DB(ss, inst, "slti");
    result = ((int64_t)reg1 < (int64_t)imm);
    break;
  }
  case 0x3: { // sltiu
    DB(ss, inst, "sltiu");
    result = (reg1 < (uint64_t)imm);
    break;
  }
  case 0x4: { // xori
    DB(ss, inst, "XORI");
    result = reg1 ^ imm;
    break;
  }
  case 0x6: { // ori
    DB(ss, inst, "ori");
    result = reg1 | imm;
    break;
  }
  case 0x7: { // andi
    DB(ss, inst, "andi");
    result = reg1 & imm;
    break;
  }
  case 0x1: { // slli
    DB(ss, inst, "slli");
    result = reg1 << static_cast<uint32_t>(imm & 0x3F);
    break;
  }
  case 0x5: { // srli or srai
    if (funct7 == 0x20) {
      DB(ss, inst, "srai");
      result = static_cast<int64_t>(reg1) >> imm;
    } else if (funct7 == 0) {
      DB(ss, inst, "srli");
      result = reg1 >> static_cast<uint32_t>(imm & 0x3F);
    } else {
      return std::unexpected(Exception::IllegalInstruction);
    }
    break;
  }
  default:
    return std::unexpected(Exception::IllegalInstruction);
  }
  this->xregs->write(RD(inst), result);
  return {};
}

// 0x3B: Integer register-register w-versions
Cpu::ExecResult Cpu::exec_OP_32(uint64_t inst, std::stringstream *ss) {
  uint64_t reg1 = this->xregs->read(RS1(inst));
  uint64_t reg2 = this->xregs->read(RS2(inst));
  uint64_t funct3 = FUNCT3(inst);
  uint64_t funct7 = FUNCT7(inst);
  uint64_t result;

  if (funct7 & 0x1) { // Multiplication (M-extension)
    switch (funct3) {
    case 0x0:
      DB(ss, inst, "mulw");
      result = SIGNEXTEND_CAST(reg1 * (int64_t)reg2, int32_t);
      break;
    case 0x4:
      DB(ss, inst, "divw");
      if (reg2 == 0)
        result = ~0ULL;
      else if ((int32_t)reg1 == INT32_MIN && (int32_t)reg2 == -1)
        result = INT32_MIN;
      else
        result = SIGNEXTEND_CAST((int32_t)reg1 / (int32_t)reg2, int64_t);
      break;
    case 0x5:
      DB(ss, inst, "divuw");
      if (reg2 == 0)
        result = ~0ULL;
      else
        result = SIGNEXTEND_CAST((uint32_t)reg1 / (uint32_t)reg2, int32_t);
      break;
    case 0x6:
      DB(ss, inst, "remw");
      if (reg2 == 0)
        result = SIGNEXTEND_CAST((int32_t)reg1, int32_t);
      else
        result = SIGNEXTEND_CAST(
            (int64_t)static_cast<int32_t>(reg1) % (int64_t)reg2, int32_t);
      break;
    case 0x7:
      DB(ss, inst, "remuw");
      if (reg2 == 0)
        result = SIGNEXTEND_CAST((int32_t)reg1, int32_t);
      else
        result = reg1 % (int64_t)reg2;
      break;
    default:
      return std::unexpected(Exception::IllegalInstruction);
    }
  } else { // Standard Int Ops
    switch (funct3) {
    case 0x0:
      if (funct7 == 0) {
        DB(ss, inst, "addw");
        result = SIGNEXTEND_CAST(reg1 + (int64_t)reg2, int32_t);
      } else if (funct7 == 0x20) {
        DB(ss, inst, "subw");
        result = SIGNEXTEND_CAST(reg1 - (int64_t)reg2, int32_t);
      } else
        return std::unexpected(Exception::IllegalInstruction);
      break;
    case 0x1:
      DB(ss, inst, "sllw");
      result = SIGNEXTEND_CAST(reg1 << (reg2 & 0x1F), int32_t);
      break;
    case 0x5:
      if (funct7 == 0) {
        DB(ss, inst, "srlw");
        result = SIGNEXTEND_CAST((uint32_t)reg1 >> (reg2 & 0x1F), int32_t);
      } else if (funct7 == 0x20) {
        DB(ss, inst, "sraw");
        result = SIGNEXTEND_CAST((int32_t)reg1 >> (reg2 & 0x1F), int32_t);
      } else
        return std::unexpected(Exception::IllegalInstruction);
      break;
    default:
      return std::unexpected(Exception::IllegalInstruction);
    }
  }
  this->xregs->write(RD(inst), result);
  return {};
}

// 0x33: Integer register-register + M extension
Cpu::ExecResult Cpu::exec_OP(uint64_t inst, std::stringstream *ss) {
  uint64_t reg1 = this->xregs->read(RS1(inst));
  uint64_t reg2 = this->xregs->read(RS2(inst));
  uint64_t funct3 = FUNCT3(inst);
  uint64_t funct7 = FUNCT7(inst);
  uint64_t result;

  if (funct7 & 0x1) { // M-Extension
    switch (funct3) {
    case 0x0:
      DB(ss, inst, "mul");
      result = (int64_t)((int64_t)reg1 * (int64_t)reg2);
      break;
    case 0x1:
      DB(ss, inst, "mulh");
      result = ((__int128_t)(int64_t)reg1 * (__int128_t)(int64_t)reg2) >> 64;
      break;
    case 0x2:
      DB(ss, inst, "mulhsu");
      result =
          ((__uint128_t)(__int128_t)(int64_t)reg1 * (__uint128_t)reg2) >> 64;
      break;
    case 0x3:
      DB(ss, inst, "mulhu");
      result = ((__uint128_t)reg1 * (__uint128_t)reg2) >> 64;
      break;
    case 0x4:
      DB(ss, inst, "div");
      if (reg2 == 0)
        result = -1;
      else if ((int64_t)reg1 == INT64_MIN && (int64_t)reg2 == -1)
        result = INT64_MIN;
      else
        result = (int64_t)reg1 / (int64_t)reg2;
      break;
    case 0x5:
      DB(ss, inst, "divu");
      result = (reg2 == 0) ? UINT64_MAX : reg1 / reg2;
      break;
    case 0x6:
      DB(ss, inst, "rem");
      if (reg2 == 0)
        result = reg1;
      else if ((int64_t)reg1 == INT64_MIN && (int64_t)reg2 == -1)
        result = 0;
      else
        result = (int64_t)reg1 % (int64_t)reg2;
      break;
    case 0x7:
      DB(ss, inst, "remu");
      result = (reg2 == 0) ? reg1 : reg1 % reg2;
      break;
    default:
      return std::unexpected(Exception::IllegalInstruction);
    }
  } else { // Base Integer Ops
    switch (funct3) {
    case 0x0:
      if (funct7 == 0) {
        DB(ss, inst, "add");
        result = static_cast<int64_t>(reg1) + static_cast<int64_t>(reg2);
      } else if (funct7 == 0x20) {
        DB(ss, inst, "sub");
        result = static_cast<int64_t>(reg1) - static_cast<int64_t>(reg2);
      } else
        return std::unexpected(Exception::IllegalInstruction);
      break;
    case 0x1:
      DB(ss, inst, "sll");
      result = reg1 << (reg2 & 0x3F);
      break;
    case 0x2:
      DB(ss, inst, "slt");
      result = ((int64_t)reg1 < (int64_t)reg2) ? 1 : 0;
      break;
    case 0x3:
      DB(ss, inst, "sltu");
      result = (reg1 < reg2) ? 1 : 0;
      break;
    case 0x4:
      DB(ss, inst, "xor");
      result = reg1 ^ reg2;
      break;
    case 0x5:
      if (funct7 == 0) {
        DB(ss, inst, "srl");
        result = reg1 >> (reg2 & 0x3F);
      } else if (funct7 == 0x20) {
        DB(ss, inst, "sra");
        result = (int64_t)reg1 >> (reg2 & 0x3F);
      } else
        return std::unexpected(Exception::IllegalInstruction);
      break;
    case 0x6:
      DB(ss, inst, "or");
      result = reg1 | reg2;
      break;
    case 0x7:
      DB(ss, inst, "and");
      result = reg1 & reg2;
      break;
    default:
      return std::unexpected(Exception::IllegalInstruction);
    }
  }
  this->xregs->write(RD(inst), result);
  return {};
}

Cpu::ExecResult Cpu::exec_LUI(uint64_t inst, std::stringstream *ss) {
  DB(ss, inst, "lui");
  this->xregs->write(RD(inst), SIGNEXTEND_CAST(inst & 0xFFFFF000, int32_t));
  return {};
}

Cpu::ExecResult Cpu::exec_AUIPC(uint64_t inst, std::stringstream *ss) {
  DB(ss, inst, "auipc");
  int64_t imm = SIGNEXTEND_CAST(IMM_U(inst), int32_t);
  this->xregs->write(RD(inst), this->pc + imm);
  return {};
}

Cpu::ExecResult Cpu::exec_LOAD(uint64_t inst, std::stringstream *ss) {
  uint64_t reg1 = this->xregs->read(RS1(inst));
  int64_t imm = IMM_I(inst);
  uint64_t addr = reg1 + imm;
  uint64_t funct3 = FUNCT3(inst);

  switch (funct3) {
  case 0x0:
    DB(ss, inst, "lb");
    this->xregs->write(RD(inst), (int64_t)(int8_t)this->bus->read(addr, BYTE));
    break;
  case 0x1:
    DB(ss, inst, "lh");
    this->xregs->write(RD(inst),
                       (int64_t)(int16_t)this->bus->read(addr, HALFWORD));
    break;
  case 0x2:
    DB(ss, inst, "lw");
    this->xregs->write(RD(inst), (int64_t)(int32_t)this->bus->read(addr, WORD));
    break;
  case 0x4:
    DB(ss, inst, "lbu");
    this->xregs->write(RD(inst), this->bus->read(addr, BYTE));
    break;
  case 0x5:
    DB(ss, inst, "lhu");
    this->xregs->write(RD(inst), this->bus->read(addr, HALFWORD));
    break;
  case 0x6:
    DB(ss, inst, "lwu");
    this->xregs->write(RD(inst), (this->bus->read(addr, WORD) & 0xFFFFFFFF));
    break;
  case 0x3:
    DB(ss, inst, "ld");
    this->xregs->write(RD(inst), this->bus->read(addr, DOUBLEWORD));
    break;
  default:
    return std::unexpected(Exception::IllegalInstruction);
  }
  return {};
}

Cpu::ExecResult Cpu::exec_STORE(uint64_t inst, std::stringstream *ss) {
  int64_t imm = IMM_S(inst);
  uint64_t reg1 = this->xregs->read(RS1(inst));
  uint64_t reg2 = this->xregs->read(RS2(inst));
  uint64_t dest = reg1 + imm;
  uint64_t funct3 = FUNCT3(inst);

  switch (funct3) {
  case 0x0:
    DB(ss, inst, "sb");
    this->bus->write(dest, BYTE, reg2 & 0xff);
    break;
  case 0x1:
    DB(ss, inst, "sh");
    this->bus->write(dest, HALFWORD, reg2 & 0xffff);
    break;
  case 0x2:
    DB(ss, inst, "sw");
    this->bus->write(dest, WORD, reg2);
    break;
  case 0x3:
    DB(ss, inst, "sd");
    this->bus->write(dest, DOUBLEWORD, reg2);
    break;
  default:
    return std::unexpected(Exception::IllegalInstruction);
  }
  return {};
}

Cpu::ExecResult Cpu::exec_JAL(uint64_t inst, std::stringstream *ss) {
  DB(ss, inst, "jal");
  int64_t offset = IMM_J(inst);
  this->xregs->write(RD(inst), this->pc + 4);
  this->pc += offset - 4; // -4 because execute() loop adds +4
  return {};
}

Cpu::ExecResult Cpu::exec_JALR(uint64_t inst, std::stringstream *ss) {
  DB(ss, inst, "jalr");
  int64_t imm = IMM_I(inst);
  uint64_t addr = (this->xregs->read(RS1(inst)) + imm) & ~1;
  this->xregs->write(RD(inst), this->pc + 4);
  this->pc = addr - 4; // -4 because execute() loop adds +4
  return {};
}

Cpu::ExecResult Cpu::exec_BRANCH(uint64_t inst, std::stringstream *ss) {
  int64_t imm = IMM_B(inst);
  uint64_t reg1 = this->xregs->read(RS1(inst));
  uint64_t reg2 = this->xregs->read(RS2(inst));
  bool take = false;
  uint64_t funct3 = FUNCT3(inst);

  switch (funct3) {
  case 0x0:
    DB(ss, inst, "beq");
    take = (reg1 == reg2);
    break;
  case 0x1:
    DB(ss, inst, "bne");
    take = reg1 != reg2;
    break;
  case 0x4:
    DB(ss, inst, "blt");
    take = ((int64_t)reg1 < (int64_t)reg2);
    break;
  case 0x5:
    DB(ss, inst, "bge");
    take = ((int64_t)reg1 >= (int64_t)reg2);
    break;
  case 0x6:
    DB(ss, inst, "bltu");
    take = (reg1 < reg2);
    break;
  case 0x7:
    DB(ss, inst, "bgeu");
    take = (reg1 >= reg2);
    break;
  default:
    return std::unexpected(Exception::IllegalInstruction);
  }
  if (take) {
    this->pc += imm - 4; // -4 because execute() loop adds +4
  }
  return {};
}

Cpu::ExecResult Cpu::exec_SYSTEM(uint64_t inst, std::stringstream *ss) {
  uint64_t funct3 = FUNCT3(inst);
  uint64_t csrAddr =
      IMM_I(inst) & 0xFFF; // Extract CSR addr from immediate field

  if (funct3 == 0x0) {    // ecall, ebreak, sret, mret
    if (RS2(inst) == 1) { // ebreak
      DB(ss, inst, "ebreak");
      return std::unexpected(Exception::Breakpoint);
    } else if (RS2(inst) == 0) { // ecall
      DB(ss, inst, "ecall");
      switch (mode) {
      case Mode::MACHINE:
        return std::unexpected(Exception::EnvironmentCallMmode);
      case Mode::SUPERVISOR:
        return std::unexpected(Exception::EnvironmentCallSmode);
      case Mode::USER:
        return std::unexpected(Exception::EnvironmentCallUmode);
      }
    } else if (FUNCT7(inst) == 0x18) { // mret
      DB(ss, inst, "mret");
      if (this->mode != Mode::MACHINE)
        return std::unexpected(Exception::IllegalInstruction);

      this->pc = cregs->load(csr::Address::MEPC) - 4; // Restore PC

      // Restore Privilege Mode
      uint64_t mpp = cregs->read_bit_mstatus(csr::Mask::MSTATUSBit::MPP);
      cregs->write_bit_mstatus(csr::Mask::MSTATUSBit::MPRV, 0);
      if (mpp == 0b00)
        this->mode = Mode::USER;
      else if (mpp == 0b01)
        this->mode = Mode::SUPERVISOR;
      else if (mpp == 0b11)
        this->mode = Mode::MACHINE;

      // Restore Interrupt Enable
      cregs->write_bit_mstatus(
          csr::Mask::MSTATUSBit::MIE,
          cregs->read_bit_mstatus(csr::Mask::MSTATUSBit::MPIE));
      cregs->write_bit_mstatus(csr::Mask::MSTATUSBit::MPIE, 1);
      cregs->write_bit_mstatus(csr::Mask::MSTATUSBit::MPP, Mode::USER);
    } else {
      return std::unexpected(Exception::IllegalInstruction);
    }
  } else { // CSR Instructions
    uint64_t rs1Val = xregs->read(RS1(inst));
    uint64_t rd = RD(inst);
    uint64_t csrVal = cregs->load(csrAddr);

    switch (funct3) {
    case 0x1:
      DB(ss, inst, "csrrw"); // csrrw
      if (rd != 0)
        xregs->write(rd, csrVal);
      cregs->store(csrAddr, rs1Val);
      break;
    case 0x2:
      DB(ss, inst, "csrrs"); // csrrs
      if (RS1(inst) != 0)
        cregs->store(csrAddr, csrVal | rs1Val);
      xregs->write(rd, csrVal);
      break;
    case 0x3:
      DB(ss, inst, "csrrc"); // csrrc
      if (RS1(inst) != 0)
        cregs->store(csrAddr, csrVal & ~rs1Val);
      xregs->write(rd, csrVal);
      break;
    case 0x5:
      DB(ss, inst, "csrrwi"); // csrrwi
      if (rd != 0)
        xregs->write(rd, csrVal);
      cregs->store(csrAddr, RS1(inst)); // zimm is in rs1 field
      break;
    case 0x6:
      DB(ss, inst, "csrrsi"); // csrrsi
      if (RS1(inst) != 0)
        cregs->store(csrAddr, csrVal | RS1(inst));
      xregs->write(rd, csrVal);
      break;
    case 0x7:
      DB(ss, inst, "csrrci"); // csrrci
      if (RS1(inst) != 0)
        cregs->store(csrAddr, csrVal & ~RS1(inst));
      xregs->write(rd, csrVal);
      break;
    default:
      return std::unexpected(Exception::IllegalInstruction);
    }
  }
  return {};
}

Cpu::ExecResult Cpu::exec_FENCE(uint64_t inst, std::stringstream *ss) {
  uint64_t funct3 = FUNCT3(inst);
  if (funct3 == 1) {
    DB(ss, inst, "fence.i");
  } else if (funct3 == 0) {
    DB(ss, inst, "fence");
  } else {
    return std::unexpected(Exception::IllegalInstruction);
  }
  return {};
}
