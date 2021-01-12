#ifndef __KERNEL_MM_SWAP_H__
#define __KERNEL_MM_SWAP_H__

#include <types.h>
#include <assert.h>
#include <memlayout.h>

/* *
 * swap_entry_t
 * --------------------------------------------
 * |         offset        |   reserved   | 0 |
 * --------------------------------------------
 *           24 bits            7 bits    1 bit
 * */

#define MAX_SWAP_OFFSET_LIMIT   (1 << 24)


#define swap_offset(entry) ({                 \
        size_t __offset = (entry) >> 8;       \
        if (!(__offset > 0 && __offset < swap_max_offset())) {  \
            panic("invalid swap_entry_t = %08x.\n", entry);     \
        }                   \
        __offset;           \
    })

size_t swap_max_offset(void);
void swap_set_max_offset(size_t max_offset);

void swap_init(void);
bool try_free_pages(size_t n);

void swap_remove_entry(swap_entry_t entry);
int swap_page_count(struct Page *page);
void swap_decrease(swap_entry_t entry);
void swap_duplicate(swap_entry_t entry);
int swap_in_page(swap_entry_t entry, struct Page **pagep);
int swap_copy_entry(swap_entry_t entry, swap_entry_t *store);

int kswapd_main(void *arg) __attribute__((noreturn));
#endif // __KERNEL_MM_SWAP_H__
