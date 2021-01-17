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
#include <shmem.h>

// 除了idle_process，其他所有进程都挂接在该链表下面
ListEntry process_list;
// 需要通过kswapd管理内存的进程（页可以被置换出去的进程）挂接在该链表下
ListEntry process_mm_list;
#define HASH_SHIFT          10
#define HASH_LIST_SIZE      (1 << HASH_SHIFT)
#define pid_hashfn(x)       (hash32(x, HASH_SHIFT))

static ListEntry hash_list[HASH_LIST_SIZE];

Process *idle_process = NULL;

Process *init_process = NULL;

Process *current = NULL;

Process *kswapd = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct TrapFrame *tf);
void switch_to(Context *from, Context *to);


static void put_page_dir(MmStruct *mm) {
    free_page(kva2page(mm->page_dir));
}

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
        process->page_dir = (uintptr_t)get_boot_page_dir();
        process->flags = 0;
        memset(process->name, 0, PROCESS_NAME_LEN);
        process->wait_state = 0;
        process->child = NULL;
        process->left_sibling = NULL;
        process->right_sibling = NULL;
        list_init(&(process->thread_group));
        process->rq = NULL;
        list_init(&(process->run_link));
        process->time_slice = 0;
        process->sem_queue = NULL;
        process->fs_struct = NULL;
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
    ListEntry *head = &process_list;
    ListEntry *entry = NULL;
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

static void unhash_process(Process *process) {
    // 从hash表上将process摘下来
    list_del(&(process->hash_link));
}

Process *find_process(int pid) {
    if (0 < pid && pid < MAX_PID) {
        ListEntry *head = hash_list + pid_hashfn(pid);
        ListEntry *entry = head;
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

// 将process这个线程从线程组中删除
static void delete_thread(Process *process) {
    if (!list_empty(&process->thread_group)) {
        bool flag;
        local_intr_save(flag);
        {
            list_del_init(&(process->thread_group));
        }
        local_intr_restore(flag);
    }
}

static Process *next_thread(Process *process) {
    return le2process(list_next(&(process->thread_group)), thread_group);
}

static int copy_sem(uint32_t clone_flags, Process *process) {
    SemaphoreQueue *sem_queue = NULL;
    SemaphoreQueue *old_sem_queue = current->sem_queue;

    if (old_sem_queue == NULL) {
        // 内核线程没有信号量队列
        return 0;
    }

    if (clone_flags & CLONE_SEM) {
        sem_queue = old_sem_queue;
        goto good_sem_queue;
    }

    int ret = -E_NO_MEM;
    if ((sem_queue = sem_queue_create()) == NULL) {
        goto bad_sem_queue;
    }

    down(&(old_sem_queue->sem));
    ret = dup_sem_queue(sem_queue, old_sem_queue);
    up(&(old_sem_queue->sem));
    
    if (ret != 0) {
        goto bad_dup_cleanup_sem;
    }

good_sem_queue:
    sem_queue_ref_inc(sem_queue);
    process->sem_queue = sem_queue;
    return 0;

bad_dup_cleanup_sem:
    exit_sem_queue(sem_queue);
    sem_queue_destory(sem_queue);
bad_sem_queue:
    return ret;
}

static void put_sem_queue(Process *process) {
    SemaphoreQueue *sem_queue = process->sem_queue;
    if (sem_queue != NULL) {
        if (sem_queue_ref_dec(sem_queue) == 0) {
            exit_sem_queue(sem_queue);
            sem_queue_destory(sem_queue);
            process->sem_queue = NULL;
        }
    }
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
    if (mm != old_mm) {
        mm->brk_start = old_mm->brk_start;
        mm->brk = old_mm->brk;
        // 进程创建了MmStruct结构用于管理内存，那么将这个进程挂接到process_mm_list下面进行页内存管理
        bool flag;
        local_intr_save(flag);
        {
            list_add(&process_mm_list, &(mm->process_mm_link));
        }
        local_intr_restore(flag);
    }
    mm_count_inc(mm);
    process->mm = mm;
    process->page_dir = (uintptr_t)mm->page_dir;
    return 0;
bad_dup_cleanup_mmap:
    exit_mmap(mm);
    put_page_dir(mm);
bad_page_dir_cleanup_mm:
    mm_destory(mm);
bad_mm:
    return ret;
}

static void copy_thread(Process *process, uintptr_t esp, struct TrapFrame *tf) {
    process->tf = (struct TrapFrame *)(process->kstack + K_STACK_SIZE) - 1;
    *(process->tf) = *tf;
    // fork子进程的返回值为0
    process->tf->tf_regs.reg_eax = 0;
    // 父进程和子进程在刚fork时的栈结构是一样的
    process->tf->tf_esp = esp;
    process->tf->tf_eflags |= FL_IF;

    process->context.eip = (uintptr_t)forkret;
    process->context.esp = (uintptr_t)(process->tf);
}

static void set_links(Process *process) {
    list_add(&process_list, &(process->process_link));
    process->left_sibling = NULL;
    if ((process->right_sibling = process->parent->child) != NULL) {
        process->right_sibling->left_sibling = process;
    }
    process->parent->child = process;
    nr_process++;
}

// 将process的父子关系、兄弟关系的链表删除掉
static void remove_links(Process *process) {
    list_del(&(process->process_link));
    // process的右孩子的左节点指向process的左孩子
    if (process->right_sibling != NULL) {
        process->right_sibling->left_sibling = process->left_sibling;
    }
    // 如果process存在左节点，则将左节点的右节点指向process的右节点
    if (process->left_sibling != NULL) {
        process->left_sibling->right_sibling = process->right_sibling;
    } else {
        // 如果process不存在左节点，则process的父节点的孩子就是process，将process父节点的孩子指向process的右节点
        process->parent->child = process->right_sibling;
    }
    nr_process--;
}

// 父进程创建子进程
// 1、调用alloc_process分配一个进程结构
// 2、调用setup_kstack来为新进程分配一个新的子进程内核堆栈
// 3、调用copy_mm来复制或者共享父进程的内存地址空间（根据clone_flags标志选择复制还是共享）
// 4、调用wakeup_process来唤醒子进程
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
    assert(current->wait_state == 0);

    assert(current->time_slice >= 0);
    // 当前进程分二分之一的时间片给子进程
    process->time_slice = current->time_slice >> 1;
    current->time_slice -= process->time_slice;

    if (setup_kstack(process) != 0) {
        goto bad_fork_cleanup_process;
    }
    if (copy_sem(clone_flags, process) != 0) {
        goto bad_fork_cleanup_kstack;
    }
    if (copy_mm(clone_flags, process) != 0) {
        goto bad_fork_cleanup_sem;
    }
    copy_thread(process, stack, tf);

    bool flag;
    local_intr_save(flag);
    {
        process->pid = get_pid();
        hash_process(process);
        set_links(process);
        // 创建线程
        if (clone_flags & CLONE_THREAD) {
            list_add_before(&(current->thread_group), &(process->thread_group));
        }
    }
    local_intr_restore(flag);

    wakeup_process(process);

    ret = process->pid;

fork_out:
    return ret;
bad_fork_cleanup_sem:
    put_sem_queue(process);
bad_fork_cleanup_kstack:
    put_kstack(process);
bad_fork_cleanup_process:
    kfree(process);
    goto fork_out;
}

static int __do_kill(Process *process, int error_code) {
    if (!(process->flags & PF_EXITING)) {
        process->flags |= PF_EXITING;
        process->exit_code = error_code;
        if (process->wait_state & WT_INTERRUPTED) {
            wakeup_process(process);
        }
        return 0;
    }
    return -E_KILLED;
}

// __do_exit: 线程退出时调用该函数，主线程退出时，
// 1、调用exit_mmap、put_pgdir以及mm_destory释放进程的所有内存空间
// 2、设置进程的状态为ZOMBIE，然后唤醒父进程来回收自己；
// 3、最后调用scheduler调度器切换进程（从此不会再执行到自己了）
int __do_exit() {
    if (current == idle_process) {
        panic("idle_process exit.\n");
    }
    if (current == init_process) {
        panic("init_process exit. pid = %d, error_code = %d\n", current->pid, current->exit_code);
    }

    MmStruct *mm = current->mm;
    if (mm != NULL) {
        lcr3(PADDR(get_boot_page_dir()));
        if (mm_count_dec(mm) == 0) {
            // 删除页表和已经映射的page
            exit_mmap(mm);
            put_page_dir(mm);
            bool flag;
            local_intr_save(flag);
            {
                list_del(&(mm->process_mm_link));
            }
            local_intr_restore(flag);
            mm_destory(mm);
        }
        current->mm = NULL;
    }
    // 释放信号量
    put_sem_queue(current);
    current->state = STATE_ZOMBIE;

    bool flag;
    Process *parent = NULL;
    Process *process = NULL;
    local_intr_save(flag);
    {
        process = parent = current->parent;
        // 如果父进程所在线程组有处于WT_CHILD等待状态的进程，将其唤醒
        do {
            if (process->wait_state == WT_CHILD) {
                wakeup_process(process);
            }
            process = next_thread(process);
        } while (process != parent);

        // 如果current在一个线程组中，则将current的下一个线程作为current的孩子的父亲节点
        // todo: 如果我此时current是线程组的leader时，那么得到的parent就是current的孩子了，那么下面的将current的孩子挂在current的孩子上岂不是会出问题？？
        if ((parent = next_thread(current)) == current) {
            // 如果当前current没有和其他线程组成线程组，那么把current的孩子挂接到init_process进程
            parent = init_process;
        }
        // 将current从线程组中删除（如果存在线程组的话）
        delete_thread(current);
        // 将current的孩子挂接到init_process节点下（为init_process的孩子）
        while (current->child != NULL) {
            process = current->child;
            current->child = process->right_sibling;
            process->left_sibling = NULL;

            if ((process->right_sibling = parent->child) != NULL) {
                parent->child->left_sibling = process;
            }
            process->parent = parent;
            parent->child = process;
            if (process->state == STATE_ZOMBIE) {
                // process已经结束，并且parent在等待子进程结束
                if (parent->wait_state == WT_CHILD) {
                    wakeup_process(parent);
                }
            }
        }
    }
    local_intr_restore(flag);

    schedule();
    panic("do_exit will not return!! %d.\n", current->pid);
}

// 杀死线程组内的所有线程（包括线程leader）
int do_exit(int error_code) {
    bool flag;
    local_intr_save(flag);
    {
        ListEntry *head = &(current->thread_group);
        ListEntry *entry = head;
        while ((entry = list_next(entry)) != head) {
            Process *process = le2process(entry, thread_group);
            // 1、把所有的子线程先杀死
            __do_kill(process, error_code);
        }
    }
    local_intr_restore(flag);
    // 2、然后自己再退出
    return do_exit_thread(error_code);
}

int do_exit_thread(int error_code) {
    current->exit_code = error_code;
    return __do_exit();
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
        // brk_start的值为从加载程序的最后的一个地址开始（以页向上对齐）
        if (mm->brk_start < ph->p_va + ph->p_memsz) {
            mm->brk_start = ph->p_va + ph->p_memsz;
        }
        unsigned char *from = binary + ph->p_offset;
        size_t off, size;
        uintptr_t start = ph->p_va;
        uintptr_t end;
        uintptr_t va = ROUNDDOWN(start, PAGE_SIZE);
        ret = -E_NO_MEM;

        end = ph->p_va + ph->p_filesz;
        printk("[start, end) = [%08x, %08x)\n", start, end);
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
    // brk == brk_start说明还没有分配任何堆内存
    mm->brk_start = mm->brk = ROUNDUP(mm->brk_start, PAGE_SIZE);

    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    // 设置堆栈的vma以及权限
    if ((ret = mm_map(mm, USER_STACK_TOP - USER_STACK_SIZE, USER_STACK_SIZE, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        list_add(&process_mm_list, &(mm->process_mm_link));
    }
    local_intr_restore(intr_flag);

    mm_count_inc(mm);
    current->mm = mm;
    current->page_dir = (uintptr_t)mm->page_dir;
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
    printk("tf_eip = 0x%08x\n", tf->tf_eip);
    ret = 0;

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

    if (len > PROCESS_NAME_LEN) {
        len = PROCESS_NAME_LEN;
    }

    char local_name[PROCESS_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));

    lock_mm(mm);
    {
        if (!copy_from_user(mm, local_name, name, len, 0)) {
            unlock_mm(mm);
            return -E_INVAL;
        }
    }
    unlock_mm(mm);
    
    
    if (mm != NULL) {
        // 切换到内核地址空间，因为进程mm要被释放掉了
        lcr3(PADDR(get_boot_page_dir()));
        if (mm_count_dec(mm) == 0) {
            // 释放所有页表项映射的page，以及释放所有的页表，最后只留下一个页目录
            exit_mmap(mm);
            // 将页目录也是释放掉
            put_page_dir(mm);
            bool intr_flag;
            local_intr_save(intr_flag);
            {
                list_del(&(mm->process_mm_link));
            }
            local_intr_restore(intr_flag);
            // 释放所有vma结构以及mm结构
            mm_destory(mm);
        }
        current->mm = NULL;
    }
    // 将当前的信号量释放掉
    put_sem_queue(current);

    int ret = -E_NO_MEM;
    // 创建一个新的信号量
    if ((current->sem_queue = sem_queue_create()) == NULL) {
        goto execve_exit;
    }
    sem_queue_ref_inc(current->sem_queue);

    // 加载新的进程地址空间到current
    if ((ret = load_icode(binary, size)) != 0) {
        goto execve_exit;
    }
    set_process_name(current, local_name);
    // 当前进程为为新的内存地址空间
    // todo: 为什么execve新进程后将其从线程组中移除
    delete_thread(current);
    printk("current pid = %d, name = %s\n", current->pid, current->name);
    return 0;

execve_exit:
    do_exit(ret);
    panic("already exit: %e.\n", ret);
}

int do_yield(void) {
    current->need_resched = 1;
    return 0;
}

// 等待进程的孩子结束，此时如果进程在线程组中，如果进程没有ZOMBIE状态的孩子，那么就查找线程组中其他线程的孩子是否存于ZOMBIE状态
int do_wait(int pid, int *code_store) {
    MmStruct *mm = current->mm;

    Process *process = NULL;
    Process *parent_process = NULL;
    bool flag;
    // 是否继续等待子进程结束
    bool wait_again;
repeat:
    parent_process = current;
    wait_again = false;
    if (pid != 0) {
        process = find_process(pid);
        // 找到pid对应的进程
        if (process != NULL) {
            do {
                if (process->parent == parent_process) {
                    wait_again = true;
                    if (process->state == STATE_ZOMBIE) {
                        goto found;
                    }
                    // 子进程不是ZOMBIE状态，跳出去继续等待
                    break;
                }
                // 在线程组中查找process的父进程
                parent_process = next_thread(parent_process);
            } while (parent_process != current);
        }
    } else {
        do {
            process = parent_process->child;
            // 当前线程组存在子孩子
            while (process != NULL) {
                wait_again = true;
                if (process->state == STATE_ZOMBIE) {
                    goto found;
                }
                process = process->right_sibling;
            }
            parent_process = next_thread(parent_process);
        } while (parent_process != current);
    }
    // 找到子进程，但是子进程还没有结束
    // 在等待子进程结束的时候，如果当前进程current被杀死，则直接退出
    if (wait_again) {
        current->state = STATE_SLEEPING;
        current->wait_state = WT_CHILD;
        schedule();
        may_killed();
        goto repeat;
    }
    // pid不等于0时，找不到对应pid对应的process；
    // 或者pid为0时，当前进程不存在子进程
    return -E_BAD_PROCESS;

found:
    if (process == idle_process || process == init_process) {
        panic("wait idle_process or init_process.\n");
    }
    
    int exit_code = process->exit_code;

    local_intr_save(flag);
    {
        unhash_process(process);
        remove_links(process);
        // 在进程执行__do_exit时，已经从线程组中将进程移除了
        // delete_thread(process);
    }
    local_intr_restore(flag);
    // ZOMBIE状态的process堆栈是没有释放的，这个堆栈可以用来调试
    put_kstack(process);
    kfree(process);

    int ret = 0;
    if (code_store != NULL) {
        lock_mm(mm);
        {
            if (!copy_to_user(mm, code_store, &exit_code, sizeof(exit_code))) {
                ret = -E_INVAL;
            }
        }
        unlock_mm(mm);
    }
    return ret;
}

// 强制杀死process，设置process的flags为PF_EXITING
int do_kill(int pid, int error_code) {
    Process *process = NULL;
    if ((process = find_process(pid)) != NULL) {
        return __do_kill(process, error_code);
    }
    return -E_INVAL;
}

int do_brk(uintptr_t *brk_store) {
    MmStruct *mm = current->mm;
    if (mm == NULL) {
        // mm为NULL表明当前为内核线程（内核线程没有mm结构）
        panic("kernel thread call sys_brk!.\n");
    }
    if (brk_store == NULL) {
        return -E_INVAL;
    }
    // 希望申请的新的brk的值
    uintptr_t brk;
    lock_mm(mm);
    if (!copy_from_user(mm, &brk, brk_store, sizeof(uintptr_t), true)) {
        unlock_mm(mm);
        return -E_INVAL;
    }

    if (brk < mm->brk_start) {
        goto out_unlock;
    }
    uintptr_t new_brk = ROUNDUP(brk, PAGE_SIZE);
    uintptr_t old_brk = mm->brk;
    assert(old_brk % PAGE_SIZE == 0);
    // 新申请的brk在之前申请的brk范围之内，并且有相同的上边界（按页对齐）
    if (new_brk == old_brk) {
        goto out_unlock;
    }

    if (new_brk < old_brk) {
        // 释放[new_brk, old_brk)这部分的vma
        if (mm_unmap(mm, new_brk, old_brk - new_brk) != 0) {
            goto out_unlock;
        }
    } else {
        // 申请内存
        // todo: 为什么上边界为new_brk + PAGE_SIZE，是为了留一个page的缓冲区吗？以保护堆栈段
        if (find_vma_intersection(mm, old_brk, new_brk + PAGE_SIZE) != NULL) {
            goto out_unlock;
        }
        if (mm_brk(mm, old_brk, new_brk - old_brk) != 0) {
            goto out_unlock;
        }
    }

    mm->brk = new_brk;

out_unlock:
out:
    *brk_store = mm->brk;
    unlock_mm(mm);
    return 0;
}

int do_sleep(unsigned int time) {
    if (time == 0) {
        return 0;
    }
    bool flag;
    Timer __timer;
    Timer *timer = NULL;
    local_intr_save(flag);
    {
        timer = timer_init(&__timer, current, time); 
        current->state = STATE_SLEEPING;
        current->wait_state = WT_TIMER;
        add_timer(timer);
    }
    local_intr_restore(flag);
    schedule();

    del_timer(timer);
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

// 内核线程：用于执行用户态程序
static int user_main(void *arg) {
#ifdef TEST
    KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE);
#else
    KERNEL_EXECVE(sem_test);
#endif
    panic("user_main execve failed.\n");
}

// init_process内核线程： 该线程用于创建kswap_main & user_main等内核线程
static int init_main(void *arg) {
    int pid;
    if ((pid = kernel_thread(kswapd_main, NULL, 0)) <= 0) {
        panic("kswapd init failed.\n");
    }
    kswapd = find_process(pid);
    printk("kswapd pid = %d\n", kswapd->pid);
    set_process_name(kswapd, "kswapd");

    size_t nr_free_pages_store = nr_free_pages();
    size_t slab_allocated_store = slab_allocated();
    unsigned int nr_process_store = nr_process;

    pid = kernel_thread(user_main, NULL, 0);
    if (pid <= 0) {
        panic("create user_main failed.\n");
    }

    // 等待init_process所有的子进程结束
    while (do_wait(0, NULL) == 0) {
        if (nr_process_store == nr_process) {
            // 除了kswapd 和 idle_process以及 init_process没有结束外，其他所有进程都结束了
            assert(nr_process == 3);
            break;
        }
        schedule();
    }

    assert(kswapd != NULL);
    int i;
    for (i = 0; i < 10; i++) {
        if (kswapd->wait_state == WT_TIMER) {
            wakeup_process(kswapd);
        }
        schedule();
    }

    printk("all user-mode processes have quit.\n");
    assert(init_process->child == kswapd);
    assert(init_process->left_sibling == NULL);
    assert(init_process->right_sibling == NULL);
    // 只有两个进程了，一个idle_process，另一个为init_process
    assert(nr_process == 3);
    assert(kswapd->child == NULL);
    assert(kswapd->left_sibling == NULL);
    assert(kswapd->right_sibling == NULL);
    assert(nr_free_pages_store == nr_free_pages());
    assert(slab_allocated_store == slab_allocated());

    printk("init check memory pass.\n");
    return 0;
}

void process_init(void) {
    int i;
    list_init(&process_list);
    list_init(&process_mm_list);
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

int do_mmap(uintptr_t *addr_store, size_t len, uint32_t mmap_flags) {
    MmStruct *mm = current->mm;
    if (mm == NULL) {
        panic("kernel thread call do_mmap!\n");
    }
    if (addr_store == NULL || len == 0) {
        return -E_INVAL;
    }

    int ret = -E_INVAL;

    uintptr_t addr;

    lock_mm(mm);
    if (!copy_from_user(mm, &addr, addr_store, sizeof(uintptr_t), true)) {
        goto out_unlock;
    }
    
    uintptr_t start = ROUNDDOWN(addr, PAGE_SIZE);
    uintptr_t end = ROUNDUP(addr + len, PAGE_SIZE);
    addr = start;
    len = end - start;

    uint32_t vm_flags = VM_READ;
    if (mmap_flags & MMAP_WRITE) vm_flags |= VM_WRITE;
    if (mmap_flags & MMAP_STACK) vm_flags |= VM_STACK;

    ret = -E_NO_MEM;
    if (addr == 0) {
        if ((addr = get_unmapped_area(mm, len)) == 0) {
            goto out_unlock;
        }
    }
    if ((ret = mm_map(mm, addr, len, vm_flags, NULL)) == 0) {
        *addr_store = addr;
    }
out_unlock:
    unlock_mm(mm);
    return ret;
}

int do_munmap(uintptr_t addr, size_t len) {
    MmStruct *mm = current->mm;
    if (mm == NULL) {
        panic("kernel thread call do_munmap!.\n");
    }
    if (len == 0) {
        return -E_INVAL;
    }
    int ret;
    lock_mm(mm);
    {
        ret = mm_unmap(mm, addr, len);
    }
    unlock_mm(mm);
    return ret;
}

// 创建共享内存
int do_shmem(uintptr_t *addr_store, size_t len, uint32_t mmap_flags) {
    MmStruct *mm = current->mm;
    if (mm == NULL) {
        panic("kernel thread call do_shmem.\n");
    }
    if (addr_store == NULL || len == 0) {
        return -E_INVAL;
    }

    int ret = -E_INVAL;

    uintptr_t addr;

    lock_mm(mm);
    if (!copy_from_user(mm, &addr, addr_store, sizeof(uintptr_t), true)) {
        goto out_unlock;
    }

    uintptr_t start = ROUNDDOWN(addr, PAGE_SIZE);
    uintptr_t end = ROUNDUP(addr + len, PAGE_SIZE);
    addr = start;
    len = end - start;
    uint32_t vm_flags = VM_READ;
    if (mmap_flags & MMAP_WRITE) vm_flags |= VM_WRITE;
    if (mmap_flags & MMAP_STACK) vm_flags |= VM_STACK;

    ret = -E_NO_MEM;
    if (addr == 0) {
        if ((addr = get_unmapped_area(mm, len)) == 0) {
            goto out_unlock;
        }
    }
    ShareMemory *shmem = NULL;
    if ((shmem = shmem_create(len)) == NULL) {
        goto out_unlock;
    }

    if ((ret = mm_map_shmem(mm, addr, vm_flags, shmem, NULL)) != 0) {
        assert(shmem_ref(shmem) == 0);
        shmem_destory(shmem);
        goto out_unlock;
    }

    *addr_store = addr;
out_unlock:
    unlock_mm(mm);
    return ret;
}

void may_killed(void) {
    if (current->flags & PF_EXITING) {
        __do_exit();
    }
}

static void inline print_process(Process *process, int level) {
    int i;
    for (i = 0; i < level; i++) {
        printk("\t");
    }
    int ppid;
    Process *parent = process->parent;
    if (parent != NULL) {
        ppid = parent->pid;
    }
    printk("pid: %d, s: %d, ppid = %d\n", process->pid, process->state, ppid);
}

static void process_tree(Process *process, int level) {
    while (process != NULL) {
        print_process(process, level);
        process_tree(process->child, level + 1);
        process = process->right_sibling;
    }
}

void print_process_tree(void) {
    if (init_process == NULL) {
        printk("init_process does not exist\n");
        return;
    }
    process_tree(init_process, 1);
}

void print_current_thread_group(void) {
    if (current == NULL) {
        return;
    }
    Process *process = current;
    do {
        printk("  pid:%d  ", process->pid);
        process = next_thread(process);
    } while (process != current);
    printk("\n");
}

void cpu_idle(void) {
    while (1) {
        if (current->need_resched) {
            schedule();
        }
    }
}

