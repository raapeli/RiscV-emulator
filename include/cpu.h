#pragma once

#include "bus.h"
#include "csrs.h"
#include "exception.h"
#include "interrupt.h"

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <ostream>
#include <sstream>
#include <unordered_set>

#define SIGNEXTEND_CAST2(val, upcast_from)                                     \
  (static_cast<int64_t>(static_cast<upcast_from>(val)))

#define SIGNEXTEND_CAST(val, upcast_from)                                      \
  (static_cast<uint64_t>(SIGNEXTEND_CAST2(val, upcast_from)))

// ---- Bitfield extractors ----
#define OPCODE(inst) ((inst) & 0x7F)
#define RD(inst) (((inst) >> 7) & 0x1F)
#define FUNCT3(inst) (((inst) >> 12) & 0x7)
#define RS1(inst) (((inst) >> 15) & 0x1F)
#define RS2(inst) (((inst) >> 20) & 0x1F)
#define FUNCT7(inst) (((inst) >> 25) & 0x7F)
#define FUNCT5(inst) ((FUNCT7(inst)) >> 2)
// ---- Immediates for different types ----
#define IMM_I(inst) (SIGNEXTEND_CAST2(inst & 0xFFF00000, int32_t) >> 20)
#define IMM_S(inst)                                                            \
  (SIGNEXTEND_CAST2(inst & 0xFE000000, int32_t) >> 20) | ((inst >> 7) & 0x1F)
#define IMM_B(inst)                                                            \
  (SIGNEXTEND_CAST2(inst & 0x80000000, int32_t) >> 19) |                       \
      ((inst & 0x80) << 4) | ((inst >> 20) & 0x7E0) | ((inst >> 7) & 0x1E)
#define IMM_U(inst) ((int64_t)(inst) & 0xFFFFF000)
#define IMM_J(inst)                                                            \
  (SIGNEXTEND_CAST2(inst & 0x80000000, int32_t) >> 11) | (inst & 0xFF000) |    \
      ((inst >> 9) & 0x800) | ((inst >> 20) & 0x7fE)

#ifdef DEBUG

#include <print>

#define DB(ss, inst, name)                                                     \
  do {                                                                         \
    if (ss) {                                                                  \
      std::println(*ss, "[DEBUG] {} Instruction 0x{:08x}", name, inst);        \
      std::println(*ss, "opcode: 0x{:02x}  rd: x{}  rs1: x{}  rs2: x{}",       \
                   OPCODE(inst), RD(inst), RS1(inst), RS2(inst));              \
      std::println(*ss, "funct3: 0x{:02x}  funct7: 0x{:02x} funct5: 0x{:02x}", \
                   FUNCT3(inst), FUNCT7(inst), FUNCT5(inst));                  \
      std::println(*ss, "imm_I: 0x{:08x}  imm_S: 0x{:08x}  imm_B: 0x{:08x}",   \
                   IMM_I(inst), IMM_S(inst), IMM_B(inst));                     \
      std::println(*ss, "imm_U: 0x{:08x}  imm_J: 0x{:08x}", IMM_U(inst),       \
                   IMM_J(inst));                                               \
      std::println(*ss, "PC: 0x{:08x}", pc);                                   \
    }                                                                          \
  } while (0)

#else
#define DB(ss, inst, x) ((void)0)
#endif // DEBUG

#define REGISTER_COUNT 32

const uint8_t BYTE = 8;
const uint8_t HALFWORD = 16;
const uint8_t WORD = 32;
const uint8_t DOUBLEWORD = 64;

class XRegisters {
public:
  void write(uint64_t rd, uint64_t value);
  uint64_t read(uint64_t reg);

private:
  std::array<uint64_t, REGISTER_COUNT> registers = {};
};

enum Mode : uint64_t { USER = 0, SUPERVISOR = 01, MACHINE = 3 };

class Cpu {

public:
  using ExecResult = std::expected<void, Exception::ExceptionValue>;

  Cpu(Bus *bus);
  ExecResult execute(std::stringstream *ss = nullptr);
  // Program counter
  uint64_t pc = DRAM_BASE;
  uint64_t prev_inst = 0L; // Privilege level
  Mode mode = Mode::MACHINE;
  csr::Csr *cregs;

  std::optional<Interrupt::InterruptValue> check_pending_interrupt();

  void dump_registers(std::ostream &stream);

  void start();
  void oneTick(std::stringstream *debug_stream = nullptr);

  Interrupt state;
  Exception exception;
  XRegisters *xregs;
  Bus *bus;

private:
  std::unordered_set<uint64_t> reservations;

  uint64_t fetch();
  ExecResult executeGeneral(uint64_t inst, std::stringstream *ss = nullptr);

  ExecResult exec_OP_IMM_32(uint64_t inst, std::stringstream *ss);
  ExecResult exec_OP_IMM(uint64_t inst, std::stringstream *ss);
  ExecResult exec_OP_32(uint64_t inst, std::stringstream *ss);
  ExecResult exec_OP(uint64_t inst, std::stringstream *ss);
  ExecResult exec_LUI(uint64_t inst, std::stringstream *ss);
  ExecResult exec_AUIPC(uint64_t inst, std::stringstream *ss);
  ExecResult exec_LOAD(uint64_t inst, std::stringstream *ss);
  ExecResult exec_STORE(uint64_t inst, std::stringstream *ss);
  ExecResult exec_JAL(uint64_t inst, std::stringstream *ss);
  ExecResult exec_JALR(uint64_t inst, std::stringstream *ss);
  ExecResult exec_BRANCH(uint64_t inst, std::stringstream *ss);
  ExecResult exec_SYSTEM(uint64_t inst, std::stringstream *ss);
  ExecResult exec_FENCE(uint64_t inst, std::stringstream *ss);
  ExecResult exec_ATOMIC(uint64_t inst, std::stringstream *ss);
};
