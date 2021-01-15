#ifndef __KERNEL_MM_SHMEM_H__
#define __KERNEL_MM_SHMEM_H__

#include <types.h>
#include <list.h>
#include <memlayout.h>
// #include <sync.h>
#include <semaphore.h>

typedef struct {
    uintptr_t start;
    uintptr_t end;
    pte_t *entry;
    ListEntry link;
} ShareMemNode;

#define SHMN_N_ENTRY    (PAGE_SIZE / sizeof(pte_t))

#define le2shmn(entry, member)  \
    container_of(entry, ShareMemNode, member)

typedef struct shmem_struct {
    ListEntry shmn_link;
    ShareMemNode *shmn_cache;
    size_t len;
    atomic_t ref;   // 该结构的引用计数
    Semaphore sem; 
} ShareMemory;

ShareMemory *shmem_create(size_t len);
void shmem_destory(ShareMemory *shmem);
pte_t *shmem_get_entry(ShareMemory *shmem, uintptr_t addr, bool create);
int shmem_insert_entry(ShareMemory *shmem, uintptr_t addr, pte_t entry);
int shmem_remove_entry(ShareMemory *shmem, uintptr_t addr);

static inline int shmem_ref(ShareMemory *shmem) {
    return atomic_read(&(shmem->ref));
}

static inline void shmem_ref_set(ShareMemory *shmem, int val) {
    atomic_set(&(shmem->ref), val);
}

static inline int shmem_ref_inc(ShareMemory *shmem) {
    return atomic_add_return(&(shmem->ref), 1);
}

static inline int shmem_ref_dec(ShareMemory *shmem) {
    return atomic_sub_return(&(shmem->ref), 1);
}

static inline void shmem_lock(ShareMemory *shmem) {
    down(&(shmem->sem));
}

static inline void shmem_unlock(ShareMemory *shmem) {
    up(&(shmem->sem));
}

#endif // __KERNEL_MM_SHMEM_H__