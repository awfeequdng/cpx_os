#ifndef __INCLUDE_MMU_H__
#define __INCLUDE_MMU_H__

#ifdef __ASSEMBLER__

#define SEG_NULL                                                \
    .word 0, 0;                                                 \
    .byte 0, 0, 0, 0

#define SEG_ASM(type,base,lim)                                  \
    .word (((lim) >> 12) & 0xffff), ((base) & 0xffff);          \
    .byte (((base) >> 16) & 0xff), (0x90 | (type)),             \
        (0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

#else	// not __ASSEMBLER__
#include <types.h>

struct SegDesc {
	unsigned sd_lim_15_0 : 16;	// low bits of segment limit
	unsigned sd_base_15_0 : 16;	// low bits of segment base address
	unsigned sd_base_23_16 : 8;	// middle bits of segment base address
	unsigned sd_type : 4; 		// segment type (see STS_ constants)
	unsigned sd_s : 1;			// 0 = system, 1 = application
	unsigned sd_dpl : 2;		// descriptor privilege level
	unsigned sd_p : 1;			// present
	unsigned sd_lim_19_16 : 4;	// high bits of segment limit
	unsigned sd_avl : 1;		// unused
	unsigned sd_res : 1;		// reserved
	unsigned sd_db : 1;			// 0 = 16-bit segment, 1 = 32-bit segment
	unsigned sd_g : 1;			// granularity: limit scaled by 4Kwhen set
	unsigned sd_base_31_24 : 8;	// high bits of segment base address
};

struct GateDescriptor {
	unsigned gd_off_15_0 : 16;
	unsigned gd_ss : 16;
	unsigned gd_args: 5;
	unsigned gd_rsv1: 3;
	unsigned gd_type : 4;
	unsigned gd_s : 1;
	unsigned gd_dpl : 2;
	unsigned gd_p : 1;
	unsigned gd_off_31_16 : 16;
};


/* task state segment format (as described by the Pentium architecture book) */
struct TaskState {
    uint32_t ts_link;       // old ts selector
    uintptr_t ts_esp0;      // stack pointers and segment selectors
    uint16_t ts_ss0;        // after an increase in privilege level
    uint16_t ts_padding1;
    uintptr_t ts_esp1;
    uint16_t ts_ss1;
    uint16_t ts_padding2;
    uintptr_t ts_esp2;
    uint16_t ts_ss2;
    uint16_t ts_padding3;
    uintptr_t ts_cr3;       // page directory base
    uintptr_t ts_eip;       // saved state from last task switch
    uint32_t ts_eflags;
    uint32_t ts_eax;        // more saved state (registers)
    uint32_t ts_ecx;
    uint32_t ts_edx;
    uint32_t ts_ebx;
    uintptr_t ts_esp;
    uintptr_t ts_ebp;
    uint32_t ts_esi;
    uint32_t ts_edi;
    uint16_t ts_es;         // even more saved state (segment selectors)
    uint16_t ts_padding4;
    uint16_t ts_cs;
    uint16_t ts_padding5;
    uint16_t ts_ss;
    uint16_t ts_padding6;
    uint16_t ts_ds;
    uint16_t ts_padding7;
    uint16_t ts_fs;
    uint16_t ts_padding8;
    uint16_t ts_gs;
    uint16_t ts_padding9;
    uint16_t ts_ldt;
    uint16_t ts_padding10;
    uint16_t ts_t;          // trap on task switch
    uint16_t ts_iomb;       // i/o map base address
} __attribute__((packed));

/* *
 * Set up a normal interrupt/trap gate descriptor
 *   - istrap: 1 for a trap (= exception) gate, 0 for an interrupt gate
 *   - sel: Code segment selector for interrupt/trap handler
 *   - off: Offset in code segment for interrupt/trap handler
 *   - dpl: Descriptor Privilege Level - the privilege level required
 *          for software to invoke this interrupt/trap gate explicitly
 *          using an int instruction.
 * */
#define SETGATE(gate, istrap, ss, off, dpl) {               \
        (gate).gd_off_15_0 = (uint32_t)(off) & 0xffff;      \
        (gate).gd_ss = (ss);                                \
        (gate).gd_args = 0;                                 \
        (gate).gd_rsv1 = 0;                                 \
        (gate).gd_type = (istrap) ? STS_TG32 : STS_IG32;    \
        (gate).gd_s = 0;                                    \
        (gate).gd_dpl = (dpl);                              \
        (gate).gd_p = 1;                                    \
        (gate).gd_off_31_16 = (uint32_t)(off) >> 16;        \
    }

/* Set up a call gate descriptor */
#define SETCALLGATE(gate, ss, off, dpl) {                   \
        (gate).gd_off_15_0 = (uint32_t)(off) & 0xffff;      \
        (gate).gd_ss = (ss);                                \
        (gate).gd_args = 0;                                 \
        (gate).gd_rsv1 = 0;                                 \
        (gate).gd_type = STS_CG32;                          \
        (gate).gd_s = 0;                                    \
        (gate).gd_dpl = (dpl);                              \
        (gate).gd_p = 1;                                    \
        (gate).gd_off_31_16 = (uint32_t)(off) >> 16;        \
    }

#define SEG_NULL                                            \
    (struct SegDesc) {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

#define SEG(type, base, lim, dpl)                           \
    (struct SegDesc) {                                      \
        ((lim) >> 12) & 0xffff, (base) & 0xffff,            \
        ((base) >> 16) & 0xff, type, 1, dpl, 1,             \
        (unsigned)(lim) >> 28, 0, 0, 1, 1,                  \
        (unsigned) (base) >> 24                             \
    }

#define SEGTSS(type, base, lim, dpl)                        \
    (struct SegDesc) {                                      \
        (lim) & 0xffff, (base) & 0xffff,                    \
        ((base) >> 16) & 0xff, type, 0, dpl, 1,             \
        (unsigned) (lim) >> 16, 0, 0, 1, 0,                 \
        (unsigned) (base) >> 24                             \
    }

#endif // __ASSEMBER__

// Eflags register
#define FL_CF           0x00000001  // Carry Flag
#define FL_PF           0x00000004  // Parity Flag
#define FL_AF           0x00000010  // Auxiliary carry Flag
#define FL_ZF           0x00000040  // Zero Flag
#define FL_SF           0x00000080  // Sign Flag
#define FL_TF           0x00000100  // Trap Flag
#define FL_IF           0x00000200  // Interrupt Flag
#define FL_DF           0x00000400  // Direction Flag
#define FL_OF           0x00000800  // Overflow Flag
#define FL_IOPL_MASK    0x00003000  // I/O Privilege Level bitmask
#define FL_IOPL_0       0x00000000  //   IOPL == 0
#define FL_IOPL_1       0x00001000  //   IOPL == 1
#define FL_IOPL_2       0x00002000  //   IOPL == 2
#define FL_IOPL_3       0x00003000  //   IOPL == 3
#define FL_NT           0x00004000  // Nested Task
#define FL_RF           0x00010000  // Resume Flag
#define FL_VM           0x00020000  // Virtual 8086 mode
#define FL_AC           0x00040000  // Alignment Check
#define FL_VIF          0x00080000  // Virtual Interrupt Flag
#define FL_VIP          0x00100000  // Virtual Interrupt Pending
#define FL_ID           0x00200000  // ID flag

/* Application segment type bits */
#define STA_X	0x8	// 可执行
#define STA_E	0x4	// 
#define STA_C	0x4
#define STA_W	0x2	// 可写：不可执行段
#define STA_R	0x2	// 可读：可执行段
#define STA_A	0x1	// 被访问


/* System segment type bits */
#define STS_T16A        0x1         // Available 16-bit TSS
#define STS_LDT         0x2         // Local Descriptor Table
#define STS_T16B        0x3         // Busy 16-bit TSS
#define STS_CG16        0x4         // 16-bit Call Gate
#define STS_TG          0x5         // Task Gate / Coum Transmitions
#define STS_IG16        0x6         // 16-bit Interrupt Gate
#define STS_TG16        0x7         // 16-bit Trap Gate
#define STS_T32A        0x9         // Available 32-bit TSS
#define STS_T32B        0xB         // Busy 32-bit TSS
#define STS_CG32        0xC         // 32-bit Call Gate
#define STS_IG32        0xE         // 32-bit Interrupt Gate
#define STS_TG32        0xF         // 32-bit Trap Gate


// 分页相关的宏设置
// #define PD_ENTRIES      1024    // 页目录中页目录项的数目
// #define PT_ENTRIES      1024    // 页表中页表项的数目
#define PDX_SHIFT		22
#define PTX_SHIFT		12

#define PDE_ENTRIES		1024
#define PTE_ENTRIES		1024

#define PAGE_SIZE		4096
#define PAGE_SHIFT		12

// 一个page table可以映射的内存大小（4M）
#define PT_SIZE			(PAGE_SIZE * PTE_ENTRIES)
#define PT_SHIFT		22

// page directory 的索引
#define PDX(vaddr)	((((uintptr_t)(vaddr)) >> PDX_SHIFT) & 0x3ff)

// page table 索引
#define PTX(vaddr)	((((uintptr_t)(vaddr)) >> PTX_SHIFT) & 0x3ff)

// 物理地址对应的页框号
#define PPN(paddr)	(((uintptr_t)(paddr)) >> PTX_SHIFT)

// 虚拟地址在页内的偏移
#define PAGE_OFF(vaddr)	(((uintptr_t)(vaddr)) & 0xfff)

// 根据页目录项、页表项以及页内偏移地址构造出虚拟地址
#define PAGE_ADDR(d, t, o) ((uintptr_t)((d) << PDX_SHIFT | (t) << PTX_SHIFT | (o)))

#define PTE_ADDR(pte) ((uintptr_t)(pte) & ~0xfff)
#define PDE_ADDR(pde) PTE_ADDR(pde)

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

#define PTE_USER	(PTE_U | PTE_W | PTE_P)

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
