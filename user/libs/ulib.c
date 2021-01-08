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

void yield(void) {
    sys_yield();
}

int getpid(void) {
    return sys_getpid();
}

void print_page_dir(void) {
    sys_page_dir();
}