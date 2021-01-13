#ifndef __USER_LIBS_THREAD_H__
#define __USER_LIBS_THREAD_H__

typedef struct thread_t {
    int pid;
    void *stack;
} Thread;

#define THREAD_STACK_SIZE   (4096 * 10)

int thread(int (*fn)(void *), void *arg, Thread *tidp);
int thread_wait(Thread *tidp, int *exit_code);
int thread_kill(Thread *tidp);

#endif // __USER_LIBS_THREAD_H__