
OUTPUT_ARCH(i386)

/* 在entry.S中，由于entry的的值是链接地址，而不是实际加载的物理地址，因此我们不用entry作为程序入口地址 */
/* ENTRY(entry) */

/* 在entry.S中，_start是已经将entry链接地址转换成的物理地址，因此将_start作为程序的入口地址，这样设置以后ELF header中的entry字段就是该_start值 */
ENTRY(_start)

SECTIONS
{
	/* 内核的链接地址在0xc0100000 */
	. = 0xc0100000;

	PROVIDE(text = .);
	/* AT(...) 设置段的加载地址（也就是实际的物理地址），*/
	/* 这个地址将告诉bootloader，然后将内核加载到这个物理内存处 */
	.text : AT(0x100000) {
		*(.text .text.*)
	}
	PROVIDE(etext = .);

	.rodata : {
		*(.rodata .rodata.*)
	}

	/* 按4k对齐 */
	. = ALIGN(4K);
	
	.data : {
		*(.data)
	}
	PROVIDE(edata = .);

	.bss : {
		*(.bss)
	}
	PROVIDE(end = .);
}
