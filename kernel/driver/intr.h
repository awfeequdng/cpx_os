#ifndef __KERNEL_DRIVER_INTR_H__
#define __KERNEL_DRIVER_INTR_H__
#include <x86.h>

static inline void intr_enable(void) __attribute__((always_inline));
static inline void intr_disable(void) __attribute__((always_inline));

static inline void intr_enable(void) {
    sti();
}

static inline void intr_disable(void) {
    cli();
}

#endif //__KERNEL_DRIVER_INTR_H__