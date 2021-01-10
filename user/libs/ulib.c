#include <ulib.h>
#include <syscall.h>
#include <stdio.h>
#include <types.h>

void exit(int error_code) {
    sys_exit(error_code);
    printf("BUG: exit failed.\n");
    while (1);
}

int fork(void) {
    return sys_fork();
}

int wait(void) {
    return sys_wait(0, NULL);
}

int waitpid(int pid, int *store) {
    return sys_wait(pid, store);
}

void yield(void) {
    sys_yield();
}

int kill(int pid) {
    return sys_kill(pid);
}

int sleep(unsigned int time) {
    return sys_sleep(time);
}

unsigned int gettime_msec(void) {
    return sys_gettime();
}

int getpid(void) {
    return sys_getpid();
}

void print_page_dir(void) {
    sys_page_dir();
}