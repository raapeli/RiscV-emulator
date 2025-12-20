#pragma once

class Cpu;

struct Interrupt {
  enum InterruptValue {
    UserSoftwareInterrupt,
    SupervisorSoftwareInterrupt,
    MachineSoftwareInterrupt,
    UserTimerInterrupt,
    SupervisorTimerInterrupt,
    MachineTimerInterrupt,
    UserExternalInterrupt,
    SupervisorExternalInterrupt,
    MachineExternalInterrupt,
  };

  InterruptValue cause;
  void take_trap(Cpu *cpu);
};
