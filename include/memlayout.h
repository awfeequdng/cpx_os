#ifndef __INCLUDE_MEMLAYOUT_H__
#define __INCLUDE_MEMLAYOUT_H__

#include <mmu.h>

#define KERNEL_BASE	    0xc0000000
#define KERNEL_MEM_SIZE 0x38000000
#define KERNEL_TOP      (KERNEL_BASE + KERNEL_MEM_SIZE)

#define VPT     0xFAC00000

// #define K_STACK_TOP  KERNEL_BASE
#define K_STACK_SIZE (8 * PAGE_SIZE)

// 内核代码段
#define SEG_KTEXT	1
// 内核数据段
#define SEG_KDATA	2

// 用户态代码段
#define	SEG_UTEXT	3
// 用户态数据段
#define SEG_UDATA	4


#define SEG_TSS		5

#define GD_KTEXT 	((SEG_KTEXT) << 3)	// kernel text
#define GD_KDATA	((SEG_KDATA) << 3)  // kernel data
#define GD_UTEXT	((SEG_UTEXT) << 3)  // user text
#define GD_UDATA	((SEG_UDATA) << 3)	// user data
#define GD_TSS		((SEG_TSS) << 3)	// task segment selector

#define DPL_KERNEL	0
#define DPL_USER	3

#define KERNEL_CS	((GD_KTEXT) | DPL_KERNEL)
#define KERNEL_DS	((GD_KDATA) | DPL_KERNEL)
#define USER_CS		((GD_UTEXT) | DPL_USER)
#define USER_DS		((GD_UDATA) | DPL_USER)

#ifndef __ASSEMBLER__

#include <atomic.h>
#include <list.h>

#define E820_MAX            20      // number of entries in E820MAP
#define E820_ARM            1       // address range memory
#define E820_ARR            2       // address range reserved

struct E820Map {
    int nr_map;
    struct {
        uint64_t addr;
        uint64_t size;
        uint32_t type;
    } __attribute__((packed)) map[E820_MAX];
};


struct Page {
    atomic_t ref;                   // page frame's reference counter
    uint32_t flags;                 // array of flags that describe the status of the page frame
    unsigned int property;          // # of pages in continuous memory block
    int zone_num;                   // used in buddy system, the No. of zone which the page belongs to
    list_entry_t page_link;         // free list link
};

/* Flags describing the status of a page frame */
#define PG_reserved                 0       // the page descriptor is reserved for kernel or unusable
#define PG_property                 1       // the member 'property' is valid

#define SetPageReserved(page)       set_bit(PG_reserved, &((page)->flags))
#define ClearPageReserved(page)     clear_bit(PG_reserved, &((page)->flags))
#define PageReserved(page)          test_bit(PG_reserved, &((page)->flags))
#define SetPageProperty(page)       set_bit(PG_property, &((page)->flags))
#define ClearPageProperty(page)     clear_bit(PG_property, &((page)->flags))
#define PageProperty(page)          test_bit(PG_property, &((page)->flags))

#define le2page(le, member)         \
    container_of((le), struct Page, member)

typedef struct {
    list_entry_t free_list;
    unsigned int nr_free;
} free_area_t;

#endif // __ASSEMBLER__

#endif // __INCLUDE_MEMLAYOUT_H__
