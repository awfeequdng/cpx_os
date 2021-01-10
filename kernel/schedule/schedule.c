#include <list.h>
#include <sync.h>
#include <schedule.h>
#include <assert.h>

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