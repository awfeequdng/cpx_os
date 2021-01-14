#include <schedule_RR.h>
#include <list.h>
#include <assert.h>
#include <process.h>
#include <types.h>

static void RR_init(RunQueue *rq) {
    list_init(&rq->run_list);
    rq->process_count = 0;
}


static void RR_enqueue(RunQueue *rq, Process *process) {
    assert(list_empty(&(process->run_link)));
    list_add_before(&(rq->run_list), &(process->run_link));
    if (process->time_slice == 0 || process->time_slice > rq->max_time_slice) {
        process->time_slice = rq->max_time_slice;
    }
    process->rq = rq;
    rq->process_count++;
}

static void RR_dequeue(RunQueue *rq, Process *process) {
    assert(!list_empty(&(process->run_link)) && process->rq == rq);
    list_del_init(&(process->run_link));
    rq->process_count--;
}


static Process *RR_pick_next(RunQueue *rq) {
    ListEntry *entry = list_next(&(rq->run_list));
    if (entry != &(rq->run_list)) {
        return le2process(entry, run_link);
    }
    return NULL;
}

static void RR_process_tick(RunQueue *rq, Process *process) {
    if (process->time_slice > 0) {
        process->time_slice --;
    }
    if (process->time_slice == 0) {
        process->need_resched = 1;
    }
}

ScheduleClass RR_schedule_class = {
    .name = "RR_scheduler",
    .init = RR_init,
    .enqueue = RR_enqueue,
    .dequeue = RR_dequeue,
    .pick_next = RR_pick_next,
    .process_tick = RR_process_tick,
};


ScheduleClass *get_RR_schedule_class(void) {
    return &RR_schedule_class;
}