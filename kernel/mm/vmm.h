#ifndef __KERNEL_MM_VMM_H__
#define __KERNEL_MM_VMM_H__

#include <types.h>
#include <list.h>
#include <memlayout.h>
#include <sync.h>
#include <rbtree.h>
#include <shmem.h>
#include <atomic.h>

struct mm_struct;

typedef struct {
    struct mm_struct *vm_mm;
    // [start, end)
    uintptr_t vm_start;
    uintptr_t vm_end;
    uint32_t vm_flags;
    rbtree_node_t rb_link;
    list_entry_t vma_link;
    ShareMemory *shmem;
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
    list_entry_t mmap_link;
    rbtree_t mmap_tree;        // 紅黑樹，用於鏈接VmmStruct，紅黑樹按照vmm的start 地址排序
    VmaStruct *mmap_cache;
    pde_t *page_dir;
    int map_count;
    uintptr_t swap_address;
    atomic_t mm_count;
    lock_t mm_lock;
} MmStruct;

// 當節點數量大於32時，採用紅黑樹將vma鏈接起來
#define RB_MIN_MAP_COUNT 32

VmaStruct *find_vma(MmStruct *mm, uintptr_t addr);
VmaStruct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags);
void insert_vma_struct(MmStruct *mm, VmaStruct *vma);

MmStruct *mm_create(void);
void mm_destory(MmStruct *mm);

int mm_map(MmStruct *mm, uintptr_t addr, size_t len, uint32_t vm_flags,
         VmaStruct **vma_store);
int mm_unmap(MmStruct *mm, uintptr_t addr, size_t len);

int dup_mmap(MmStruct *to, MmStruct *from);

void exit_mmap(MmStruct *mm);

void vmm_init(void);

int do_page_fault(MmStruct *m, uint32_t error_code, uintptr_t addr);

bool user_mem_check(MmStruct *mm, uintptr_t start, size_t len, bool write);

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

static inline void lock_mm(MmStruct *mm) {
    if (mm != NULL) {
        lock(&(mm->mm_lock));
    } 
}

static inline void unlock_mm(MmStruct *mm) {
    if (mm != NULL) {
        unlock(&(mm->mm_lock));
    }
}

void print_vma(void);

#endif // __KERNEL_MM_VMM_H__