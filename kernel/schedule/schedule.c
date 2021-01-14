#include <list.h>
#include <sync.h>
#include <schedule.h>
#include <assert.h>
#include <stdio.h>
#include <schedule_FCFS.h>
#include <schedule_RR.h>
#include <schedule_MLFQ.h>

static ListEntry timer_list;

static ScheduleClass *schedule_class;

static RunQueue *run_queue;

static inline void schedule_class_enqueue(Process *process) {
    if (process != idle_process) {
        schedule_class->enqueue(run_queue, process);
    }
}

static inline void schedule_class_dequeue(Process *process) {
    schedule_class->dequeue(run_queue, process);
}

static inline Process *schedule_class_pick_next(void) {
    return schedule_class->pick_next(run_queue);
}

static void schedule_class_process_tick(Process *process) {
    if (process != idle_process) {
        schedule_class->process_tick(run_queue, process);
    } else {
        process->need_resched = true;
    }
}

static RunQueue __run_quque[4];

void schedule_init(void) {
    list_init(&timer_list);

    run_queue = __run_quque;
    list_init(&(run_queue->rq_link));
    run_queue->max_time_slice = 8;
    int i;
    for (i = 1; i < ARRAY_SIZE(__run_quque); i++) {
        list_add_before(&(run_queue->rq_link), &(__run_quque[i].rq_link));
        __run_quque[i].max_time_slice = run_queue->max_time_slice * (1 << i);
    }
    
    schedule_class = get_MLFQ_schedule_class();
    // schedule_class = get_RR_schedule_class();
    schedule_class->init(run_queue);

    printk("schedule class: %s\n", schedule_class->name);
}

void wakeup_process(Process *process) {
    assert(process->state != STATE_ZOMBIE);
    bool flag;
    local_intr_save(flag);
    {
        if (process->state != STATE_RUNNABLE) {
            process->state = STATE_RUNNABLE;
            process->wait_state = 0;
            if (process != current) {
                schedule_class_enqueue(process);
            }
        } else {
            warn("wakeup runnable process.\n");
        }
    }
    local_intr_restore(flag);

}

void schedule(void) {
    bool flag;
    Process *next = NULL;
    local_intr_save(flag);
    {
        current->need_resched = false;
        if (current->state == STATE_RUNNABLE) {
            // 当前进程还可以继续被调度
            schedule_class_enqueue(current);
        }
        if ((next = schedule_class_pick_next()) != NULL) {
            schedule_class_dequeue(next);
        }
        if (next == NULL) {
            next = idle_process;
        }
        next->runs++;
        if (next != current) {
            process_run(next);
        }
    }
    local_intr_restore(flag);
}

void add_timer(Timer *timer) {
    bool flag;
    local_intr_save(flag);
    {
        assert(timer->expires > 0 && timer->process != NULL);
        assert(list_empty(&(timer->timer_link)));
        ListEntry *entry = list_next(&timer_list);
        while (entry != &timer_list) {
            Timer *next = le2timer(entry, timer_link);
            if (timer->expires < next->expires) {
                next->expires -= timer->expires;
                break;
            }
            timer->expires -= next->expires;
            entry = list_next(entry);
        }
        list_add_before(entry, &(timer->timer_link));
    }
    local_intr_restore(flag);
}


void del_timer(Timer *timer) {
    bool flag;
    local_intr_save(flag);
    {
        if (!list_empty(&(timer->timer_link))) {
            if (timer->expires != 0) {
                ListEntry *entry = list_next(&timer->timer_link);
                if (entry != &timer_list) {
                    Timer *next = le2timer(entry, timer_link);
                    next->expires += timer->expires;
                }
            }
            list_del_init(&(timer->timer_link));
        }
    }
    local_intr_restore(flag);
}

// 每个时钟滴答执行一次，我们的滴答为10ms一次
void run_timer_list(void) {
    bool flag;
    local_intr_save(flag);
    {
        ListEntry *entry = list_next(&timer_list);
        if (entry != &timer_list) {
            Timer *timer = le2timer(entry, timer_link);
            assert(timer->expires != 0);
            timer->expires--;
            while (timer->expires == 0) {
                entry = list_next(entry);
                Process *process = timer->process;
                if (process->wait_state != 0) {
                    assert(process->wait_state & WT_INTERRUPTED);
                } else {
                    warn("process %d's wait_state == 0.\n", process->pid);
                }
                wakeup_process(process);
                del_timer(timer);
                if (entry == &timer_list) {
                    break;
                }
                timer = le2timer(entry, timer_link);
            }
        }
        // 根据系统滴答来给当前进程计时
        schedule_class_process_tick(current);
    }
    local_intr_restore(flag);
}