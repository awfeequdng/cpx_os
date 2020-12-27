#include <types.h>
#include <x86.h>
#include <mmu.h>
#include <memlayout.h>
#include <pmm.h>


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
uint8_t stack0[1024];

static struct TaskState ts = {0};

static void gdt_init(void) {
    ts.ts_esp0 = (uint32_t)stack0 + sizeof(stack0);
    ts.ts_ss0 = KERNEL_DS;

    gdt[SEG_TSS] = SEGTSS(STS_T32A, (uint32_t)&ts, sizeof(ts), DPL_KERNEL);

    pmm_lgdt(&gdt_pd);

    // load tss
    ltr(GD_TSS);
}

void pmm_init(void) {
    gdt_init();
}