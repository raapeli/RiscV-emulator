.section .text.init, "ax", @progbits
.globl _start
_start:
    lla sp, stack_top
    call shell
1:
    j 1b

.section .bss, "aw", @nobits
.balign 16
    .space 4096
stack_top:
