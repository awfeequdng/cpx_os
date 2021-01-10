#include <list.h>
#include <sync.h>
#include <schedule.h>
#include <assert.h>

static list_entry_t timer_list;

void schedule_init(void) {
    list_init(&timer_list);
}

void wakeup_process(Process *process) {
    assert(process->state != STATE_ZOMBIE);
    bool flag;
    local_intr_save(flag);
    {
        if (process->state != STATE_RUNNABLE) {
            process->state = STATE_RUNNABLE;
            process->wait_state = 0;
        } else {
            warn("wakeup runnable process.\n");
        }
    }
    local_intr_restore(flag);

}

void schedule(void) {
    bool flag;
    list_entry_t *entry = NULL;
    list_entry_t *start = NULL;
    Process *next = NULL;
    local_intr_save(flag);
    {
        current->need_resched = 0;
        start = (current == idle_process) ? &process_list : &(current->process_link);
        entry = start;
        do {
            if ((entry = list_next(entry)) != &process_list) {
                next = le2process(entry, process_link);
                if (next->state == STATE_RUNNABLE) {
                    break;
                }
            }
        } while (entry != start);

        if (next == NULL || next->state != STATE_RUNNABLE) {
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
        list_entry_t *entry = list_next(&timer_list);
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
                list_entry_t *entry = list_next(&timer->timer_link);
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
        list_entry_t *entry = list_next(&timer_list);
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
    }
    local_intr_restore(flag);
}