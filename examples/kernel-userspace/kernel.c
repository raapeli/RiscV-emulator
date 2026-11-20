#include <stdint.h>

#define CONSOLE_DATA (*(volatile uint8_t *)0x10000000)

extern const char user_ecall[];

static void print(const char *text) {
  while (*text)
    CONSOLE_DATA = *text++;
}

void kernel_start(void) { print("kernel: entering U-mode\n"); }

_Noreturn void kernel_trap(uint64_t cause, uintptr_t pc) {
  if (cause == 8 && pc == (uintptr_t)user_ecall)
    print("kernel: received U-mode ecall\n");
  else
    print("kernel: unexpected trap\n");

  for (;;)
    __asm__ volatile("wfi");
}
