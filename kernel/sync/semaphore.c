#include <semaphore.h>
#include <assert.h>
#include <process.h>
#include <sync.h>
#include <slab.h>
#include <wait.h>

void sem_init(Semaphore *sem, int val) {
    sem->value = val;
    wait_queue_init(&(sem->wait_queue));
}

static void __attribute__((noinline)) __up(Semaphore *sem, uint32_t wait_state) {
    bool flag;
    local_intr_save(flag);
    {
        Wait *wait = NULL;
        if ((wait = wait_queue_first(&(sem->wait_queue))) == NULL) {
            sem->value++;
        } else {
            assert(wait->process->wait_state == wait_state);
            wakeup_wait(&(sem->wait_queue), wait, wait_state, true);
        }
    }
    local_intr_restore(flag);
}

static uint32_t __attribute__((noinline)) __down(Semaphore *sem, uint32_t wait_state) {
    bool flag;
    local_intr_save(flag);
    
    if (sem->value > 0) {
        sem->value--;
        local_intr_restore(flag);
        return 0;
    }
    Wait __wait, *wait = &__wait;
    wait_current_set(&(sem->wait_queue), wait, wait_state);
    local_intr_restore(flag);

    schedule();

    local_intr_save(flag);
    wait_current_del(&(sem->wait_queue), wait);
    local_intr_restore(flag);

    if (wait->wakeup_flags != wait_state) {
        return wait->wakeup_flags;
    }
    return 0;
}

void up(Semaphore *sem) {
    __up(sem, WT_KSEM);
}

void down(Semaphore *sem) {
    uint32_t flags = __down(sem, WT_KSEM);
    assert(flags == 0);
}

bool try_down(Semaphore *sem) {
    bool flag;
    bool ret = false;
    local_intr_save(flag);
    {
        if (sem->value > 0) {
            sem->value--;
            ret = true;
        }
    }
    local_intr_restore(flag);
    return ret;
}