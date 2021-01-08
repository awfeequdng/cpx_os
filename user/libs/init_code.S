    .text
    .global _start
_start:
    movl $0x0, %ebp

    subl $0x20, %esp

    call umain
1:  jmp 1b