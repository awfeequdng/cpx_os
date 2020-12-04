#ifndef __KERNEL_MONITOR_H__
#define __KERNEL_MONITOR_H__

#ifndef CPX_OS_KERNEL
# error "this is a cpx_os kernel header; user programs should not #include it"
#endif

#include <trap.h>
//struct TrapFrame;

// activate the kernel monitor
// optionally providing a trap frame indicating the current state
// NULL if none
void monitor(struct TrapFrame *tf);

int monitor_help(int argc, char **argv, struct TrapFrame *tf);
int monitor_kernel_info(int argc, char **argv, struct TrapFrame *tf);
int monitor_backtrace(int argc, char **argv, struct TrapFrame *tf);

#endif // __KERNEL_MONITOR_H__
