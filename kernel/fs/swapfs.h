#ifndef __KERNEL_FS_SWAPFS_H__
#define __KERNEL_FS_SWAPFS_H__

#include <swap.h>

void swapfs_init(void);
// 从swap分区中将entry映射的swap frame复制到page中
int swapfs_read(swap_entry_t entry, struct Page *page);
// 将page的内容写到swap分区中entry映射的swap frame
int swapfs_write(swap_entry_t entry, struct Page *page);
#endif // __KERNEL_FS_SWAPFS_H__