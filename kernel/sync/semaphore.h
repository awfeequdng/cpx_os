#ifndef __KERNEL_SYNC_SEMAPHORE_H__
#define __KERNEL_SYNC_SEMAPHORE_H__

#include <types.h>
#include <atomic.h>
#include <wait.h>

typedef struct {
    int value;
    atomic_t ref;
    WaitQueue wait_queue;
} Semaphore;

// The SemaphoreUndo is used to permit semaphore manipulations that can be undone. If a process
// crashes after modifying a semaphore, the information held in the list is used to return the semaphore
// to its state prior to modification. The mechanism is useful when the crashed process has made changes
// after which processes waiting on the semaphore can no longer be woken. By undoing these actions (using
// the information in the sem_undo list), the semaphore can be returned to a consistent state, thus preventing
// deadlocks.
typedef struct semaphore_undo {
    Semaphore *sem;
    ListEntry sem_undo_link;
} SemaphoreUndo;

#define le2semundo(le, member)      \
    container_of((le), SemaphoreUndo, member)

typedef struct semaphore_queue {
    // 这个信号量是用来保护向sem_undo_list队列中添加数据时的操作的
    Semaphore sem;
    atomic_t ref;
    ListEntry sem_undo_list;
} SemaphoreQueue;

void sem_init(Semaphore *sem, int val);
void up(Semaphore *sem);
void down(Semaphore *sem);
bool try_down(Semaphore *sem);

SemaphoreUndo *sem_undo_create(Semaphore *sem, int val);
void sem_undo_destory(SemaphoreUndo *sem_undo);

SemaphoreQueue *sem_queue_create(void);
void sem_queue_destory(SemaphoreQueue *sem_queue);
int dup_sem_queue(SemaphoreQueue *to, SemaphoreQueue *from);
void exit_sem_queue(SemaphoreQueue *sem_queue);

int ipc_sem_init(int val);
int ipc_sem_post(sem_t sem_id);
int ipc_sem_wait(sem_t sem_id);
int ipc_sem_get_value(sem_t sem_id, int *value_store);

static inline int sem_ref(Semaphore *sem) {
    return atomic_read(&(sem->ref));
}

static inline void set_sem_ref(Semaphore *sem, int val) {
    atomic_set(&(sem->ref), val);
}

static inline int sem_ref_inc(Semaphore *sem) {
    return atomic_add_return(&(sem->ref), 1);
}

static inline int sem_ref_dec(Semaphore *sem) {
    return atomic_sub_return(&(sem->ref), 1);
}

static inline int sem_queue_ref(SemaphoreQueue *sq) {
    return atomic_read(&(sq->ref));
}

static inline void set_sem_queue_ref(SemaphoreQueue *sq, int val) {
    atomic_set(&(sq->ref), val);
}

static inline int sem_queue_ref_inc(SemaphoreQueue *sq) {
    return atomic_add_return(&(sq->ref), 1);
}

static inline int sem_queue_ref_dec(SemaphoreQueue *sq) {
    return atomic_sub_return(&(sq->ref), 1);
}

#endif // __KERNEL_SYNC_SEMAPHORE_H__