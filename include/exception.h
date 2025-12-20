#pragma once

#include <cstdint>

class Cpu;

enum class Trap : uint32_t {
  // Visible and handled inside execution environment
  Contained,
  // Requesting action on behalf of software inside the execution environment
  Requested,
  // Handled transparently by the execution environment
  Invisible,
  // A fatal failure that causes termination of execution
  Fatal
};

struct Exception {
  enum ExceptionValue : uint32_t {
    // Synchronous exceptions (Interrupt bit = 0)
    InstructionAddressMisaligned = 0,
    InstructionAccessFault = 1,
    IllegalInstruction = 2,
    Breakpoint = 3,
    LoadAddressMisaligned = 4,
    LoadAccessFault = 5,
    StoreAmoAddressMisaligned = 6,
    StoreAmoAccessFault = 7,
    EnvironmentCallUmode = 8,
    EnvironmentCallSmode = 9,
    // 10 reserved
    EnvironmentCallMmode = 11,
    InstructionPageFault = 12,
    LoadPageFault = 13,
    // 14 reserved
    StoreAmoPageFault = 15,
  };

  ExceptionValue exception;

  uint32_t epc(uint32_t pc);
  uint32_t trap_value(uint32_t pc);
  Trap take_trap(Cpu *cpu);
};
