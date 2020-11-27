#ifndef __INCLUDE_MEMLAYOUT_H__
#define __INCLUDE_MEMLAYOUT_H__

#include <include/mmu.h>

#define KERNEL_BASE	0xc0000000

#define K_STACK_TOP  KERNEL_BASE
#define K_STACK_SIZE (8 * PG_SIZE)

#endif
