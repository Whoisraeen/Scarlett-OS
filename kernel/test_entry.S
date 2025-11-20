.section .text
.global _start
.extern test_main

_start:
    # Disable interrupts
    cli

    # Set up stack
    lea stack_top(%rip), %rsp

    # Call test_main (C function)
    call test_main

    # Halt if it returns
    cli
1:
    hlt
    jmp 1b

.section .bss
.align 16
stack_bottom:
    .skip 16384  # 16KB stack
stack_top:
