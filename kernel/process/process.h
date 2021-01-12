#ifndef __KERNEL_PROCESS_PROCESS_H__
#define __KERNEL_PROCESS_PROCESS_H__

#include <types.h>
#include <list.h>
// 循环引用了 vmm.h -> sync.h -> schedule.h -> process.h -> vmm.h
// 因此我们将vmm.h头文件注释掉，然后将struct mm_struct结构体声明以下（这个process.h中会使用）
#include <vmm.h>
#include <trap.h>

enum ProcessState {
    STATE_UNINIT = 0,
    STATE_SLEEPING,
    STATE_RUNNABLE,
    STATE_ZOMBIE,
};

// 在进程进行切换时，需要保存的寄存器
// 在进程切换时不需要保存所有的寄存器
// 常规寄存器需要保存，但是作为返回值的寄存器eax不用保存(不保存eax可以简化进程切换的代码)，
// Context的布局需要和switch.S的代码相匹配
typedef struct context_struct {
    uint32_t eip;
    uint32_t esp;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
} Context;


#define PROCESS_NAME_LEN    15
#define MAX_PROCESS         4096
#define MAX_PID             (MAX_PROCESS * 2)

// ListEntry *get_process_list(void);
extern ListEntry process_list;

struct mm_struct;

typedef struct process_struct {
    enum ProcessState state;
    int pid;
    int runs;                       // 进程的运行次数
    uintptr_t kstack;               // 进程的内核堆栈
    volatile bool need_resched;     // 是否需要重新调度以释放CPU
    struct process_struct *parent;
    struct mm_struct *mm;                   // 进程的内存管理结构
    Context context;                // 进程切换时，根据context的内容切换到目的地址
    struct TrapFrame *tf;           // 当前中断的栈结构
    uintptr_t page_dir;             // 进程页目录地址
    uint32_t flags;                 // 进程标志
    char name[PROCESS_NAME_LEN + 1];
    ListEntry process_link;      // 进程双向链表，所有进程挂载在process_list链表下
    ListEntry hash_link;         // 进程的hash list
    int exit_code;
    uint32_t wait_state;
    // child为孩子节点，left_sibling为左边的兄弟，right_sibling为右边的兄弟
    struct process_struct *child, *left_sibling, *right_sibling;
} Process;

#define PF_EXITING                  0x00000001  // getting shutdown

#define WT_CHILD                    (0x00000001 | WT_INTERRUPTED)
#define WT_TIMER                    (0x00000002 | WT_INTERRUPTED)
#define WT_INTERRUPTED               0x80000000

#define le2process(le, member)      \
    container_of((le), Process, member)

extern Process *idle_process;
extern Process *init_process;
extern Process *current;
// Process *get_idle_process(void);
// Process *get_init_process(void);
// Process *get_current_process(void);

void process_init(void);
void process_run(Process *process);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);

char *set_process_name(Process *process, const char *name);
char *get_process_name(Process *process);
void cpu_idle(void) __attribute__((noreturn));

Process *find_process(int pid);
int do_fork(uint32_t clone_flags, uintptr_t statck, struct TrapFrame *tf);
int do_exit(int error_code);
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size);
int do_yield(void);
int do_wait(int pid, int *code_store);
int do_kill(int pid);
int do_brk(uintptr_t *brk_store);
int do_sleep(unsigned int time);
#endif // __KERNEL_PROCESS_PROCESS_H__