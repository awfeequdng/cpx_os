#include <wait.h>
#include <sync.h>
#include <process.h>
#include <schedule.h>

#include <types.h>

void wait_init(Wait *wait, Process *process) {
    wait->process = process;
    wait->wakeup_flags = WT_INTERRUPTED;
    list_init(&(wait->wait_link));
}

void wait_queue_init(WaitQueue *queue) {
    list_init(&(queue->wait_head));
}

void wait_queue_add(WaitQueue *queue, Wait *wait) {
    assert(list_empty(&(wait->wait_link)) && wait->process != NULL);
    wait->wait_queue = queue;
    list_add_before(&(queue->wait_head), &(wait->wait_link));
}

void wait_queue_del(WaitQueue *queue, Wait *wait) {
    assert(!list_empty(&(wait->wait_link)) && wait->wait_queue == queue);
    list_del_init(&(wait->wait_link));
}

Wait *wait_queue_next(WaitQueue *queue, Wait *wait) {
    assert(!list_empty(&(wait->wait_link)) && wait->wait_queue == queue);
    ListEntry *entry = list_next(&(wait->wait_link));
    if (entry != &(queue->wait_head)) {
        return le2wait(entry, wait_link);
    }
    return NULL;
}

Wait *wait_queue_prev(WaitQueue *queue, Wait *wait) {
    assert(!list_empty(&(wait->wait_link)) && wait->wait_queue == queue);
    ListEntry *entry = list_prev(&(wait->wait_link));
    if (entry != &(queue->wait_head)) {
        return le2wait(entry, wait_link);
    }
    return NULL;
}

Wait *wait_queue_first(WaitQueue *queue) {
    ListEntry *entry = list_next(&(queue->wait_head));
    if (entry != &(queue->wait_head)) {
        return le2wait(entry, wait_link);
    }
    return NULL;
}

Wait *wait_queue_last(WaitQueue *queue) {
    ListEntry *entry = list_prev(&(queue->wait_head));
    if (entry != &(queue->wait_head)) {
        return le2wait(entry, wait_link);
    }
    return NULL;
}

bool wait_queue_empty(WaitQueue *queue) {
    return list_empty(&(queue->wait_head));
}

bool wait_in_queue(Wait *wait) {
    return !list_empty(&(wait->wait_link));
}

void wakeup_wait(WaitQueue *queue, Wait *wait, uint32_t wakeup_flags, bool del) {
    if (del) {
        wait_queue_del(queue, wait);
    }
    wait->wakeup_flags = wakeup_flags;
    wakeup_process(wait->process);
}

void wakeup_first(WaitQueue *queue, uint32_t wakeup_flags, bool del) {
    Wait *wait = NULL;
    if ((wait = wait_queue_first(queue)) != NULL) {
        wakeup_wait(queue, wait, wakeup_flags, del);
    }
}

// 将等待的所有进程都唤醒
void wakeup_queue(WaitQueue *queue, uint32_t wakeup_flags, bool del) {
    Wait *wait = NULL;
    if ((wait = wait_queue_first(queue)) != NULL) {
        if (del) {
            do {
                wakeup_wait(queue, wait, wakeup_flags, del);
            } while ((wait = wait_queue_first(queue)) != NULL);
        } else {
            do {
                wakeup_wait(queue, wait, wakeup_flags, false);
            } while ((wait = wait_queue_next(queue, wait)) != NULL);
        }
    }
}

void wait_current_set(WaitQueue *queue, Wait *wait, uint32_t wait_state) {
    assert(current != NULL);
    wait_init(wait, current);
    current->state = STATE_SLEEPING;
    current->wait_state = wait_state;
    wait_queue_add(queue, wait);
}