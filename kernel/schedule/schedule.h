#ifndef __KERNEL_SCHEDULE_SCHEDULE_H__
#define __KERNEL_SCHEDULE_SCHEDULE_H__

#include <process.h>
#include <types.h>
#include <list.h>


typedef struct timer_t {
    unsigned int expires;
    Process *process;
    ListEntry timer_link;
} Timer;

#define le2timer(le, member)        \
    container_of((le), Timer, member)

static inline Timer *timer_init(Timer *timer, Process *process, int expires) {
    timer->expires = expires;
    timer->process = process;
    list_init(&(timer->timer_link));
    return timer;
}

typedef struct run_queue {
    ListEntry run_list;
    unsigned int process_count;
} RunQueue;

typedef struct sched_class {
    const char *name;
    void (*init)(RunQueue *rq);
    void (*enqueue)(RunQueue *rq, Process *process);
    void (*dequeue)(RunQueue *rq, Process *process);
    Process *(*pick_next)(RunQueue *rq);
    void (*process_tick)(RunQueue *rq, Process *process);
} ScheduleClass;

void schedule_init(void);
void schedule(void);
void wakeup_process(Process *process);
void add_timer(Timer *timer);
void del_timer(Timer *timer);
void run_timer_list(void);

#endif // __KERNEL_SCHEDULE_SCHEDULE_H__