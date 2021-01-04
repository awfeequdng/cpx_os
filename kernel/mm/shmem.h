#ifndef __KERNEL_MM_SHMEM_H__
#define __KERNEL_MM_SHMEM_H__

#include <types.h>
#include <list.h>
#include <memlayout.h>
#include <sync.h>

typedef struct shmn_s {
    uintptr_t start;
    uintptr_t end;
    pte_t *entry;
    list_entry_t link;
} Shmn;

#define SHMN_N_ENTRY    (PAGE_SIZE / sizeof(pte_t))

#define le2shmn(entry, member)  \
    container_of(entry, struct shmn_s, member)

typedef struct shmem_struct {
    list_entry_t shmn_link;
    Shmn *shmn_cache;
    size_t len;
    atomic_t ref;   // 该结构的引用计数
    lock_t lock;    
} ShareMemory;

ShareMemory *shmem_create(size_t len);
void shmem_destory(ShareMemory *sh_mem);
pte_t *shmem_get_entry(ShareMemory *sh_mem, uintptr_t addr, bool create);
int shmem_insert_entry(ShareMemory *sh_mem, uintptr_t addr, pte_t entry);
int shmem_remove_entry(ShareMemory *sh_mem, uintptr_t addr);

static inline int shmem_ref(ShareMemory *sh_mem) {
    return atomic_read(&(sh_mem->ref));
}

static inline void shmem_ref_set(ShareMemory *sh_mem, int val) {
    atomic_set(&(sh_mem->ref), val);
}

static inline int shmem_ref_inc(ShareMemory *sh_mem) {
    return atomic_add_return(&(sh_mem->ref), 1);
}

static inline void shmem_lock(ShareMemory *sh_mem) {
    lock(&(sh_mem->lock));
}

static inline void shmem_unlock(ShareMemory *sh_mem) {
    unlock(&(sh_mem->lock));
}

#endif // __KERNEL_MM_SHMEM_H__