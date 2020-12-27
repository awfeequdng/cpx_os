#ifndef __KERNEL_MM_PMM_H__
#define __KERNEL_MM_PMM_H__

#include <atomic.h>
#include <list.h>
#include <memlayout.h>

struct PmmManager {
    const char *name;
    void (*init)(void);

    void (*init_memmap)(struct Page *base, size_t n);

    struct Page *(*alloc_pages)(size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};

void pmm_init(void);

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);


#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)

pte_t *get_pte(pde_t *pgdir, uintptr_t vaddr, bool create);
// ptep返回vaddr对应的页表项
struct Page *get_page(pde_t *pgdir, uintptr_t vaddr, pte_t **ptep);
void page_remove(pde_t *pgdir, uintptr_t vaddr);
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t vaddr, uint32_t perm);

void tlb_invalidate(pde_t *pgdir, uintptr_t vaddr);

void print_pgdir(void);

// 通过物理地址，返回内核虚拟地址
// #define KVADDR(pa) ({               \
//             uintptr_t __pa = (pa);   \
//             size_t __ppn = PPN(__pa);
//             })

static inline int page_ref(struct Page *page) {
    return atomic_read(&(page->ref));
}

static inline void set_page_ref(struct Page *page, int ref) {
    atomic_set(&(page->ref), ref);
}

static inline int page_ref_inc(struct Page *page) {
    return atomic_add_return(&(page->ref), 1);
}

static inline int page_reg_dec(struct Page *page) {
    return atomic_sub_return(&(page->ref), 1);
}

#endif // __KERNEL_MM_PMM_H__
