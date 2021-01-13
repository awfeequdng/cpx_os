#include <thread.h>
#include <unistd.h>
#include <error.h>
#include <types.h>
#include <ulib.h>

int thread(int (*fn)(void *), void *arg, Thread *tidp) {
    if (fn == NULL || tidp == NULL) {
        return -E_INVAL;
    }

    int ret;
    uintptr_t stack = 0;
    if ((ret = mmap(&stack, THREAD_STACK_SIZE, MMAP_WRITE | MMAP_STACK)) != 0) {
        return ret;
    }
    assert(stack != 0);

    if ((ret = clone(CLONE_VM | CLONE_THREAD, stack + THREAD_STACK_SIZE, fn, arg)) < 0) {
        munmap(stack, THREAD_STACK_SIZE);
        return ret;
    }

    tidp->pid = ret;
    tidp->stack = (void *)stack;
    return 0;
}

int thread_wait(Thread *tidp, int *exit_code) {
    int ret = -E_INVAL;
    if (tidp != NULL) {
        if ((ret = waitpid(tidp->pid, exit_code)) == 0) {
            munmap((uintptr_t)(tidp->stack), THREAD_STACK_SIZE);
        }
    }
    return ret;
}

int thread_kill(Thread *tidp) {
    if (tidp != NULL) {
        return kill(tidp->pid);
    }
    return -E_INVAL;
}