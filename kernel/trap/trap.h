#ifndef __KERNEL_TRAP_TRAP_H__
#define __KERNEL_TRAP_TRAP_H__

#include <types.h>

#define T_DIVIDE		0
#define T_DEBUG			1
#define T_NMI			2
#define T_BREAKPOINT	3
#define T_OVERFLOW		4
#define	T_BOUND			5
#define T_ILLEGAL_OP	6
#define T_DEVICE		7
#define T_DOUBLE_FAULT	8

// reserved
#define T_COPROC		9

#define T_TSS			10
#define T_SEG_NP		11
#define T_STACK			12
#define T_GP_FAULT		13
#define T_PG_FAULT		14

// reserved
#define T_RES			15
#define T_FP_ERR		16
#define T_ALIGN			17
#define T_MAC_CHK		18
#define T_SIMD_ERR		19

#define T_SYSCALL		0x80

#define IRQ_OFFSET		32

#define IRQ_TIMER 		0
#define IRQ_KBD			1
#define IRQ_COM1		4
#define IRQ_IDE1		14
#define IRQ_IDE2		15
#define IRQ_ERROR		19

#define T_SWITCH_TO_USER	120
#define T_SWITCH_TO_KERNEL	121

// registers as pushed by pushal
struct PushRegs {
	uint32_t reg_edi;	
	uint32_t reg_esi;	
	uint32_t reg_ebp;	
	uint32_t reg_oesp; // useless	
	uint32_t reg_ebx;	
	uint32_t reg_edx;	
	uint32_t reg_ecx;	
	uint32_t reg_eax;	
};

struct TrapFrame {
	struct PushRegs tf_regs;
	uint16_t tf_gs;
	uint16_t tf_pad0;
	uint16_t tf_fs;
	uint16_t tf_pad1;
	uint16_t tf_es;
	uint16_t tf_pad2;
	uint16_t tf_ds;
	uint16_t tf_pad3;
	uint32_t tf_trap_no;
	// below here defined by x86 hardware
	uint32_t tf_err;
	uintptr_t tf_eip;
	uint16_t tf_cs;
	uint16_t tf_pad4;
	uint32_t tf_eflags;
	// below here only when crossing rings, such as from user to kernel
	uintptr_t tf_esp;
	uint16_t tf_ss;
	uint16_t pad5;
} __attribute__((packed));


void idt_init(void);
void print_trap_frame(struct TrapFrame *tf);
void print_regs(struct PushRegs *regs);
bool trap_in_kernel(struct TrapFrame *tf);


#endif /*__KERNEL_TRAP_TRAP_H__*/
