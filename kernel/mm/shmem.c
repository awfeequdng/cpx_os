#include <shmem.h>
#include <slab.h>
#include <assert.h>
#include <mmu.h>
#include <pmm.h>
#include <string.h>
#include <swap.h>
#include <error.h>

ShareMemory *shmem_create(size_t len) {
    ShareMemory *sh_mem = kmalloc(sizeof(ShareMemory));
    if (sh_mem != NULL) {
        list_init(&(sh_mem->shmn_link));
        sh_mem->shmn_cache = NULL;
        sh_mem->len = len;
        shmem_ref_set(sh_mem, 0);
        lock_init(&(sh_mem->lock));
    }
    return sh_mem;
}

static ShareMemNode *shmn_create(uintptr_t start) {
    // 共享内存中entry（1个page大小）可以存放SHMN_N_ENTRY（1024）个pte_t结构，
    // 每个pte_t映射PAGE_SIZE(4k)的物理内存，因此一个共享内存结构总共可以共享
    // PAGE_SIZE * SHMN_N_ENTRY （4M）的大小，因此映射的起始地址我们也要以4M对齐
    assert(start % (PAGE_SIZE * SHMN_N_ENTRY) == 0);
    ShareMemNode *shmn = kmalloc(sizeof(ShareMemNode));
    if (shmn != NULL) {
        struct Page *page = NULL;
        if ((page = alloc_page()) != NULL) {
            shmn->entry = (pte_t *)page2kva(page);
            shmn->start = start;
            shmn->end = start + PAGE_SIZE * SHMN_N_ENTRY;
            memset(shmn->entry, 0, PAGE_SIZE);
        } else {
            kfree(shmn);
            shmn = NULL;
        }
    }
    return shmn;
}

static inline void shmem_remove_entry_pte(pte_t *ptep) {
    assert(ptep != NULL);
    if (*ptep & PTE_P) {
        struct Page *page = pte2page(*ptep);
        if (!PageSwap(page)) {
            if (page_ref_dec(page) == 0) {
                free_page(page);
            }
        } else {
            if (*ptep & PTE_D) {
                SetPageDirty(page);
            }
            page_ref_dec(page);
        }
        *ptep = 0;
    } else if (*ptep != 0) {
        swap_remove_entry(*ptep);
        *ptep = 0;
    }
}

static void shmn_destory(ShareMemNode *shmn) {
    int i;
    for (i = 0; i < SHMN_N_ENTRY; i++) {
        shmem_remove_entry_pte(shmn->entry + i);
    }
    free_page(kva2page(shmn->entry));
    kfree(shmn);
}

static ShareMemNode *shmn_find(ShareMemory *sh_mem, uintptr_t addr) {
    ShareMemNode *shmn = sh_mem->shmn_cache;
    // 当前地址在shmn中没有找到
    if (!(shmn != NULL && shmn->start <= addr && addr < shmn->end)) {
        shmn = NULL;
        list_entry_t *head = &(sh_mem->shmn_link);
        list_entry_t *entry = head;
        while ((entry = list_next(entry)) != head) {
            ShareMemNode *tmp = le2shmn(entry, link);
            if (tmp->start <= addr && addr < tmp->end) {
                shmn = tmp;
                break;
            }
        }
    }
    if (shmn != NULL) {
        sh_mem->shmn_cache = shmn;
    }
    return shmn;
}

static inline void shmn_check_overlap(ShareMemNode *prev, ShareMemNode *next) {
    assert(prev->start < prev->end);
    assert(prev->end <= next->start);
    assert(next->start < next->end);
}

static void shmn_insert(ShareMemory *sh_mem, ShareMemNode *shmn) {
    list_entry_t *head = &(sh_mem->shmn_link);
    list_entry_t *entry = head;
    list_entry_t *entry_prev = head;
    list_entry_t *entry_next = NULL;
    while ((entry = list_next(entry)) != head) {
        ShareMemNode *shmn_prev = le2shmn(entry, link);
        if (shmn_prev->start > shmn->start) {
            break;
        }
        entry_prev = entry;
    }
    entry_next = list_next(entry_prev);

    if (entry_prev != head) {
        shmn_check_overlap(le2shmn(entry_prev, link), shmn);
    }
    if (entry_next != head) {
        shmn_check_overlap(shmn, le2shmn(entry_next, link));
    }

    list_add_after(entry_prev, &(shmn->link));
}

void shmem_destory(ShareMemory *sh_mem) {
    list_entry_t *head = &(sh_mem->shmn_link);
    list_entry_t *entry = NULL;
    while ((entry = list_next(head)) != head) {
        list_del(entry);
        shmn_destory(le2shmn(entry, link));
    }
    kfree(sh_mem);
}

pte_t *shmem_get_entry(ShareMemory *sh_mem, uintptr_t addr, bool create) {
    assert(addr < sh_mem->len);
    addr = ROUNDDOWN(addr, PAGE_SIZE);
    ShareMemNode *shmn = shmn_find(sh_mem, addr);

    assert(shmn == NULL || (shmn->start <= addr && addr < shmn->end));
    if (shmn == NULL) {
        uintptr_t start = ROUNDDOWN(addr, PAGE_SIZE * SHMN_N_ENTRY);
        if (!create || (shmn = shmn_create(start)) == NULL) {
            return NULL;
        }
        shmn_insert(sh_mem, shmn);
    }
    int index = (addr - shmn->start) / PAGE_SIZE;
    if (shmn->entry[index] == 0) {
        if (create) {
            struct Page *page = alloc_page();
            if (page != NULL) {
                shmn->entry[index] = (page2pa(page) | PTE_P);
                page_ref_inc(page);
            }
        }
    }
    return shmn->entry + index;
}

int shmem_insert_entry(ShareMemory *sh_mem, uintptr_t addr, pte_t entry) {
    pte_t *ptep = shmem_get_entry(sh_mem, addr, 1);
    if (ptep == NULL) {
        return -E_NO_MEM;
    }
    if (*ptep != 0) {
        // 将addr之前的pte项清除(因为要用新的pte代替旧的pte项)
        shmem_remove_entry_pte(ptep);
    }
    if (entry & PTE_P) {
        // pte映射的page存在，则将page的引用计数加1
        page_ref_inc(pte2page(entry));
    } else if (entry != 0) {
        // pte指向swap entry,则将swap entry的引用计数加1
        swap_duplicate(entry);
    }
    *ptep = entry;
    return 0;
}

int shmem_remove_entry(ShareMemory *sh_mem, uintptr_t addr) {
    // 在地址addr处插入一个0项，也就相当于删除了这个地址的pte
    return shmem_insert_entry(sh_mem, addr, 0);
}