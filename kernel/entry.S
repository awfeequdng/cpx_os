#include <include/memlayout.h>
#include <include/mmu.h>

#define RELOC(x) ((x) - KERNEL_BASE)

#define MULTI_BOOT_HEADER_MAGIC (0x1badb002)
#define MULTI_BOOT_HEADER_FLAGS (0)
#define CHECKSUM (-(MULTI_BOOT_HEADER_MAGIC + MULTI_BOOT_HEADER_FLAGS))

	.section .text
	.align 4
	.long MULTI_BOOT_HEADER_MAGIC
	.long MULTI_BOOT_HEADER_FLAGS
	.long CHECKSUM

	.global _start
_start = RELOC(entry)
	
	.global entry
entry:
	# movw $0x1234, 0x472 # ????: todo
	
	# 设置页目录地址
	movl $(RELOC(entry_page_dir)), %eax
	movl %eax, %cr3

	# 开启分页
	movl %cr0, %eax
	orl $(CR0_PG | CR0_WP), %eax
	movl %eax, %cr0

	# 此处代码已经开启了分页，但此时的EIP还是low addr,
	# 这处代码能执行的原因是由于建立了[0, 4M) -> [0, 4M)的恒等映射
	mov $relocated, %eax
	jmp *%eax
relocated:
	# ebp为空表示这是栈底	
	movl $0, %ebp
	
	movl $(boot_stack_top), %esp
	# call跳转会将下一条指令的地址压栈，因此必须设置好堆栈，
	# 并且start_kernel已经是c函数，也需要设置好堆栈
	call start_kernel
	
spin:
	jmp spin

	.section .data
	# .align 是按字节对齐，如果要按2的指数方对齐，需要使用 .p2align
	.p2align PG_SHIFT
	.global boot_stack
boot_stack:
	.space K_STACK_SIZE
	.global boot_stack_top
boot_stack_top:	
