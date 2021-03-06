#include <trap.h>
#include <stdio.h>
#include <mmu.h>
#include <memlayout.h>
#include <x86.h>
#include <console.h>
#include <assert.h>
#include <clock.h>
#include <vmm.h>
#include <process.h>
#include <schedule.h>
#include <unistd.h>
#include <syscall.h>
#include <error.h>
#include <schedule.h>

#define TICK		30


static struct GateDescriptor idt[256] = {{0}};

static struct PseudoDescriptor idt_pd = {
	sizeof(idt) - 1, (uintptr_t)idt
};

void idt_init() {
	extern uintptr_t __vectors[];
	int i;
	for (i = 0; i < ARRAY_SIZE(idt); i++) {
		// todo: 为什么要改，不改行不行
		// SETGATE(idt[i], 0, GD_KTEXT, __vectors[i], DPL_KERNEL);
		SETGATE(idt[i], 1, GD_KTEXT, __vectors[i], DPL_KERNEL);
	}
	// 0x80陷阱门的权限为3，当发生中断时，会发生权限提升
	SETGATE(idt[T_SYSCALL], 1, GD_KTEXT, __vectors[T_SYSCALL], DPL_USER);
	lidt(&idt_pd);
}


static const char *trap_name(int trapno) {
    static const char *const trap_names[] = {
        "Divide error",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "BOUND Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection",
        "Page Fault",
        "(unknown trap)",
        "x87 FPU Floating-Point Error",
        "Alignment Check",
        "Machine-Check",
        "SIMD Floating-Point Exception"
    };

    if (trapno < ARRAY_SIZE(trap_names)) {
        return trap_names[trapno];
    }
	if (trapno == T_SYSCALL) {
		return "System call";
	}

    if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16) {
        return "Hardware Interrupt";
    }
    return "(unknown trap)";
}

void print_regs(struct PushRegs *regs) {
    printk("  edi	0x%08x\n", regs->reg_edi);
    printk("  esi  	0x%08x\n", regs->reg_esi);
    printk("  ebp  	0x%08x\n", regs->reg_ebp);
    printk("  oesp 	0x%08x\n", regs->reg_oesp);
    printk("  ebx  	0x%08x\n", regs->reg_ebx);
    printk("  edx  	0x%08x\n", regs->reg_edx);
    printk("  ecx  	0x%08x\n", regs->reg_ecx);
    printk("  eax  	0x%08x\n", regs->reg_eax);
}

bool trap_in_kernel(struct TrapFrame *tf) {
	return (tf->tf_cs == (uint16_t)KERNEL_CS);
}

static const char *IA32flags[] = {
	"CF", NULL, "PF", NULL, "AF", NULL, "ZF", "SF",
	"TF", "IF", "DF", "OF", NULL, NULL, "NT", NULL,
	"RF", "VM", "AC", "VIF", "VIP", "ID", NULL, NULL,
};

void print_trap_frame(struct TrapFrame *tf) {
	printk("trap frame at *p\n", tf);
	print_regs(&tf->tf_regs);
	printk("  es	0x----%4x\n", tf->tf_es);
	printk("  ds	0x----%4x\n", tf->tf_ds);
	printk("  trap	0x%08x %s\n", tf->tf_trap_no, trap_name(tf->tf_trap_no));
	printk("  err	0x%08x\n", tf->tf_err);
	printk("  eip	0x%08x\n", tf->tf_eip);
	printk("  cs	0x----%04x\n", tf->tf_cs);
	printk("  flag	0x%08x ", tf->tf_eflags);

	int i, j;
	for (i = 0, j = 1; i < ARRAY_SIZE(IA32flags); i++, j<<1) {
		if ((tf->tf_eflags & j) && IA32flags[i] != NULL) {
			printk("%s,", IA32flags[i]);
		}
	}
	printk("IOPL=%d\n", (tf->tf_eflags & FL_IOPL_MASK) >> 12);
	if (!trap_in_kernel(tf)) {
		printk("  esp	0x%08x\n", tf->tf_esp);
		printk("  ss	0x%----04x\n", tf->tf_ss);
	}
}

static void print_page_fault(struct TrapFrame *tf) {
	/* error_code:
     * bit 0 == 0 means no page found, 1 means protection fault
     * bit 1 == 0 means read, 1 means write
     * bit 2 == 0 means kernel, 1 means user
     * */
	printk("page fault at 0x%08x: %c/%c [%s].\n", rcr2(),
		((tf->tf_err & 4) ? 'U' : 'K'),
		((tf->tf_err & 2) ? 'W' : 'R'),
		((tf->tf_err & 1) ? "protection fault" : "no page found"));
}

static int page_fault_handler(struct TrapFrame *tf) {
	extern MmStruct *check_mm_struct;
	MmStruct *mm = NULL;
	if (check_mm_struct != NULL) {
		assert(current == idle_process);
		mm = check_mm_struct;
	} else {
		if (current == NULL) {
			print_trap_frame(tf);
			print_page_fault(tf);
			panic("unhandled page fault.\n");
		}
		mm = current->mm;
	}
	return do_page_fault(mm, tf->tf_err, rcr2());
}

static void trap_dispatch(struct TrapFrame *tf) {
	char c;
	int ret;

	size_t tick;
	switch (tf->tf_trap_no) {
		case T_PG_FAULT:
			if ((ret = page_fault_handler(tf)) != 0) {
				print_trap_frame(tf);
				if (current == NULL) {
					panic("handle page fault failed. %e\n", ret);
				} else {
					if (trap_in_kernel(tf)) {
						panic("handle page fault failed in kernel mode. %e\n", ret);
					}
					// 在不能处理page_fault时，进程将要被杀死
					// 可能在申请页时内存不够，导致do_page_fault返回失败，
					// 此时内存不够直接杀死进程
					printk("killed by kernel.\n");
					do_exit(-E_KILLED);
				}
			}
			break;
		case T_SYSCALL:
			syscall();
			break;
		case IRQ_OFFSET + IRQ_TIMER:
			// printk("fall in irq timer\n");
			tick = get_ticks();
			tick++;
			set_ticks(tick);
			run_timer_list();
			if (tick % TICK == 0) {
				// print_hz();
				// printk("%d ticks\n", tick);
				assert(current != NULL);
				// 将进程调度出去
				current->need_resched = 1;
			}
			break;
		case IRQ_OFFSET + IRQ_COM1:
			// c = console_getc();
			// printk("serial [%03d] %c\n", c, c);
			// 有串口产生，提示有数据输入，接收这个数据到缓冲区
			serial_intr();
			break;
		case IRQ_OFFSET + IRQ_KBD:
			c = console_getc();
			printk("kbd [%03d] %c\n", c, c);
			break;
		case IRQ_OFFSET + IRQ_IDE1:
    	case IRQ_OFFSET + IRQ_IDE2:
        	/* do nothing */
        	break;
		default:
			print_trap_frame(tf);
			if (current != NULL) {
				printk("unhandled trap.\n");
				do_exit(-E_KILLED);
			}
			panic("unexpected trap in kernel.\n");
	}
}

void trap(struct TrapFrame *tf)
{
	if (current == NULL) {
		// used for previous projects
		trap_dispatch(tf);
	} else {
		// keep a trapframe chain in stack
		struct TrapFrame *otf = current->tf;
		current->tf = tf;

		bool in_kernel = trap_in_kernel(tf);

		trap_dispatch(tf);

		current->tf = otf;
		if (!in_kernel) {
			// 用户态发生了中断事件
			may_killed();
			if (current->need_resched) {
				// 中断结束前进行一次调度
				schedule();
			}
		}
	}
}