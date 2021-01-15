#ifndef __KERNEL_SYNC_SEMAPHORE_H__
#define __KERNEL_SYNC_SEMAPHORE_H__

#include <types.h>
#include <atomic.h>
#include <wait.h>

typedef struct {
    int value;
    WaitQueue wait_queue;
} Semaphore;

void sem_init(Semaphore *sem, int val);
void up(Semaphore *sem);
void down(Semaphore *sem);
bool try_down(Semaphore *sem);

#endif // __KERNEL_SYNC_SEMAPHORE_H__