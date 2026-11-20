.section .text.init, "ax", @progbits
.globl _start
_start:
    lla sp, kernel_stack_top
    call kernel_start

    lla t0, trap_entry
    csrw mtvec, t0
    csrw mscratch, sp
    lla t0, user_start
    csrw mepc, t0
    li t0, (3 << 11)  # Clear mstatus.MPP to select U-mode.
    csrc mstatus, t0
    lla sp, user_stack_top
    mret

.balign 4
trap_entry:
    csrrw sp, mscratch, sp
    csrr a0, mcause
    csrr a1, mepc
    tail kernel_trap

.section .user.text, "ax", @progbits
user_start:
    addi sp, sp, -16
    li t0, 42
    sd t0, 0(sp)
    ld t1, 0(sp)
    bne t0, t1, 1f
    addi sp, sp, 16
.globl user_ecall
user_ecall:
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
