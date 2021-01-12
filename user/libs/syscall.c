#include <syscall.h>
#include <types.h>
#include <unistd.h>
#include <stdarg.h>

// 系统调用最多支持5个参数
#define MAX_ARGS        5

static inline int syscall(int num, ...) {
    va_list ap;
    va_start(ap, num);
    uint32_t arg[MAX_ARGS];
    int i, ret;
    for (i = 0; i < MAX_ARGS; i++) {
        arg[i] = va_arg(ap, uint32_t);
    }
    va_end(ap);

    asm volatile ("int %1;"
                  : "=a" (ret)
                  : "i" (T_SYSCALL),
                    "a" (num),
                    "d" (arg[0]),
                    "c" (arg[1]),
                    "b" (arg[2]),
                    "D" (arg[3]),
                    "S" (arg[4])
                  : "cc", "memory");
    return ret;
}

int sys_exit(int error_code) {
    return syscall(SYS_exit, error_code);
}

int sys_fork(void) {
    return syscall(SYS_fork);
}

int sys_wait(int pid, int *store) {
    return syscall(SYS_wait, pid, store);
}

int sys_yield(void) {
    return syscall(SYS_yield);
}

int sys_kill(int pid) {
    return syscall(SYS_kill, pid);
}

int sys_sleep(unsigned int time) {
    return syscall(SYS_sleep, time);
}

size_t sys_gettime(void) {
    return syscall(SYS_gettime);
}

int sys_getpid(void) {
    return syscall(SYS_getpid);
}

int sys_brk(uintptr_t *brk_store) {
    return syscall(SYS_brk, brk_store);
}

int sys_putc(int c) {
    return syscall(SYS_putc, c);
}

int sys_page_dir(void) {
    return syscall(SYS_pgdir);
}

int sys_mmap(uintptr_t *addr_store, size_t len, uint32_t mmap_flags) {
    return syscall(SYS_mmap, addr_store, len, mmap_flags);
}

int sys_munmap(uintptr_t addr, size_t len) {
    return syscall(SYS_munmap, addr, len);
}

int sys_shmem(uintptr_t *addr_store, size_t len, uint32_t mmap_flags) {
    return syscall(SYS_shmem, addr_store, len, mmap_flags);
}