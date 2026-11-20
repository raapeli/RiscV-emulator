#include "syscalls.h"

.section .text.init, "ax", @progbits
.globl _start
_start:
    lla sp, kernel_stack_top
    call kernel_start

    lla t0, trap_entry
    csrw mtvec, t0
    csrw mscratch, sp

    # Use TOR entries for user text (RX) and stack (RW).
    lla t0, __user_text_start
    srli t0, t0, 2
    csrw pmpaddr0, t0
    lla t0, __user_text_end
    srli t0, t0, 2
    csrw pmpaddr1, t0
    lla t0, __user_stack_start
    srli t0, t0, 2
    csrw pmpaddr2, t0
    lla t0, __user_stack_end
    srli t0, t0, 2
    csrw pmpaddr3, t0
    li t0, (0x0b << 24) | (0x0d << 8)
    csrw pmpcfg0, t0

    lla t0, user_start
    csrw mepc, t0
    li t0, (3 << 11)  # Clear mstatus.MPP to select U-mode.
    csrc mstatus, t0
    lla sp, user_stack_top
    mret

.macro transfer_caller_saved op
    \op ra, 0(sp)
    \op t0, 8(sp)
    \op t1, 16(sp)
    \op t2, 24(sp)
    \op t3, 32(sp)
    \op t4, 40(sp)
    \op t5, 48(sp)
    \op t6, 56(sp)
    \op a1, 64(sp)
    \op a2, 72(sp)
    \op a3, 80(sp)
    \op a4, 88(sp)
    \op a5, 96(sp)
    \op a6, 104(sp)
    \op a7, 112(sp)
.endm

.balign 4
trap_entry:
    csrrw sp, mscratch, sp
    addi sp, sp, -128
    transfer_caller_saved sd
    mv a3, a7
    csrr a4, mcause
    csrr a5, mepc
    call kernel_trap

    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0
    transfer_caller_saved ld
    addi sp, sp, 128
    csrrw sp, mscratch, sp
    mret

.section .user.rodata, "a", @progbits
user_message:
    .ascii "userspace: hello\n"
.set user_message_size, . - user_message

.section .user.text, "ax", @progbits
.balign 4
user_start:
    addi sp, sp, -16
    li t0, 42
    sd t0, 0(sp)

    lla t1, kernel_guard
.globl user_store_kernel
user_store_kernel:
    sb zero, 0(t1)

    li t1, 0x10000000
    li t2, '!'
.globl user_store_console
user_store_console:
    sb t2, 0(t1)

    lla t1, __user_stack_end
    li t2, 0x5a
    sb t2, -1(t1)
.globl user_store_boundary
user_store_boundary:
    sh zero, -1(t1)
    lbu t3, -1(t1)
    bne t2, t3, user_failure

    li a7, SYS_WRITE
    li a0, 2
    lla a1, user_message
    li a2, user_message_size
    ecall
    li t1, -ERR_BAD_FD
    bne a0, t1, user_failure

    li a0, STDOUT_FD
    li a1, 0
    li a2, 1
    ecall
    li t1, -ERR_BAD_ADDRESS
    bne a0, t1, user_failure

    li a0, STDOUT_FD
    lla a1, user_message
    li a2, -1
    ecall
    bne a0, t1, user_failure

    li a0, STDOUT_FD
    lla a1, __user_text_end
    addi a1, a1, -1
    li a2, 2
    ecall
    bne a0, t1, user_failure

    li a7, -1
    ecall
    li t1, -ERR_UNKNOWN_SYSCALL
    bne a0, t1, user_failure

    li a7, SYS_WRITE
    li a0, STDOUT_FD
    lla a1, user_message
    li a2, user_message_size
    ecall
    bne a0, a2, user_failure
    ld t1, 0(sp)
    bne t0, t1, user_failure
    addi sp, sp, 16

    li a0, 7
    j user_exit
user_failure:
    li a0, 1
user_exit:
    li a7, SYS_EXIT
    ecall
1:
    ebreak
    j 1b

.section .bss, "aw", @nobits
.balign 16
    .space 4096
kernel_stack_top:

.section .user.stack, "aw", @nobits
.balign 16
    .space 4096
user_stack_top:
