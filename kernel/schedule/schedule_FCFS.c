#include <schedule_FCFS.h>

static void FCFS_init(RunQueue *rq) {
    list_init(&(rq->run_list));
    rq->process_count = 0;
}

static void FCFS_enqueue(RunQueue *rq, Process *process) {
    assert(list_empty(&(process->run_link)));
    list_add_before(&(rq->run_list), &(process->run_link));
    process->rq = rq;
    rq->process_count++;
}

static void FCFS_dequeue(RunQueue *rq, Process *process) {
    assert(!list_empty(&(process->run_link)) && process->rq == rq);
    list_del_init(&(process->run_link));
    rq->process_count--;
}

static Process * FCFS_pick_next(RunQueue *rq) {
    ListEntry *entry = list_next(&(rq->run_list));
    if (entry != &(rq->run_list)) {
        return le2process(entry, run_link);
    }
    return NULL;
}

static void FCFS_process_tick(RunQueue *rq, Process *process) {
    // do nothing
}

static ScheduleClass FCFS_schedule_class = {
    .name = "FCFS_scheduler",
    .init = FCFS_init,
    .enqueue = FCFS_enqueue,
    .dequeue = FCFS_dequeue,
    .pick_next = FCFS_pick_next,
    .process_tick = FCFS_process_tick,
};

ScheduleClass *get_FCFS_schedule_class(void) {
    return &FCFS_schedule_class;
}
