#ifndef __INCLUDE_MEMLAYOUT_H__
#define __INCLUDE_MEMLAYOUT_H__

#include <mmu.h>

#define KERNEL_BASE	0xc0000000

#define K_STACK_TOP  KERNEL_BASE
#define K_STACK_SIZE (8 * PG_SIZE)

// 内核代码段
#define SEG_KTEXT	1
// 内核数据段
#define SEG_KDATA	2

// 用户态代码段
#define	SEG_UTEXT	3
// 用户态数据段
#define SEG_UDATA	4


#define SEG_TSS		5

#define GD_KTEXT 	((SEG_KTEXT) << 3)	// kernel text
#define GD_KDATA	((SEG_KDATA) << 3)  // kernel data
#define GD_UTEXT	((SEG_UTEXT) << 3)  // user text
#define GD_UDATA	((SEG_UDATA) << 3)	// user data
#define GD_TSS		((SEG_TSS) << 3)	// task segment selector

#define DPL_KERNEL	0
#define DPL_USER	3

#define KERNEL_CS	((GD_KTEXT) | DPL_KERNEL)
#define KERKEL_DS	((GD_KDATA) | DPL_KERNEL)
#define USER_CS		((GD_UTEXT) | DPL_USER)
#define USER_DS		((GD_UDATA) | DPL_USER)

#endif
