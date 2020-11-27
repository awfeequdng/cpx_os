#include<include/mmu.h>

.set CODE_SEG, 0X8
.set DATA_SEG, 0x10

.global _start
_start:
	.code16
	cli	# 关中断
	cld	# 清除方向位，字符串操作时向高地址增长

	xorw %ax, %ax
	movw %ax, %ds  # data seg
	movw %ax, %es	# extra seg
	movw %ax, %ss	# stack seg
	movw %ax, %gs
	movw %ax, %fs

seta20.1:
	inb 	$0x64, %al
	testb 	$0x2, %al
	jnz	seta20.1	# 如果io端口0x64的值的bit[1]为1表示ready，即可以操作该端口
	
	movb $0xd1, %al
	outb %al, $0x64		# 向0x64端口写入0xd1表示将A20地址线拉低功能去掉

seta20.2:
	inb 	$0x64, %al
	testb	$0x02, %al
	jnz	seta20.2	# 等待ready

	lgdt    gdtdesc		# 加载全局描述符表信息
	
	movl 	%cr0, %eax	# 将cr0的bit0 置1，表示开启保护模式
	orl	$0x01, %eax
	movl	%eax,  %cr0	

	ljmp $CODE_SEG, $prot_seg # 跳转到保护模式处


	.code32 # 从此处开始进入保护模式，即32位保护模式下
prot_seg:
	movw $DATA_SEG, %ax # 设置数据段选择子
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	movw %ax, %gs
	movw %ax, %fs
		
	# 为c程序设置好堆栈，堆栈的起始地址为_start，从高向低增长
	movl $_start, %esp
	call boot_main	

spin:
	jmp spin


	.align 2	# 四字节对齐
gdt:
	SEG_NULL
	SEG(STA_X | STA_R, 0x0, 0xffffffff)	# code seg
	SEG(STA_W, 0x0, 0xffffffff)	# data seg
gdtdesc:
	.word (gdtdesc - gdt + 1)	# gdt全局描述符表的大小，这个值需要加上1
	.long gdt			# gdt全局描述符的地址
	
