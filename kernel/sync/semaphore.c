#include <semaphore.h>
#include <assert.h>
#include <process.h>
#include <sync.h>
#include <slab.h>
#include <wait.h>
#include <error.h>

#define VALID_SEMID(sem_id)         \
    ((uintptr_t)(sem_id) < (uintptr_t)(sem_id) + KERNEL_BASE)

#define semid2sem(sem_id)           \
    ((Semaphore *)((uintptr_t)(sem_id) + KERNEL_BASE))

#define sem2semid(sem)              \
    ((sem_t)((uintptr_t)(sem) - KERNEL_BASE))

void sem_init(Semaphore *sem, int val) {
    sem->value = val;
    set_sem_ref(sem, 0);
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

// todo: 为什么一定要是noinline
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

static void user_sem_up(Semaphore *sem) {
    __up(sem, WT_USEM);
}

static void user_sem_down(Semaphore *sem) {
    uint32_t flags = __down(sem, WT_USEM);
    assert(flags == 0 || flags == WT_INTERRUPTED);
}

SemaphoreQueue *sem_queue_create(void) {
    SemaphoreQueue *sem_queue = NULL;
    if ((sem_queue = kmalloc(sizeof(SemaphoreQueue))) != NULL) {
        sem_init(&(sem_queue->sem), 1);
        set_sem_queue_ref(sem_queue, 0);
        list_init(&(sem_queue->sem_undo_list));
    }
    return sem_queue;
}

void sem_queue_destory(SemaphoreQueue *sem_queue) {
    kfree(sem_queue);
}

SemaphoreUndo *sem_undo_create(Semaphore *sem, int value) {
    SemaphoreUndo *sem_undo = NULL;
    if ((sem_undo = kmalloc(sizeof(SemaphoreUndo))) != NULL) {
        if (sem == NULL && (sem = kmalloc(sizeof(Semaphore))) != NULL) {
            sem_init(sem, value);
        }
        if (sem != NULL) {
            sem_ref_inc(sem);
            sem_undo->sem = sem;
            return sem_undo;
        }
        kfree(sem_undo);
    }
    return NULL;
}

void sem_undo_destory(SemaphoreUndo *sem_undo) {
    if (sem_ref_dec(sem_undo->sem) == 0) {
        kfree(sem_undo->sem);
    }
    kfree(sem_undo);
}

int dup_sem_queue(SemaphoreQueue *to, SemaphoreQueue *from) {
    assert(to != from);
    ListEntry *head = &(from->sem_undo_list);
    ListEntry *entry = head;
    while ((entry = list_next(entry)) != head) {
        SemaphoreUndo *sem_undo = NULL;
        if ((sem_undo = sem_undo_create(le2semundo(entry, sem_undo_link)->sem, 0)) == NULL) {
            return -E_NO_MEM;
        }
        list_add(&(to->sem_undo_list), &(sem_undo->sem_undo_link));
    }
    return 0;
}

void exit_sem_queue(SemaphoreQueue *sem_queue) {
    assert(sem_queue != NULL && sem_queue_ref(sem_queue) == 0);
    ListEntry *head = &(sem_queue->sem_undo_list);
    ListEntry *entry = head;
    while ((entry = list_next(entry)) != head) {
        list_del(entry);
        sem_undo_destory(le2semundo(entry, sem_undo_link));
    }
}


static SemaphoreUndo *sem_undo_list_search(ListEntry *head, sem_t sem_id) {
    if (VALID_SEMID(sem_id)) {
        Semaphore *sem = semid2sem(sem_id);
        ListEntry *entry = head;
        while ((entry = list_next(entry)) != head) {
            SemaphoreUndo *sem_undo = le2semundo(entry, sem_undo_link);
            if (sem_undo->sem == sem) {
                list_del(entry);
                // 找到了在sem_queue中找到了semaphore，则将这个sem_undo放在sem_queue的最前面
                list_add_after(head, entry);
                return sem_undo;
            }
        }
    }
    return NULL;
}

int ipc_sem_init(int val) {
    // 用户态进程一定有信号队列
    assert(current->sem_queue != NULL);
    
    SemaphoreUndo *sem_undo = NULL;
    if ((sem_undo = sem_undo_create(NULL, val)) == NULL) {
        return -E_NO_MEM;
    }

    SemaphoreQueue *sem_queue = current->sem_queue;
    down(&(sem_queue->sem));
    list_add_after(&(sem_queue->sem_undo_list), &(sem_undo->sem_undo_link));
    up(&(sem_queue->sem));
    return sem2semid(sem_undo->sem);
}

int ipc_sem_post(sem_t sem_id) {
    assert(current->sem_queue != NULL);
    SemaphoreUndo *sem_undo = NULL;
    SemaphoreQueue *sem_queue = current->sem_queue;
    down(&(sem_queue->sem));
    sem_undo = sem_undo_list_search(&(sem_queue->sem_undo_list), sem_id);
    up(&(sem_queue->sem));
    if (sem_undo != NULL) {
        user_sem_up(sem_undo->sem);
        return 0;
    }
    return -E_INVAL;
}


int ipc_sem_wait(sem_t sem_id) {
    assert(current->sem_queue != NULL);

    SemaphoreUndo *sem_undo = NULL;
    SemaphoreQueue *sem_queue = current->sem_queue;
    down(&(sem_queue->sem));
    sem_undo = sem_undo_list_search(&(sem_queue->sem_undo_list), sem_id);
    up(&(sem_queue->sem));
    if (sem_undo != NULL) {
        user_sem_down(sem_undo->sem);
        return 0;
    }
    return -E_INVAL;
}

int ipc_sem_get_value(sem_t sem_id, int *value_store) {
    assert(current->sem_queue != NULL);
    if (value_store == NULL) {
        return -E_INVAL;
    }

    MmStruct *mm = current->mm;
    if (!user_mem_check(mm, (uintptr_t)value_store, sizeof(int), true)) {
        return -E_INVAL;
    }

    SemaphoreUndo *sem_undo = NULL;
    SemaphoreQueue *sem_queue = current->sem_queue;
    down(&(sem_queue->sem));
    sem_undo = sem_undo_list_search(&(sem_queue->sem_undo_list), sem_id);
    up(&(sem_queue->sem));
    int ret = -E_INVAL;
    if (sem_undo != NULL) {
        int value = sem_undo->sem->value;
        lock_mm(mm);
        {
            if (copy_to_user(mm, value_store, &value, sizeof(int))) {
                ret = 0;
            }
        }
        unlock_mm(mm);
    }
    return ret;
}