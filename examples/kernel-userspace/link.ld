OUTPUT_ARCH(riscv)
ENTRY(_start)

PHDRS
{
  kernel_text PT_LOAD FLAGS(5);
  kernel_data PT_LOAD FLAGS(6);
  user_text PT_LOAD FLAGS(5);
  user_data PT_LOAD FLAGS(6);
}

SECTIONS
{
  . = 0x8000;
  .text : { *(.text.init) *(.text .text.*) } :kernel_text
  .rodata : { *(.rodata .rodata.*) } :kernel_text
  .data : { *(.data .data.*) } :kernel_data
  .bss (NOLOAD) : { *(.bss .bss.*) } :kernel_data

  .user.text ALIGN(0x1000) : {
    __user_text_start = .;
    *(.user.text)
    *(.user.rodata)
    __user_text_end = .;
  } :user_text
  .user.stack ALIGN(0x1000) (NOLOAD) : {
    __user_stack_start = .;
    *(.user.stack)
    __user_stack_end = .;
  } :user_data

  ASSERT(. <= 0x108000, "Kernel and userspace exceed guest RAM")
}
