.section .text, "ax", @progbits
.globl _start

.equ UART, 0x10000000
.equ UART_IER, 1
.equ UART_IIR, 2
.equ UART_LCR, 3
.equ UART_MCR, 4
.equ UART_LSR, 5
.equ PLIC_UART_PRIORITY, 0x0c000028
.equ PLIC_MACHINE_ENABLE, 0x0c002000
.equ PLIC_MACHINE_CLAIM, 0x0c200004
.equ UART_IRQ, 10

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

    li s0, UART
    lbu t0, UART_LSR(s0)
    li t1, 0x60
    bne t0, t1, fail

    # Set and read back the divisor latch, then select 8-bit characters.
    li t0, 0x80
    sb t0, UART_LCR(s0)
    li t0, 1
    sb t0, 0(s0)
    sb zero, UART_IER(s0)
    lbu t1, 0(s0)
    bne t0, t1, fail
    lbu t0, UART_IER(s0)
    bnez t0, fail
    li t0, 3
    sb t0, UART_LCR(s0)

    # Loop back two bytes to check that the receive FIFO preserves their order.
    li t0, 7
    sb t0, UART_IIR(s0)
    li t0, 0x10
    sb t0, UART_MCR(s0)
    li t0, 'A'
    li t1, 'B'
    sb t0, 0(s0)
    sb t1, 0(s0)
    lbu t2, 0(s0)
    bne t2, t0, fail
    lbu t2, 0(s0)
    bne t2, t1, fail
    sb zero, UART_MCR(s0)
    sb zero, UART_IIR(s0)

    # Check that reading the interrupt ID acknowledges a transmitter interrupt.
    li t0, 2
    sb t0, UART_IER(s0)
    lbu t1, UART_IIR(s0)
    bne t0, t1, fail
    lbu t1, UART_IIR(s0)
    li t0, 1
    bne t0, t1, fail
    sb zero, UART_IER(s0)

    li t0, 'P'
    sb t0, 0(s0)
poll_input:
    lbu t0, UART_LSR(s0)
    andi t0, t0, 1
    beqz t0, poll_input
    lbu t0, 0(s0)
    bnez t0, fail
    lbu t0, UART_LSR(s0)
    andi t0, t0, 1
    bnez t0, fail

    # One received byte is below the FIFO trigger and must interrupt on timeout.
    li t0, 0x47
    sb t0, UART_IIR(s0)
    li t0, 1
    sb t0, UART_IER(s0)
    li t1, PLIC_UART_PRIORITY
    sw t0, 0(t1)
    li t1, PLIC_MACHINE_ENABLE
    li t0, (1 << UART_IRQ)
    sw t0, 0(t1)
    # Claim and complete the transmitter interrupt latched by the PLIC.
    li t1, PLIC_MACHINE_CLAIM
    lw t0, 0(t1)
    li t2, UART_IRQ
    bne t0, t2, fail
    sw t0, 0(t1)

    li s1, 0
    lla t0, uart_interrupt
    csrw mtvec, t0
    li t0, (1 << 11)
    csrs mie, t0
    csrsi mstatus, 8
    li t0, 'Q'
    sb t0, 0(s0)
wait_interrupt:
    beqz s1, wait_interrupt
    li t0, 'R'
    j report

.balign 4
uart_interrupt:
    csrr t0, mcause
    li t1, 0x800000000000000b
    bne t0, t1, fail
    li t2, PLIC_MACHINE_CLAIM
    lw t0, 0(t2)
    li t1, UART_IRQ
    bne t0, t1, fail
    lbu t1, UART_IIR(s0)
    li t3, 0xcc
    bne t1, t3, fail
    lbu t1, 0(s0)
    li t3, 'X'
    bne t1, t3, fail
    sw t0, 0(t2)
    lw t0, 0(t2)
    bnez t0, fail
    lbu t0, UART_IIR(s0)
    li t1, 0xc1
    bne t0, t1, fail
    li s1, 1
    mret

fail:
    li t0, 'F'
report:
    li t1, UART
    sb t0, 0(t1)
1:
    j 1b
