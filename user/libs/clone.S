#include <unistd.h>

    .text
    .global __clone
__clone:
    pushl %ebp
    movl  %esp, %ebp
    // 系统调用会传递四个参数，分别放在edx、ecx、ebx以及edi中,
    // 因此先将这四个寄存器内容保存
    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %edi

    movl 0x8(%ebp), %edx    # clone_flags
    movl 0xc(%ebp), %ecx    # stack
    movl 0x10(%ebp), %ebx   # fn
    movl 0x14(%ebp), %edi   # arg

    movl $SYS_clone, %eax
    int $T_SYSCALL

    cmpl $0x0, %eax
    # 如果返回为0，说明是子线程，跳转到1标号处执行
    je 1f

    # parent
    popl %edi
    popl %ebx
    popl %ecx
    popl %edx

    leave           # restore ebp
    ret

    # child
1:  
    # 调用子线程
    pushl %edi      # edi: arg
    call *%ebx      # ebx: fn
    movl %eax, %edx # （正常return时）保存退出码到系统调用的第一个参数edx
    movl $SYS_exit_thread, %eax
    int $T_SYSCALL

spin:
    jmp spin