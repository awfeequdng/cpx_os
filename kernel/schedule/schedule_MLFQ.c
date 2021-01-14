#include <list.h>
#include <assert.h>
#include <process.h>
#include <types.h>
#include <schedule_RR.h>
#include <schedule_MLFQ.h>

static ScheduleClass *schedule_class;

static void MLFQ_init(RunQueue *rq) {
    schedule_class = get_RR_schedule_class();
    ListEntry *head = &(rq->rq_link);
    ListEntry *entry = head;
    do {
        schedule_class->init(le2runqueue(entry, rq_link));
        entry = list_next(entry);
    } while (entry != head);
}

static void MLFQ_enqueue(RunQueue *rq, Process *process) {
    assert(list_empty(&(process->run_link)));
    RunQueue *next_rq = rq;
    // 进程的时间片用完了，但是还没有完成任务，需要继续放入队列（放入下一个时间片更大的队列中）
    if (process->rq != NULL && process->time_slice == 0) {
        next_rq = le2runqueue(list_next(&(process->rq->rq_link)), rq_link);
        if (next_rq == rq) {
            // 如果下一个队列的时间片比当前队列时间片小的话，则还是放入当前队列
            next_rq = process->rq;
        }
    }
    schedule_class->enqueue(next_rq, process);
}


static void MLFQ_dequeue(RunQueue *rq, Process *process) {
    assert(!list_empty(&process->run_link));
    schedule_class->dequeue(process->rq, process);
}

static Process *MLFQ_pick_next(RunQueue *rq) {
    Process *next = NULL;
    ListEntry *head = &(rq->rq_link);
    ListEntry *entry = head;

    do {
        if ((next = schedule_class->pick_next(le2runqueue(entry, rq_link))) != NULL) {
            break;
        }
        entry = list_next(entry);
    } while (entry != head);
    return next;
}

static void MLFQ_process_tick(RunQueue *rq, Process *process) {
    schedule_class->process_tick(process->rq, process);
}

ScheduleClass MLFQ_schedule_class = {
    .name = "MLFQ_scheduler",
    .init = MLFQ_init,
    .enqueue = MLFQ_enqueue,
    .dequeue = MLFQ_dequeue,
    .pick_next = MLFQ_pick_next,
    .process_tick = MLFQ_process_tick,
};

ScheduleClass *get_MLFQ_schedule_class(void) {
    return &MLFQ_schedule_class;
}