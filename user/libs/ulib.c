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

int mmap(uintptr_t *addr_store, size_t len, uint32_t mmap_flags) {
    return sys_mmap(addr_store, len, mmap_flags);
}

int munmap(uintptr_t addr, size_t len) {
    return sys_munmap(addr, len);
}

int shmem(uintptr_t *addr_store, size_t len, uint32_t mmap_flags) {
    return sys_shmem(addr_store, len, mmap_flags);
}