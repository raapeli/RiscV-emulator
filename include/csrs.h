#pragma once

#include <array>
#include <cstdint>

namespace csr {

const uint32_t MXLEN = 64;
// Count of CSRs
const uint64_t CSR_SIZE = 4096;

struct Address {
  enum AddressValues : uint64_t {
    // Unprivileged floating‑point CSRs
    FFLAGS = 0x001,
    FRM = 0x002,
    FCSR = 0x003,

    // Unprivileged counters/timers
    CYCLE = 0xC00,
    TIME = 0xC01,
    INSTRET = 0xC02,
    HPMCOUNTER3 = 0xC03,
    // ...
    HPMCOUNTER31 = 0xC1F,

    // RV32 high halves
    CYCLEH = 0xC80,
    TIMEH = 0xC81,
    INSTRETH = 0xC82,
    HPMCOUNTER3H = 0xC83,
    // ...
    HPMCOUNTER31H = 0xC9F,

    // Supervisor CSRs
    SSTATUS = 0x100,
    SIE = 0x104,
    STVEC = 0x105,
    SCOUNTEREN = 0x106,
    SENVCFG = 0x10A,

    SSCRATCH = 0x140,
    SEPC = 0x141,
    SCAUSE = 0x142,
    STVAL = 0x143,
    SIP = 0x144,

    SATP = 0x180,
    SCONTEXT = 0x5A8,

    // Machine info
    MVENDORID = 0xF11,
    MARCHID = 0xF12,
    MIMPID = 0xF13,
    MHARTID = 0xF14,
    MCONFIGPTR = 0xF15,

    // Machine trap setup
    MSTATUS = 0x300,
    MISA = 0x301,
    MEDELEG = 0x302,
    MIDELEG = 0x303,
    MIE = 0x304,
    MTVEC = 0x305,
    MCOUNTEREN = 0x306,
    MSTATUSH = 0x310, // RV32 only

    // Machine trap handling
    MSCRATCH = 0x340,
    MEPC = 0x341,
    MCAUSE = 0x342,
    MTVAL = 0x343,
    MIP = 0x344,
    MTINST = 0x34A,
    MTVAL2 = 0x34B,

    // Machine configuration
    MENVCFG = 0x30A,
    MENVCFGH = 0x31A, // RV32 only

    // Machine counters
    MCYCLE = 0xB00,
    MINSTRET = 0xB02,
    MHPMCOUNTER3 = 0xB03,
    // ...
    MHPMCOUNTER31 = 0xB1F,

    MCYCLEH = 0xB80,       // RV32 only
    MINSTRETH = 0xB82,     // RV32 only
    MHPMCOUNTER3H = 0xB83, // RV32 only
    // ...
    MHPMCOUNTER31H = 0xB9F, // RV32 only

    // Machine performance event selectors
    MHPMEVENT3 = 0x323,
    // ...
    MHPMEVENT31 = 0x33F,

    // Debug CSRs (names/usage depend on debug spec; addresses 0x7A0–0x7BF)
    // Add as needed.
  };
};

struct Mask {
  enum SSTATUS_MASK : uint64_t {
    SIE = 1U << 1U,
    SPIE = 1U << 5U,
    UBE = 1U << 6U,
    SPP = 1U << 8U,
    FS = 0x6000U,
    XS = 0x18000U,
    SUM = 1U << 18U,
    MXR = 1U << 19U,
    UXL = 0x300000000ULL,
    SD = 1ULL << 63ULL,

    SSTATUS = SIE | SPIE | UBE | SPP | FS | XS | SUM | MXR | UXL | SD,
  };
  enum class SSTATUSBit : uint64_t { SIE = 1, SPIE = 5, SPP = 8 };

  enum class MSTATUSBit : uint64_t {
    MIE = 3,
    MPIE = 7,
    MPP = 12,
    MPRV = 17,
    SUM = 18,
    MXR = 19,
    TVM = 20,
    TSR = 22,
  };
};

struct Misa {
  enum Extension : uint64_t {
    A_EXT = 1U << 0U,
    C_EXT = 1U << 2U,
    D_EXT = 1U << 3U,
    RV32E = 1U << 4U,
    F_EXT = 1U << 5U,
    HYPERVISOR = 1U << 7U,
    RV32I_64I_128I = 1U << 8U,
    M_EXT = 1U << 12U,
    N_EXT = 1U << 13U,
    QUAD_EXT = 1U << 16U,
    SUPERVISOR = 1U << 18U,
    USER = 1U << 20U,
    NON_STD_PRESENT = 1U << 22U,

    XLEN_32 = 1U << 31U,
    XLEN_64 = 2ULL << 62U
  };
};

class Csr {
public:
  Csr();

  uint64_t load(uint64_t addr);
  void store(uint64_t addr, uint64_t value);

  uint64_t read_bit(uint64_t addr, uint64_t offset);
  uint64_t read_bits(uint64_t addr, uint64_t upper_offset,
                     uint64_t lower_offset);

  void write_bit(uint64_t addr, uint64_t offset, uint64_t value);
  void write_bits(uint64_t addr, uint64_t upper_offset, uint64_t lower_offset,
                  uint64_t value);

  uint64_t read_bit_mstatus(Mask::MSTATUSBit bit);
  void write_bit_mstatus(Mask::MSTATUSBit bit, uint64_t value);

  uint64_t read_bit_sstatus(Mask::SSTATUSBit bit);
  void write_bit_sstatus(Mask::SSTATUSBit bit, uint64_t value);

  std::array<uint64_t, CSR_SIZE> regs = {};
};

} // namespace csr
