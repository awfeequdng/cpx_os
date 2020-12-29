#include <types.h>
#include <x86.h>
#include <mmu.h>
#include <memlayout.h>
#include <pmm.h>
#include <default_pmm.h>
#include <sync.h>


static struct SegDesc gdt[] = {
	SEG_NULL,
    [SEG_KTEXT] = SEG(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_KERNEL),
    [SEG_KDATA] = SEG(STA_W, 0x0, 0xFFFFFFFF, DPL_KERNEL),
    [SEG_UTEXT] = SEG(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_USER),
    [SEG_UDATA] = SEG(STA_W, 0x0, 0xFFFFFFFF, DPL_USER),
    [SEG_TSS]   = SEG_NULL,
};

static struct PseudoDescriptor gdt_pd = {
    sizeof(gdt) - 1, (uintptr_t)gdt
};

static struct TaskState ts = {0};

struct Page *pages_base;

size_t pages_num = 0;

const struct Page *get_pages_base(void) {
    return pages_base;
}

const size_t get_npage(void) {
    return pages_num;
}

pde_t *boot_pgdir = NULL;

// boot_cr3位页目录的物理地址（非虚拟地址）
uintptr_t boot_cr3;

const struct PmmManager *pmm_manager;

// 重新定义以下lgdt的实现
static inline void pmm_lgdt(struct PseudoDescriptor *pd) {
    asm volatile ("lgdt (%0)" :: "r" (pd));
    asm volatile ("movw %%ax, %%gs" :: "a" (USER_DS));
    asm volatile ("movw %%ax, %%fs" :: "a" (USER_DS));
    asm volatile ("movw %%ax, %%es" :: "a" (KERNEL_DS));
    asm volatile ("movw %%ax, %%ds" :: "a" (KERNEL_DS));
    asm volatile ("movw %%ax, %%ss" :: "a" (KERNEL_DS));
    // reload cs
    asm volatile ("ljmp %0, $1f\n 1:\n" :: "i" (KERNEL_CS));
}

// 内核临时堆栈
// uint8_t stack0[1024];

void load_esp0(uintptr_t esp0) {
    ts.ts_esp0 = esp0;
}

static void gdt_init(void) {
    load_esp0(boot_stack_top);

    ts.ts_ss0 = KERNEL_DS;

    gdt[SEG_TSS] = SEGTSS(STS_T32A, (uint32_t)&ts, sizeof(ts), DPL_KERNEL);

    pmm_lgdt(&gdt_pd);

    // load tss
    ltr(GD_TSS);
}

static void init_pmm_manager(void) {
    pmm_manager = get_default_pmm_manager();
    printk("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

struct Page *alloc_pages(size_t n) {
    struct Page *page;
    bool flag;
    local_intr_save(flag);
    page = pmm_manager->alloc_pages(n);
    local_intr_restore(flag);
    return page;
}

void free_pages(struct Page *base, size_t n) {
    bool flag;
    local_intr_save(flag);
    pmm_manager->free_pages(base, n);
    local_intr_restore(flag);
}

size_t nr_free_pages(void) {
    size_t size;
    bool flag;
    local_intr_save(flag);
    size = pmm_manager->nr_free_pages();
    local_intr_restore(flag);
    return size;
}

static void page_init(void) {
    struct E820Map *memmap = (struct E820Map *)(0x8000 + KERNEL_BASE);
    uint64_t max_phy_addr = 0;

    printk("e820 map:\n");
    int i;
    for (i = 0; i < memmap->nr_map; i++) {
        uint64_t begin = memmap->map[i].addr;
        uint64_t size = memmap->map[i].size;
        uint64_t end = begin + size;
        uint32_t type = memmap->map[i].type;
        printk("  memory: %08llx, [%08llx, %08llx), type = %d\n",
            size, begin, end, type);
        if (type == E820_ARM) {
            if (max_phy_addr < end && begin < KERNEL_MEM_SIZE) {
                max_phy_addr = end;
            }
        }
    }
    if (max_phy_addr > KERNEL_MEM_SIZE) {
        max_phy_addr = KERNEL_MEM_SIZE;
    }

    extern char end[];
    pages_num = max_phy_addr / PAGE_SIZE;
    pages_base = (struct Page *)ROUNDUP((void *)end, PAGE_SIZE);

    for (i = 0; i < pages_num; i++) {
        SetPageReserved(pages_base + i);
    }


    uintptr_t free_mem = PADDR((uintptr_t)pages_base + sizeof(struct Page) * pages_num);


    for (i = 0; i < memmap->nr_map; i++) {
        uint64_t begin = memmap->map[i].addr;
        uint64_t size = memmap->map[i].size;
        uint64_t end = begin + size;
        uint32_t type = memmap->map[i].type;
        if (type == E820_ARM) {
            if (begin < free_mem) {
                begin = free_mem;
            }
            // 这里限制了使用的物理地址最大为896M，这是为了让内核可以直接线性映射物理内存
            if (end > KERNEL_MEM_SIZE) {
                end = KERNEL_MEM_SIZE;
            }

            if (begin < end) {
                begin = ROUNDUP(begin, PAGE_SIZE);
                end = ROUNDDOWN(end, PAGE_SIZE);
                if (begin < end) {
                    init_memmap(pa2page(begin), (end - begin) / PAGE_SIZE);
                }
            }
        }
    }
}


static void enable_paging(void) {
    // 设置页目录地址
    lcr3(boot_cr3);

    // 开启分页
    uintptr_t cr0 = rcr0();
    cr0 |= CR0_PE | CR0_PG | CR0_AM | CR0_WP | CR0_NE | CR0_TS | CR0_EM | CR0_MP;
    cr0 &= ~(CR0_TS | CR0_EM);
    lcr0(cr0);
}

//boot_alloc_page - allocate one page using pmm->alloc_pages(1) 
// return value: the kernel virtual address of this allocated page
//note: this function is used to get the memory for PDT(Page Directory Table)&PT(Page Table)
static void *boot_alloc_page(void) {
    struct Page *page = alloc_page();
    if (page == NULL) {
        panic("boot_alloc_page failed.\n");
    }
    return page2kva(page);
}

    //use pmm->check to verify the correctness of the alloc/free function in a pmm
static void check_alloc_page() {
    pmm_manager->check();
}

static void check_pgdir(void) {
    assert(get_npage() <= KERNEL_MEM_SIZE / PAGE_SIZE);
    assert(boot_pgdir != NULL && (uintptr_t)PAGE_OFF(boot_pgdir) == 0);
    // assert(get_page(boot_pgdir, ))
}

extern pte_t entry_page_table[];
extern pde_t entry_page_dir[];

void kernel_page_table_init() {
    int i;
    int page_cnt = 0;

    boot_pgdir = entry_page_dir;
    boot_cr3 = PADDR(boot_pgdir);

    // 初始化所有的页表项，总共256个页表，每个页表1024项，总共可以映射1G的地址空间
    // 实际上我们不需要映射1G的地址空间，应为内核最大可用的线性地址为896M，也就是224个页表
    // page放在一个连续的1M地址空间中
    pte_t *page_table = entry_page_table;
    // int page_table_entries = 1024 * 256;
    // 映射0xc0000000 ~ 0xf8000000的地址空间，需要224个页表
    // 同时，我们实际的物理地址可能没有896M，此时肯能需要映射的页表就更小了
    page_cnt = 224;
    int page_table_entries = 1024 * page_cnt;
    for (i = 1024; i < page_table_entries; i++) {
        page_table[i] = (i << PAGE_SHIFT) | PTE_P | PTE_W; 
    }
    // 初始化224个页目录项
    // [KERNEL_BASE >> PDX_SHIFT] = ((uintptr_t)entry_page_table - KERNEL_BASE) + PTE_P + PTE_W
    for (i = 1; i < page_cnt; i++) {
        boot_pgdir[(KERNEL_BASE >> PDX_SHIFT) + i] = PADDR(page_table + (i * 1024)) | PTE_P | PTE_W;
    }
}

void pmm_init(void) {
    //We need to alloc/free the physical memory (granularity is 4KB or other size). 
    //So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    //First we should init a physical memory manager(pmm) based on the framework.
    //Then pmm can alloc/free the physical memory. 
    //Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    page_init();

    // 初始化内核页表，当前页表只映射了[0xc0000000, 0xc0400000)的虚拟地址，
    // 现在将将内核的映射扩大为[0xc0000000, 0xf8000000)，
    kernel_page_table_init();

    struct Page *page = alloc_page();
    uintptr_t kvaddr = page2kva(page);
    memset(kvaddr, 0, PAGE_SIZE);
    free_page(page);

    gdt_init();
}

pte_t *get_pte(pde_t *pgdir, uintptr_t va, bool create) {
    pde_t *pdep = &pgdir[PDX(va)];
    if (!(*pdep & PTE_P)) {
        struct Page *page;
    }

}