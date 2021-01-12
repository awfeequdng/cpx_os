#ifndef __USER_LIBS_LOCK_H__
#define __USER_LIBS_LOCK_H__

#include <types.h>
#include <atomic.h>
#include <ulib.h>

#define INIT_LOCK   {0}

typedef volatile int lock_t;

static inline void lock_init(lock_t *lock) {
    *lock = 0;
}

static inline bool try_lock(lock_t *lock) {
    return !test_and_set_bit(0, lock);
}

static inline void lock(lock_t *lock) {
    if (!try_lock(lock)) {
        int step = 0;
        do {
            yield();
            if (++step == 100) {
                step = 0;
                sleep(10);
            }
        } while (!try_lock(lock));
    }
}

static inline void unlock(lock_t *lock) {
    test_and_clear_bit(0, lock);
}

#endif //__USER_LIBS_LOCK_H__
