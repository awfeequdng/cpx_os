#include <ulib.h>
#include <syscall.h>
#include <stdio.h>
#include <types.h>
#include <lock.h>

static lock_t fork_lock = INIT_LOCK;

void lock_fork(void) {
    lock(&fork_lock);
}

void unlock_fork(void) {
    unlock(&fork_lock);
}

void exit(int error_code) {
    sys_exit(error_code);
    printf("BUG: exit failed.\n");
    while (1);
}

int fork(void) {
    // 后面需要支持多线程了，在进行fork时需要先加锁
    int ret;
    lock_fork();
    ret = sys_fork();
    unlock_fork();
    return ret;
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