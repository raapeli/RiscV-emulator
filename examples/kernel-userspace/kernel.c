#include "syscalls.h"

#include <stdbool.h>
#include <stdint.h>

#define CONSOLE_DATA (*(volatile uint8_t *)0x10000000)

extern const char __user_text_start[], __user_text_end[];
extern const char __user_stack_start[], __user_stack_end[];
extern const char user_store_kernel[], user_store_console[], user_store_boundary[];

volatile uint8_t kernel_guard = 0x5a;

static void print(const char *text) {
  while (*text)
    CONSOLE_DATA = *text++;
}

void kernel_start(void) { print("kernel: entering U-mode\n"); }

static _Noreturn void halt(void) {
  for (;;)
    __asm__ volatile("wfi");
}

static _Noreturn void exit_user(uint8_t status) {
  char digits[3];
  unsigned count = 0;
  do {
    digits[count++] = '0' + status % 10;
    status /= 10;
  } while (status != 0);

  print("kernel: user exited with status ");
  while (count != 0)
    CONSOLE_DATA = digits[--count];
  CONSOLE_DATA = '\n';
  halt();
}

static bool contains_buffer(uintptr_t address, uint64_t length,
                            const char *start, const char *end) {
  return address >= (uintptr_t)start && address < (uintptr_t)end &&
         length <= (uintptr_t)end - address;
}

static int64_t write_user(uint64_t fd, uintptr_t address, uint64_t length) {
  if (fd != STDOUT_FD)
    return -ERR_BAD_FD;
  if (length == 0)
    return 0;
  if (!contains_buffer(address, length, __user_text_start, __user_text_end) &&
      !contains_buffer(address, length, __user_stack_start, __user_stack_end))
    return -ERR_BAD_ADDRESS;

  const uint8_t *buffer = (const uint8_t *)address;
  for (uint64_t i = 0; i < length; ++i)
    CONSOLE_DATA = buffer[i];
  return length;
}

int64_t kernel_trap(uint64_t arg0, uintptr_t arg1, uint64_t arg2,
                    uint64_t syscall, uint64_t cause, uintptr_t pc) {
  if (cause == 7) {
    if (pc == (uintptr_t)user_store_kernel && kernel_guard == 0x5a) {
      print("kernel: blocked user store to kernel RAM\n");
      return arg0;
    }
    if (pc == (uintptr_t)user_store_console) {
      print("kernel: blocked user store to console\n");
      return arg0;
    }
    if (pc == (uintptr_t)user_store_boundary) {
      print("kernel: blocked user store across stack boundary\n");
      return arg0;
    }
  }

  if (cause != 8) {
    print("kernel: unexpected trap\n");
    halt();
  }

  switch (syscall) {
  case SYS_WRITE:
    return write_user(arg0, arg1, arg2);
  case SYS_EXIT:
    exit_user((uint8_t)arg0);
  default:
    return -ERR_UNKNOWN_SYSCALL;
  }
}
