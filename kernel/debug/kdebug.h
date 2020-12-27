#ifndef __KERNEL_DEBUG_KDEBUG_H__
#define __KERNEL_DEBUG_KDEBUG_H__

#include <types.h>

void print_kerninfo(void);
void print_stackframe(void);
void print_debuginfo(uintptr_t eip);

#endif //__KERNEL_DEBUG_KDEBUG_H__
