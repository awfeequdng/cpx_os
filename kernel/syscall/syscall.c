#include <syscall.h>
#include <process.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>


static uint32_t sys_exit(uint32_t arg[]) {
    int error_code = (int)arg[0];
    return do_exit(error_code);
}

static uint32_t sys_fork(uint32_t arg[]) {
    struct TrapFrame *tf = current->tf;
    uintptr_t stack = tf->tf_esp;
    return do_fork(0, stack, tf);
}

static uint32_t sys_wait(uint32_t arg[]) {
    int32_t pid = (int32_t)arg[0];
    int32_t *store = (int32_t *)arg[1];
    return do_wait(pid, store);
}

static uint32_t sys_exec(uint32_t arg[]) {
    const char *name = (const char *)arg[0];
    size_t len = (size_t)arg[1];
    unsigned char *binary = (unsigned char *)arg[2];
    size_t size = (size_t)arg[3];
    return do_execve(name, len, binary, size);
}

static uint32_t sys_kill(uint32_t arg[]) {
    int32_t pid = (int32_t)arg[0];
    return do_kill(pid);
}

static uint32_t sys_yield(uint32_t arg[]) {
    return do_yield();
}

static uint32_t sys_getpid(uint32_t arg[]) {
    return current->pid;
}

static uint32_t sys_brk(uint32_t arg[]) {
    uintptr_t *brk_store = (uintptr_t *)arg[0];
    return do_brk(brk_store);
}

static uint32_t sys_putc(uint32_t arg[]) {
    int c = (int)arg[0];
    putchar(c);
    return 0;
}

static uint32_t sys_page_dir(uint32_t arg[]) {
    // todo:
    return 0;
}

static uint32_t (*syscalls[])(uint32_t arg[]) = {
    [SYS_exit] = sys_exit,
    [SYS_fork] = sys_fork,
    [SYS_wait] = sys_wait,
    [SYS_exec] = sys_exec,
    [SYS_yield] = sys_yield,
    [SYS_kill] = sys_kill,
    [SYS_getpid] = sys_getpid,
    [SYS_brk] = sys_brk,
    [SYS_putc] = sys_putc,
    [SYS_pgdir] = sys_page_dir,
};

#define NUM_SYSCALLS        ((sizeof(syscalls)) / (sizeof(syscalls[0])))

void syscall(void) {
    struct TrapFrame *tf = current->tf;
    uint32_t arg[5];
    // eax保存系统调用号
    // 系统调用最多带五个参数
    int num = tf->tf_regs.reg_eax;
    if (num >= 0 && num < NUM_SYSCALLS) {
        if (syscalls[num] != NULL) {
            arg[0] = tf->tf_regs.reg_edx;
            arg[1] = tf->tf_regs.reg_ecx;
            arg[2] = tf->tf_regs.reg_ebx;
            arg[3] = tf->tf_regs.reg_edi;
            arg[4] = tf->tf_regs.reg_esi;
            tf->tf_regs.reg_eax = syscalls[num](arg);
            return;
        }
    }
    print_trap_frame(tf);
    panic("undefined syscall %d, pid = %d, name = %s.\n",
            num, current->pid, current->name);
}
