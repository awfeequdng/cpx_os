#include <memlayout.h>

	.text
	.global __all_traps
__all_traps:
	pushl %ds
	pushl %es
	pushl %fs
	pushl %gs
	pushal

	movl $GD_KDATA, %eax
	movw %ax, %ds
	movw %ax, %es

	pushl %esp

	# call trap(tf), where tf=%esp
	call trap

	# pop the pushed stack pointer
	popl %esp

	.global __trap_ret
__trap_ret:
	popal

	popl %gs
	popl %fs
	popl %es
	popl %ds

	# get rid of the trap number and error code
	addl $0x8, %esp
	iret


	.global forkrets
forkrets:
	# 让esp指向tf的起始地址
	movl 4(%esp), %esp
	jmp __trap_ret
