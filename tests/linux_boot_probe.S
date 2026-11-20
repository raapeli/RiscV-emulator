.section .text, "ax", @progbits
.globl _start
_start:
    auipc t0, 0
    li t1, 0x80000000
    bne t0, t1, fail
    bnez a0, fail
    csrr t0, mhartid
    bnez t0, fail
    li t1, 0x87e00000
    bne a1, t1, fail

    lbu t0, 0(a1)
    li t1, 0xd0
    bne t0, t1, fail
    li t0, 0x80200000
    lbu t0, 0(t0)
    li t1, 'K'
    bne t0, t1, fail
    li t0, 0x84000000
    lbu t0, 0(t0)
    li t1, 'I'
    bne t0, t1, fail

    li t0, 0x02004000
    ld t0, 0(t0)
    li t1, -1
    bne t0, t1, fail
    li t0, 0x0c000000
    lw t0, 0(t0)
    bnez t0, fail

    li t0, 'P'
    j report
fail:
    li t0, 'F'
report:
    li t1, 0x10000000
    sb t0, 0(t1)
1:
    j 1b
