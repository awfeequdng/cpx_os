#ifndef __KERNEL_FS_FS_H__
#define __KERNEL_FS_FS_H__

#include <mmu.h>
#include <types.h>
#include <semaphore.h>
#include <atomic.h>

// 磁盤扇區大小爲512B
#define SECT_SIZE       512
#define PAGE_NSECT      (PAGE_SIZE / SECT_SIZE)

// 第一個磁盤作爲swap分區使用
#define SWAP_DEV_NO     1
// 文件系统磁盘分区号
#define DISK0_DEV_NO    2

struct inode;
struct file;

typedef struct fs_struct {
    // 当前工作目录
    struct inode *pwd;
    struct file *filemap;
    atomic_t fs_count;
    Semaphore fs_sem;
} FsStruct;

#define FS_STRUCT_BUFSIZE       (PAGE_SIZE - sizeof(FsStruct))
#define FS_STRUCT_NENTRY        (FS_STRUCT_BUFSIZE / sizeof(struct file))

void lock_fs(FsStruct *fs_struct);
void unlock_fs(FsStruct *fs_struct);

void fs_init(void);
void fs_cleanup(void);

FsStruct *fs_create(void);
void fs_destory(FsStruct *fs_struct);
int dup_fs(FsStruct *to, FsStruct *from);

static inline int fs_count(FsStruct *fs_struct) {
    return atomic_read(&(fs_struct->fs_count));
}

static inline int fs_count_inc(FsStruct *fs_struct) {
    return atomic_add_return(&(fs_struct->fs_count), 1);
}

static inline int fs_count_dec(FsStruct *fs_struct) {
    return atomic_sub_return(&(fs_struct->fs_count), 1);
}


#endif // __KERNEL_FS_FS_H__