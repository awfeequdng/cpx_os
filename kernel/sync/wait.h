#ifndef __KERNEL_SYNC_WAIT_H__
#define __KERNEL_SYNC_WAIT_H__

#include <list.h>

typedef struct wait_queue_t {
    ListEntry wait_head;
} WaitQueue;

struct process_struct;

typedef struct wait_t {
    // struct process_struct *process;
    struct process_struct *process;
    uint32_t wakeup_flags;
    WaitQueue *wait_queue;
    ListEntry wait_link;
} Wait;

#define le2wait(le, member)     \
    container_of((le), Wait, member)

void wait_init(Wait *wait, struct process_struct *process);
void wait_queue_init(WaitQueue *queue);
void wait_queue_add(WaitQueue *queue, Wait *wait);
void wait_queue_del(WaitQueue *queue, Wait *wait);

Wait *wait_queue_next(WaitQueue *queue, Wait *wait);
Wait *wait_queue_prev(WaitQueue *queue, Wait *wait);
Wait *wait_queue_first(WaitQueue *queue);
Wait *wait_queue_last(WaitQueue *queue);

bool wait_queue_empty(WaitQueue *queue);
bool wait_in_queue(Wait *wait);
void wakeup_wait(WaitQueue *queue, Wait *wait, uint32_t wakeup_flags, bool del);
void wakeup_first(WaitQueue *queue, uint32_t wakeup_flags, bool del);
void wakeup_queue(WaitQueue *queue, uint32_t wakeup_flags, bool del);

void wait_current_set(WaitQueue *queue, Wait *wait, uint32_t wait_state);

#define wait_current_del(queue, wait)       \
    do {    \
        if (wait_in_queue(wait)) { \
            wait_queue_del(queue, wait);   \
        }       \
    } while (0)

#endif // __KERNEL_SYNC_WAIT_H__
