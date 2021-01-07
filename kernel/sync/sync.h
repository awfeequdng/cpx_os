#ifndef __KERNEL_SYNC_SYNC_H__
#define __KERNEL_SYNC_SYNC_H__

#include <x86.h>
#include <mmu.h>
#include <intr.h>
#include <assert.h>
#include <atomic.h>
// #include <schedule.h>

static inline bool __intr_save(void) {
    if (read_eflags() & FL_IF) {
        intr_disable();
        return 1;
    }
    return 0;
}

static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}

#define local_intr_save(x)  do { x = __intr_save(); } while(0)
#define local_intr_restore(x) __intr_restore(x)

typedef volatile int lock_t;

static inline void lock_init(lock_t *lock) {
    *lock = 0;
}

static inline bool try_lock(lock_t *lock) {
    return !test_and_set_bit(0, lock);
}

void schedule(void);

static inline void lock(lock_t *lock) {
    // may deadlock
    while(!try_lock(lock)) {
        schedule();
    }
}

static inline void unlock(lock_t *lock) {
    if (!test_and_clear_bit(0, lock)) {
        panic("Unlock failed.\n");
    }
}

#endif //__KERNEL_SYNC_SYNC_H__