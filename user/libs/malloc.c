#include <malloc.h>
#include <ulib.h>
#include <syscall.h>
#include <types.h>
#include <lock.h>
#include <unistd.h>

void lock_fork(void);
void unlock_fork(void);

typedef union header {
    struct {
        union header *ptr;
        size_t size;
        uint32_t type;  // 0: normal, sys_brk; 1: shared memory, shmem
    } s;
    uint32_t align[16];
} Header;

static Header base;
static Header *freep = NULL;

static lock_t mem_lock = INIT_LOCK;

static void free_locked(void *ap);

static inline void lock_malloc(void) {
    // 在进行内存申请的时候，不准fork子进程
    lock_fork();
    lock(&mem_lock);
}

static inline void unlock_malloc(void) {
    unlock(&mem_lock);
    unlock_fork();
}

static bool more_core_brk_locked(size_t num_units) {
    static uintptr_t brk = 0;
    if (brk == 0) {
        if (sys_brk(&brk) != 0 || brk == 0) {
            return false;
        }
    }
    uintptr_t new_brk = brk + num_units * sizeof(Header);
    if (sys_brk(&new_brk) != 0 || new_brk <= brk) {
        return false;
    }
    Header *p = (Header *)brk;
    p->s.size = (new_brk - brk) / sizeof(Header);
    p->s.type = 0;
    free_locked((void *)(p + 1));
    brk = new_brk;
    return true;
}

static bool more_core_shmem_locked(size_t num_units) {
    size_t size = ((num_units * sizeof(Header) + 0xfff) & (~0xfff));
    uintptr_t mem = 0;
    if (sys_shmem(&mem, size, MMAP_WRITE) != 0 || mem == 0) {
        return false;
    }
    Header *p = (void *)mem;
    p->s.size = size / sizeof(Header);
    p->s.type = 1;
    free_locked((void *)(p + 1));
    return true;
}

static void *malloc_locked(size_t size, uint32_t type) {
    static_assert(sizeof(Header) == 0x40);

    Header *p = NULL;
    Header *prevp = NULL;
    size_t num_units;
    if (type) {
        // 保证type为0和1
        type = 1;
    }

    // size需要多少个Header大小的块，再加上一个Header头的大小，即总共需要申请多少个Header
    num_units =((size + sizeof(Header) - 1) / sizeof(Header)) + 1;
    if ((prevp = freep) == NULL) {
        // 当前没有可用内存
        base.s.ptr = freep = prevp = &base;
        base.s.size = 0;
    }

    for (p = prevp->s.ptr; ; prevp = p, p = p->s.ptr) {
        if (p->s.type == type && p->s.size >= num_units) {
            if (p->s.size == num_units) {
                prevp->s.ptr = p->s.ptr;
            } else {
                Header *np = prevp->s.ptr = (p + num_units);
                np->s.ptr = p->s.ptr;
                np->s.size = p->s.size - num_units;
                np->s.type = type;
                p->s.size = num_units;
            }
            freep = prevp;
            return (void *)(p + 1);
        }
        // 整个链表都查询了一遍没有找到合适的大小
        if (p == freep) {
            bool (*more_core_locked)(size_t num_units);
            more_core_locked = (!type) ? more_core_brk_locked : more_core_shmem_locked;
            if (!more_core_locked(num_units)) {
                return NULL;
            }
        }
    }
}

static void free_locked(void *ptr) {
    Header *bp = ((Header *)ptr) - 1, *p;
    for (p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr) {
        if (p >= p->s.ptr && (bp > p || bp < p->s.ptr)) {
            break;
        }
    }
    if (bp->s.type == p->s.ptr->s.type && bp + bp->s.size == p->s.ptr) {
        bp->s.size += p->s.ptr->s.size;
        bp->s.ptr = p->s.ptr->s.ptr;
    } else {
        bp->s.ptr = p->s.ptr;
    } 

    if (p->s.type == bp->s.type && p + p->s.size == bp) {
        p->s.size += bp->s.size;
        p->s.ptr = bp->s.ptr;
    } else {
        p->s.ptr = bp;
    } 
    
    freep = p;
}


void *malloc(size_t size) {
    void *ret = NULL;
    lock_malloc();
    ret = malloc_locked(size, 0);
    unlock_malloc();
    return ret;
}

void *shmem_malloc(size_t size) {
    void *ret = NULL;
    lock_malloc();
    ret = malloc_locked(size, 1);
    unlock_malloc();
    return ret;
}

void free(void *ptr) {
    lock_malloc();
    free_locked(ptr);
    unlock_malloc();
}