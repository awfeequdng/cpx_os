#include <process.h>
#include <slab.h>
#include <sync.h>
#include <pmm.h>
#include <assert.h>
#include <vmm.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <error.h>
#include <stdio.h>
#include <schedule.h>
#include <elf.h>

list_entry_t process_list;

#define HASH_SHIFT          10
#define HASH_LIST_SIZE      (1 << HASH_SHIFT)
#define pid_hashfn(x)       (hash32(x, HASH_SHIFT))

static list_entry_t hash_list[HASH_LIST_SIZE];

Process *idle_process = NULL;

Process *init_process = NULL;

Process *current = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct TrapFrame *tf);
void switch_to(Context *from, Context *to);

static Process *alloc_process(void) {
    Process *process = kmalloc(sizeof(Process));
    if (process != NULL) {
        process->state = STATE_UNINIT;
        process->pid = -1;
        process->runs = 0;
        process->kstack = 0;
        process->need_resched = false;
        process->parent = NULL;
        process->mm = NULL;
        memset(&(process->context), 0, sizeof(Context));
        process->tf = NULL;
        process->page_dir = get_boot_page_dir();
        process->flags = 0;
        memset(process->name, 0, PROCESS_NAME_LEN);
    }
    return process;
}

char *set_process_name(Process *process, const char *name) {
    memset(process->name, 0, sizeof(process->name));
    return memcpy(process->name, name, PROCESS_NAME_LEN);
}

// 对返回name的修改不会影响到process的name
char *get_process_name(Process *process) {
    static char name[PROCESS_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, process->name, PROCESS_NAME_LEN);
}

// 分配一个没有使用的pid
// get_pid函数的意图很明显，就是找到一个pid分配给进程。首先变量last_pid，
// 用于记录上一次分配给进程时的pid值。而分配的过程，一般而言是last_pid+1，
// 如果超出pid个数的最大值（MAX_PID），那么进程pid值从1开始重新查找未用的。
// 也就是说，一般用户进程的pid值范围[1，MAX_PID]。
// 对于变量next_safe的含义是，在[last_pid, next_safe)之间，都是没有使用过
// 的pid，一旦last_pid+1大于了next_safe，也就是说pid值进入了不可靠空间，
// 有可能这个值被使用，这时需要遍历process来确认。这样遍历process，找到一个没有
// 用过的pid，同时，确定next_safe，以保证last_pid到next_safe的区间中pid
// 是空闲的，这样只要再次分配pid时，其值小于next_safe就可以直接得到，而不
// 需要遍历process来查找空闲的pid。
// todo: 这段代码还没有看懂？？？
static int get_pid(void) {
    static_assert(MAX_PID > MAX_PROCESS);
    Process *process = NULL;
    list_entry_t *head = &process_list;
    list_entry_t *entry = NULL;
    static int next_safe = MAX_PID;
    static int last_pid = MAX_PID;
    if (++last_pid >= MAX_PID) {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe) {
    inside:
        next_safe = MAX_PID;
    repeat:
        entry = head;
        while ((entry = list_next(entry)) != head) {
            process = le2process(entry, process_link);
            if (process->pid == last_pid) {
                if (++last_pid >= next_safe) {
                    if (last_pid >= MAX_PID) {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            } else if (process->pid > last_pid && next_safe > process->pid) {
                next_safe = process->pid;
            }
        }
    }
    return last_pid;
}

void process_run(Process *process) {
    if (process != NULL) {
        bool flag;
        Process *prev_process = current;
        Process *next_process = process; 
        local_intr_save(flag);
        {
            current = process;
            load_esp0(next_process->kstack + K_STACK_SIZE);
            lcr3(PADDR(next_process->page_dir));
            switch_to(&(prev_process->context), &(next_process->context));
        }
        local_intr_restore(flag);
    }
}


static void forkret(void) {
    forkrets(current->tf);
}

// 将进程添加到hash list
static void hash_process(Process *process) {
    list_add(hash_list + pid_hashfn(process->pid), &(process->hash_link));
}

Process *find_process(int pid) {
    if (0 < pid && pid < MAX_PID) {
        list_entry_t *head = hash_list + pid_hashfn(pid);
        list_entry_t *entry = head;
        while ((entry = list_next(entry)) != head) {
            Process *process = le2process(entry, hash_link);
            if (process->pid == pid) {
                return process;
            }
        }
    }
    return NULL;
}

int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags) {
    struct TrapFrame tf;
    memset(&tf, 0, sizeof(struct TrapFrame));
    tf.tf_cs = KERNEL_CS;
    tf.tf_ds = tf.tf_es = tf.tf_ss = KERNEL_DS;
    tf.tf_regs.reg_ebx = (uint32_t)fn;
    tf.tf_regs.reg_edx = (uint32_t)arg;
    tf.tf_eip = (uint32_t)kernel_thread_entry;
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

static int setup_kstack(Process *process) {
    struct Page *page = alloc_pages(K_STACK_PAGE);
    if (page != NULL) {
        process->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

static void put_kstack(Process *process) {
    free_pages(kva2page((void *)process->kstack), K_STACK_PAGE);
}


static int setup_page_dir(MmStruct *mm) {
    struct Page *page = NULL;
    if ((page = alloc_page()) == NULL) {
        return -E_NO_MEM;
    }
    pde_t *page_dir = page2kva(page);
    memcpy(page_dir, get_boot_page_dir(), PAGE_SIZE);
    page_dir[PDX(VPT)] = PADDR(page_dir) | PTE_P | PTE_W;
    mm->page_dir = page_dir;
    return 0;
}

// 根据clone_flags标志是否复制或者共享当前的进程'current'
static int copy_mm(uint32_t clone_flags, Process *process) {
    MmStruct *mm= NULL;
    MmStruct *old_mm = current->mm;
    if (old_mm == NULL) {
        return 0;
    }
    if (clone_flags & CLONE_VM) {
        // 设置了CLONE_VM标志，则两个进程使用同一个mm
        mm = old_mm;
        goto good_mm;
    }
    int ret = -E_NO_MEM;
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }

    if (setup_page_dir(mm) != 0) {
        goto bad_page_dir_cleanup_mm;
    }

    lock_mm(old_mm);
    {
        ret = dup_mmap(mm, old_mm);
    }
    unlock_mm(old_mm);

    if (ret != 0) {
        goto bad_dup_cleanup_mmap;
    }

good_mm:
    mm_count_inc(mm);
    process->mm = mm;
    process->page_dir = mm->page_dir;
    return 0;
bad_dup_cleanup_mmap:

bad_page_dir_cleanup_mm:

bad_mm:
    return ret;
}

static void copy_thread(Process *process, uintptr_t esp, struct TrapFrame *tf) {
    process->tf = (struct TrapFrame *)(process->kstack + K_STACK_SIZE) - 1;
    *(process->tf) = *tf;
    process->tf->tf_regs.reg_eax = 0;
    process->tf->tf_esp = esp;
    process->tf->tf_eflags |= FL_IF;

    process->context.eip = (uintptr_t)forkret;
    process->context.esp = (uintptr_t)(process->tf);
}

int do_fork(uint32_t clone_flags, uintptr_t stack, struct TrapFrame *tf) {
    int ret = -E_NO_FREE_PROCESS;
    Process *process = NULL;
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }

    ret = -E_NO_MEM;

    if ((process = alloc_process()) == NULL) {
        goto fork_out;
    }

    process->parent = current;

    if (setup_kstack(process) != 0) {
        goto bad_fork_cleanup_process;
    }
    if (copy_mm(clone_flags, process) != 0) {
        goto bad_fork_cleanup_kstack;
    }
    copy_thread(process, stack, tf);

    bool flag;
    local_intr_save(flag);
    {
        process->pid = get_pid();
        hash_process(process);
        list_add(&process_list, &(process->process_link));
        nr_process++;
    }
    local_intr_restore(flag);

    wakeup_process(process);

    ret = process->pid;

fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(process);
bad_fork_cleanup_process:
    kfree(process);
    goto fork_out;
}


int do_exit(int error_code) {
    if (current == idle_process) {
        panic("idle_process exit.\n");
    }
    if (current == init_process) {
        panic("init_process exit.\n");
    }

    panic("process exit, pid = %d, error_code = %d\n", current->pid, error_code);
}

static void put_page_dir(MmStruct *mm) {
    free_page(kva2page(mm->page_dir));
}

// load_icode -  called by sys_exec-->do_execve
// 1. create a new mm for current process
// 2. create a new PDT, and mm->pgdir= kernel virtual addr of PDT
// 3. copy TEXT/DATA/BSS parts in binary to memory space of process
// 4. call mm_map to setup user stack, and put parameters into user stack
// 5. setup trapframe for user environment
static int load_icode(unsigned char *binary, size_t size) {
    if (current->mm != NULL) {
        panic("load_icode: current->mm must be empty.\n");
    }

    int ret = -E_NO_MEM;

    MmStruct *mm = NULL;

    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }

    if (setup_page_dir(mm) != 0) {
        goto bad_page_dir_cleanup_mm;
    }

    struct Page *page = NULL;
    struct Elf *elf = (struct Elf*)binary;
    struct ProgHeader *ph = (struct ProgHeader *)(binary + elf->e_phoff);
    if (elf->e_magic != ELF_MAGIC) {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_page_dir;
    }
    uint32_t vm_flags, perm;
    struct ProgHeader *ph_end = ph + elf->e_phnum;
    for (; ph < ph_end; ph++) {
        if (ph->p_type != ELF_PT_LOAD) {
            // 不是可加载的program 段
            continue;
        }
        // file size 和mem size的关系是什么？
        // todo: 
        if (ph->p_filesz > ph->p_memsz) {
            ret = -E_INVAL_ELF;
            goto bad_cleanup_mmap;
        }
        if (ph->p_filesz == 0) {
            continue;
        }
        vm_flags = 0;
        // 用户态程序
        perm = PTE_U;
        if (ph->p_flags & ELF_PF_X) {
            vm_flags |= VM_EXEC;
        }
        if (ph->p_flags & ELF_PF_W) {
            vm_flags |= VM_WRITE;
        }
        if (ph->p_flags & ELF_PF_R) {
            vm_flags |= VM_READ;
        }
        if (vm_flags & VM_WRITE) {
            perm |= PTE_W;
        }
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0) {
            goto bad_cleanup_mmap;
        }

        unsigned char *from = binary + ph->p_offset;
        size_t off, size;
        uintptr_t start = ph->p_va;
        uintptr_t end;
        uintptr_t va = ROUNDDOWN(start, PAGE_SIZE);
        ret = -E_NO_MEM;

        end = ph->p_va + ph->p_filesz;
        // 将可加载段段的内容拷贝到对应的vma
        while (start < end) {
            if ((page = page_dir_alloc_page(mm->page_dir, va, perm)) == NULL) {
                goto bad_cleanup_mmap;
            }
            off = start - va;
            size = PAGE_SIZE - off;
            va += PAGE_SIZE;
            if (end < va) {
                size -= (va - end);
            }
            memcpy(page2kva(page) + off, from, size);
            start += size;
            from += size;
        }
        end = ph->p_va + ph->p_memsz;

        // start到va(va是页边界)还有有段空缺的内容没有使用
        if (start < va) {
            if (start == end) {
                // 可执行文件中的filesz和memsz大小相等
                continue;
            }
            // 可加载段的filesz和memsz的大小不相等，memsz大于filesz，
            // 说明filesz到memsz之间需要填充零
            // bss段就是filesz为0，而memsz为未初始化全局变量和局部静态变量的总大小
            off = start + PAGE_SIZE - va;
            size = PAGE_SIZE - off;
            if (end < va) {
                size -= (va - end);
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
            assert((end < va && start == end) || (end >= va && start == va));
        }

        while (start < end) {
            if ((page = page_dir_alloc_page(mm->page_dir, va, perm)) == NULL) {
                goto bad_cleanup_mmap;
            }
            off = start - va;
            size = PAGE_SIZE - off;
            va += PAGE_SIZE;
            if (end < va) {
                size -= (va - end);
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
        }
    }
    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    // 设置堆栈的vma以及权限
    if ((ret = mm_map(mm, USER_STACK_TOP - USER_STACK_SIZE, USER_STACK_TOP, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }

    mm_count_inc(mm);
    current->mm = mm;
    current->page_dir = mm->page_dir;
    lcr3(PADDR(mm->page_dir));

    struct TrapFrame *tf = current->tf;
    memset(tf, 0, sizeof(struct TrapFrame));

    tf->tf_cs = USER_CS;
    tf->tf_ds = USER_DS;
    tf->tf_es = USER_DS;
    tf->tf_ss = USER_DS;
    tf->tf_esp = USER_STACK_TOP;
    tf->tf_eip = elf->e_entry;
    tf->tf_eflags = FL_IF;
    ret = 0;
out:
    return ret;
bad_cleanup_mmap:
    exit_mmap(mm);
bad_elf_cleanup_page_dir:
    put_page_dir(mm);
bad_page_dir_cleanup_mm:
    mm_destory(mm);
bad_mm:
    return ret;
}

int do_execve(const char *name, size_t len, unsigned char *binary, size_t size) {
    MmStruct *mm = current->mm;
    if (!user_mem_check(mm, (uintptr_t)name, len, 0)) {
        return -E_INVAL;
    }

    if (len > PROCESS_NAME_LEN) {
        len = PROCESS_NAME_LEN;
    }

    char local_name[PROCESS_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));
    memcpy(local_name, name, len);
    
    if (mm != NULL) {
        // 切换到内核地址空间，因为进程mm要被释放掉了
        lcr3(PADDR(get_boot_page_dir()));
        if (mm_count_dec(mm) == 0) {
            // 释放所有页表项映射的page，以及释放所有的页表，最后只留下一个页目录
            exit_mmap(mm);
            // 将页目录也是释放掉
            put_page_dir(mm);
            // 释放所有vma结构以及mm结构
            mm_destory(mm);
        }
        current->mm = NULL;
    }

    int ret;
    // 加载新的进程地址空间到current
    if ((ret = load_icode(binary, size)) != 0) {
        goto execve_exit;
    }
    set_process_name(current, local_name);
    return 0;

execve_exit:
    do_exit(ret);
    panic("already exit: %e.\n", ret);
}

int do_yield(void) {
    current->need_resched = 1;
    return 0;
}

// exec系统调用接口
static int kernel_execve(const char *name, unsigned char *binary, size_t size) {
    int len = strlen(name);
    int ret;
    // int 0x80;
    // eax = 4(SYS_exec)
    // edx = name
    // ecx = len
    // ebx = binary
    // edi = size
    asm volatile ("int %1;"
                  : "=a" (ret)
                  : "i" (T_SYSCALL), "0" (SYS_exec), "d" (name), "c" (len), "b" (binary), "D" (size)
                  : "memory");
    return ret;
}

#define __KERNEL_EXECVE(name, binary, size) ({                  \
            printk("kernel_execve: pid = %d, name = \"%s\".\n", \
                   current->pid, name); \
            kernel_execve(name, binary, (size_t)(size)); \
})

#define KERNEL_EXECVE(x) ({ \
            extern unsigned char _binary_obj_user_##x##_start[],   \
                _binary_obj_user_##x##_size[];     \
            __KERNEL_EXECVE(#x, _binary_obj_user_##x##_start,  \
                            _binary_obj_user_##x##_size);     \
        })

#define __KERNEL_EXECVE2(x, xstart, xsize)  ({ \
            extern unsigned char xstart[], xsize;   \
            __KERNEL_EXECVE(#x, xstart, (size_t)xsize);\
    })

#define KERNEL_EXECVE2(x, xstart, xsize)    __KERNEL_EXECVE2(x, xstart, xsize)

static int init_main(void *arg) {
    printk("this init_process, pid = %d, name = \"%s\"\n", current->pid, get_process_name(current));
    printk("To U: \"%s\".\n", (char *)arg);
    printk("To U: \"en.., Bye, Bye. :)\"\n");
#ifdef TEST
    KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE);
#else
    KERNEL_EXECVE(hello);
#endif
    panic("kernel_execve failed.\n");
    return 0;
}

void process_init(void) {
    int i;
    list_init(&process_list);
    for (i = 0; i < HASH_LIST_SIZE; i++) {
        list_init(hash_list + i);
    }

    if ((idle_process = alloc_process()) == NULL) {
        panic("can't alloc idle process\n");
    }

    idle_process->pid = 0;
    idle_process->state = STATE_RUNNABLE;
    idle_process->kstack = (uintptr_t)boot_stack;
    idle_process->need_resched = 1;
    set_process_name(idle_process, "idle");

    nr_process++;

    current = idle_process;

    int pid = kernel_thread(init_main, "Hello world!", 0);

    if (pid <= 0) {
        panic("create init_main failed.\n");
    }

    init_process = find_process(pid);
    set_process_name(init_process, "init");

    assert(idle_process != NULL && idle_process->pid == 0);
    assert(init_process != NULL && init_process->pid == 1);
}

void cpu_idle(void) {
    while (1) {
        if (current->need_resched) {
            schedule();
        }
    }
}

