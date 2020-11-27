#ifndef __INCLUDE_MMU_H__
#define __INCLUDE_MMU_H__

#ifdef __ASSEMBLER__
#define SEG_NULL	\
	.word 0, 0;	\
	.byte 0, 0, 0, 0

#define SEG(type, base, limit)	\
	.word (((limit) >> 12) & 0xffff), ((base) & 0xffff);	\
	.byte (((base) >> 16) & 0xff), ((0x90 | type)), \
		(0xc0 | (((limit) >> 28) & 0xf)), (((base) >> 24) & 0xff)

#else	// not __ASSEMBLER__

#endif // __ASSEMBER__

#define STA_X	0x8	// 可执行
#define STA_E	0x4	// 
#define STA_C	0x4
#define STA_W	0x2	// 可写：不可执行段
#define STA_R	0x2	// 可读：可执行段
#define STA_A	0x1	// 被访问




// 分页相关的宏设置
#define PD_ENTRIES      1024    // 页目录中页目录项的数目
#define PT_ENTRIES      1024    // 页表中页表项的数目
#define PG_SIZE         4096    // 分页的大小
#define PG_SHIFT        12      // 页地址偏移
#define PDX_SHIFT	22

// 页表/页目录项的标志位
#define PTE_P		0x001	// Present
#define PTE_W		0x002	// Writeable
#define PTE_U		0x004	// User
#define PTE_PWT		0x008	// Write-Through
#define PTE_PCD		0x010	// Cache-Disable
#define PTE_A		0x020	// Accessed
#define PTE_D		0x040	// Dirty
#define PTE_PS		0x080	// Page Size
#define PTE_G		0x100	// Global


// Control Register flags
#define CR0_PE		0x00000001	// Protection Enable
#define CR0_MP		0x00000002	// Monitor coProcessor
#define CR0_EM		0x00000004	// Emulation
#define CR0_TS		0x00000008	// Task Switched
#define CR0_ET		0x00000010	// Extension Type
#define CR0_NE		0x00000020	// Numeric Errror
#define CR0_WP		0x00010000	// Write Protect
#define CR0_AM		0x00040000	// Alignment Mask
#define CR0_NW		0x20000000	// Not Writethrough
#define CR0_CD		0x40000000	// Cache Disable
#define CR0_PG		0x80000000	// Paging


#endif
