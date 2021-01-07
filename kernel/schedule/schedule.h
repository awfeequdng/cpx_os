#ifndef __KERNEL_SCHEDULE_SCHEDULE_H__
#define __KERNEL_SCHEDULE_SCHEDULE_H__

#include <process.h>

void schedule(void);
void wakeup_process(Process *process);

#endif // __KERNEL_SCHEDULE_SCHEDULE_H__