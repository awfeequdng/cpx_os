#ifndef __KERNEL_DRIVER_INTR_H__
#define __KERNEL_DRIVER_INTR_H__
#include <x86.h>

inline void intr_enable(void) __attribute__((always_inline));
inline void intr_disable(void) __attribute__((always_inline));

inline void intr_enable(void) {
    sti();
}

inline void intr_disable(void) {
    cli();
}

#endif //__KERNEL_DRIVER_INTR_H__