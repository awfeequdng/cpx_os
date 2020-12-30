#ifndef __KERNEL_MM_PMM_H__
#define __KERNEL_MM_PMM_H__

#include <atomic.h>
#include <list.h>
#include <memlayout.h>
#include <assert.h>

struct PmmManager {
    char *name;
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

pte_t *get_pte(pde_t *pgdir, uintptr_t va, bool create);
// ptep返回vaddr对应的页表项
struct Page *get_page(pde_t *pgdir, uintptr_t va, pte_t **ptep_store);
void page_remove(pde_t *pgdir, uintptr_t va);
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t va, uint32_t perm);

void tlb_invalidate(pde_t *pgdir, uintptr_t vaddr);

void check_pgdir(void);

void print_pgdir(void);

#define PADDR(kva) ({                                                   \
            uintptr_t __kva = (uintptr_t)(kva);                         \
            if (__kva < KERNEL_BASE) {                                  \
                panic("PADDR called with invalid kva %08lx", __kva);    \
            }                                                           \
            __kva - KERNEL_BASE;                                        \
    })

// 通过物理地址，返回内核虚拟地址
#define KVADDR(pa) ({                                               \
            uintptr_t __pa = (pa);                                  \
            size_t __ppn = PPN(__pa);                               \
            const size_t __npage = get_npage();                     \
            if (__ppn >= __npage) {                                 \
                panic("KADDR called with invalid pa %08lx", __pa);  \
            }                                                       \
            if ((__pa + KERNEL_BASE >= KERNEL_TOP) ||                 \
                (__pa + KERNEL_BASE < KERNEL_BASE)) {                 \
                panic("KADDR called with invalid pa %08lx", __pa);  \
            }                                                       \
            (void *) (__pa + KERNEL_BASE);                          \
            })

struct Page *get_pages_base(void);
const size_t get_npage(void);

// 根据page获取物理页索引号
static inline ppn_t page2ppn(struct Page *page) {
    return page - get_pages_base();
}

// 根据page获取到page管理的物理其实地址
static inline uintptr_t page2pa(struct Page *page) {
    return page2ppn(page) << PAGE_SHIFT;
}

// 根据物理地址获取对应的page
static inline struct Page *pa2page(uintptr_t pa) {
    if (PPN(pa) >= get_npage()) {
        panic("pa2page called with invalid pa");
    }
    return &(get_pages_base()[PPN(pa)]);
}

// 根据page获取对应的内核虚拟地址
static inline void *page2kva(struct Page *page) {
    return KVADDR(page2pa(page));
}

static inline struct Page *kva2page(void *kva) {
    return pa2page(PADDR(kva));
}

static inline struct Page *pte2page(pte_t pte) {
    if (!(pte & PTE_P)) {
        panic("pte2page called with invalid pte\n");
    }
    return pa2page(PTE_ADDR(pte));
}

static inline struct Page *pde2page(pde_t pde) {
    if (!(pde & PTE_P)) {
        panic("pde2page called with invalid pde\n");
    }
    return pa2page(PDE_ADDR(pde));
}

static inline int page_ref(struct Page *page) {
    return atomic_read(&(page->ref));
}

static inline void set_page_ref(struct Page *page, int ref) {
    atomic_set(&(page->ref), ref);
}

static inline int page_ref_inc(struct Page *page) {
    return atomic_add_return(&(page->ref), 1);
}

static inline int page_ref_dec(struct Page *page) {
    return atomic_sub_return(&(page->ref), 1);
}

extern char boot_stack[], boot_stack_top[];

#endif // __KERNEL_MM_PMM_H__
