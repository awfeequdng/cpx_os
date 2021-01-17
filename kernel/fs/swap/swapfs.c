
#include <swapfs.h>
#include <assert.h>
#include <ide.h>
#include <fs.h>
#include <mmu.h>
#include <pmm.h>

void swapfs_init(void) {
    static_assert((PAGE_SIZE % SECT_SIZE) == 0);
    if (!ide_device_valid(SWAP_DEV_NO)) {
        panic("swap fs isn't available.\n");
    }

    size_t max_size = ide_device_size(SWAP_DEV_NO) / (PAGE_NSECT);
    swap_set_max_offset(max_size);
}

// 从swap分区中将entry映射的swap frame复制到page中
// return: 0 成功，其他值失败
int swapfs_read(swap_entry_t entry, struct Page *page) {
    return ide_read_secs(SWAP_DEV_NO, swap_offset(entry) * PAGE_NSECT,
                         page2kva(page), PAGE_NSECT);
}

// 将page的内容写到swap分区中entry映射的swap frame
// return: 0 成功， 其他值为失败
int swapfs_write(swap_entry_t entry, struct Page *page) {
    return ide_write_secs(SWAP_DEV_NO, swap_offset(entry) * PAGE_NSECT,
                          page2kva(page), PAGE_NSECT);
}