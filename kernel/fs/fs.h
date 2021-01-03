#ifndef __KERNEL_FS_FS_H__
#define __KERNEL_FS_FS_H__

#include <mmu.h>

// 磁盤扇區大小爲512B
#define SECT_SIZE       512

#define PAGE_NSECT      (PAGE_SIZE / SECT_SIZE)

// 第一個磁盤作爲swap分區使用
#define SWAP_DEV_NO     1

#endif // __KERNEL_FS_FS_H__