#ifndef __KERNEL_MM_VMM_H__
#define __KERNEL_MM_VMM_H__

#include <types.h>
#include <list.h>
#include <memlayout.h>
#include <sync.h>
#include <rbtree.h>
// #include <shmem.h>
#include <atomic.h>
#include <semaphore.h>

struct mm_struct;

struct shmem_struct;

typedef struct {
    struct mm_struct *vm_mm;
    // [start, end)
    uintptr_t vm_start;
    uintptr_t vm_end;
    uint32_t vm_flags;
    rbtree_node_t rb_link;
    ListEntry vma_link;
    struct shmem_struct *shmem;
    size_t shmem_off;
} VmaStruct;

#define le2vma(le, member)  \
    container_of(le, VmaStruct, member)

#define rbn2vma(node, member)   \
    container_of(node, VmaStruct, member)

#define VM_READ         0x00000001
#define VM_WRITE        0x00000002
#define VM_EXEC         0x00000004
#define VM_STACK        0x00000008
#define VM_SHARE        0x00000010

typedef struct mm_struct {
    ListEntry mmap_link;
    rbtree_t mmap_tree;        // 紅黑樹，用於鏈接VmmStruct，紅黑樹按照vmm的start 地址排序
    VmaStruct *mmap_cache;
    pde_t *page_dir;
    int map_count;
    uintptr_t swap_address;
    atomic_t mm_count;
    lock_t mm_lock;
    int locked_by;      // mm是被哪个进程锁住的
    uintptr_t brk_start;
    uintptr_t brk;
    // 进程挂接在kswapd管理的链表上
    ListEntry process_mm_link;
    Semaphore mm_sem;
} MmStruct;

#define le2mm(le, member)   \
    container_of(le, MmStruct, member)

// 當節點數量大於32時，採用紅黑樹將vma鏈接起來
#define RB_MIN_MAP_COUNT 32

VmaStruct *find_vma(MmStruct *mm, uintptr_t addr);
VmaStruct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags);
void insert_vma_struct(MmStruct *mm, VmaStruct *vma);
VmaStruct *find_vma_intersection(MmStruct *mm, uintptr_t start, uintptr_t end);
int mm_brk(MmStruct *mm, uintptr_t addr, size_t len);
MmStruct *mm_create(void);
void mm_destory(MmStruct *mm);
int mm_map_shmem(MmStruct *mm, uintptr_t addr, uint32_t vm_flags,
        struct shmem_struct *shmem, VmaStruct **vma_store);

int mm_map(MmStruct *mm, uintptr_t addr, size_t len, uint32_t vm_flags,
         VmaStruct **vma_store);
int mm_unmap(MmStruct *mm, uintptr_t addr, size_t len);

int dup_mmap(MmStruct *to, MmStruct *from);

void exit_mmap(MmStruct *mm);

void vmm_init(void);

int do_page_fault(MmStruct *m, uint32_t error_code, uintptr_t addr);

bool user_mem_check(MmStruct *mm, uintptr_t start, size_t len, bool write);

uintptr_t get_unmapped_area(MmStruct *mm, size_t len);

static inline int mm_count(MmStruct *mm) {
    return atomic_read(&(mm->mm_count));
}

static inline void set_mm_count(MmStruct *mm, int val) {
    atomic_set(&(mm->mm_count), val);
}

static inline int mm_count_inc(MmStruct *mm) {
    return atomic_add_return(&(mm->mm_count), 1);
}

static inline int mm_count_dec(MmStruct *mm) {
    return atomic_sub_return(&(mm->mm_count), 1);
}

void lock_mm(MmStruct *mm);
void unlock_mm(MmStruct *mm);
bool try_lock_mm(MmStruct *mm);

bool copy_from_user(MmStruct *mm, void *dst, const void *src, size_t len, bool writable);
bool copy_to_user(MmStruct *mm, void *dst, const void *src, size_t len);
bool copy_string(MmStruct *mm, char *dst, const char *src, size_t max_len);

void print_vma(void);

#endif // __KERNEL_MM_VMM_H__