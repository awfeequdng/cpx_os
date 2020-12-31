#ifndef __KERNEL_MM_VMM_H__
#define __KERNEL_MM_VMM_H__

#include <types.h>
#include <list.h>
#include <memlayout.h>
#include <sync.h>


struct MmStruct;

struct VmaStruct {
    struct MmStruct *vm_mm;
    // [start, end)
    uintptr_t vm_start;
    uintptr_t vm_end;

    uint32_t vm_flags;
    list_entry_t vma_link;
};

#define le2vma(le, member)  \
    container_of(le, struct VmaStruct, member)

#define VM_READ         0x00000001
#define VM_WRITE        0x00000002
#define VM_EXEC         0x00000004

struct MmStruct {
    list_entry_t mmap_link;
    struct VmaStruct *mmap_cache;
    pde_t *page_dir;
    int map_count;
};

// 當節點數量大於32時，採用紅黑樹將vma鏈接起來
#define RB_MIN_MAP_COUNT 32

struct VmaStruct *find_vma(struct MmStruct *mm, uintptr_t addr);
struct VmaStruct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags);
void insert_vma_struct(struct MmStruct *mm, struct VmaStruct *vma);

struct MmStruct *mm_create(void);
void mm_destory(struct MmStruct *mm);

void vmm_init(void);

int do_page_fault(struct MmStruct *m, uint32_t error_code, uintptr_t addr);


#endif // __KERNEL_MM_VMM_H__