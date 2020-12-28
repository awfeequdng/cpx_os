#ifndef __KERNEL_SYNC_SYNC_H__
#define __KERNEL_SYNC_SYNC_H__

#include <x86.h>
#include <mmu.h>
#include <intr.h>

static inline bool __intr_save(void) {
    if (read_eflags() & FL_IF) {
        intr_disable();
        return 1;
    }
    return 0;
}

static inline bool __intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}

#define local_intr_save(x)  do { x = __intr_save(); } while(0)
#define local_intr_restore(x) __intr_restore(x)

#endif //__KERNEL_SYNC_SYNC_H__