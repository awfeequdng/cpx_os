#ifndef __KERNEL_SCHEDULE_SCHEDULE_H__
#define __KERNEL_SCHEDULE_SCHEDULE_H__

#include <process.h>
#include <types.h>
#include <list.h>

typedef struct timer_t {
    unsigned int expires;
    Process *process;
    list_entry_t timer_link;
} Timer;

#define le2timer(le, member)        \
    container_of((le), Timer, member)

static inline Timer *timer_init(Timer *timer, Process *process, int expires) {
    timer->expires = expires;
    timer->process = process;
    list_init(&(timer->timer_link));
    return timer;
}

void schedule_init(void);
void schedule(void);
void wakeup_process(Process *process);
void add_timer(Timer *timer);
void del_timer(Timer *timer);
void run_timer_list(void);

#endif // __KERNEL_SCHEDULE_SCHEDULE_H__