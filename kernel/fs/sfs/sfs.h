#ifndef __KERNEL_FS_SFS_H__
#define __KERNEL_FS_SFS_H__

#include <mmu.h>
#include <unistd.h>
#include <types.h>
#include <semaphore.h>
#include <dev.h>
#include <stdlib.h>
#include <bitmap.h>


#define SFS_MAGIC           0x2f8dbe2a
#define SFS_BLK_SIZE        PAGE_SIZE
// sfs文件直接索引块的个数，也就是说通过直接索引可以最大达到12个block大小的文件
#define SFS_N_DIRECT        12
// 超级块对象的信息最大长度
#define SFS_MAX_INFO_LEN    31
#define SFS_MAX_FNAME_LEN   FS_MAX_FNAME_LEN
// 文件最大大小：以block为单位， 即最大128M
#define SFS_MAX_FILE_SIZE   (1024 * 1024 * 128)
// 超级快所在的block number
#define SFS_SUPER_BLK_NO    0 
// 根目录inode信息所在的block
#define SFS_ROOT_BLK_NO     1 
// 可用于存储inode以及data的block开始序号
#define SFS_FREEMAP_BLK_NO  2

// 一个block中bit位的个数
#define SFS_BLK_BITS        ((SFS_BLK_SIZE) * BYTE_BITS)

// 在sfs disk inode的间接索引block中，一个block最多可以放多少个索引数
#define SFS_BLK_N_ENTRY     ((SFS_BLK_SIZE) / sizeof(uint32_t))

// 文件类型
#define SFS_TYPE_INVAL      0
#define SFS_TYPE_FILE       1
#define SFS_TYPE_DIR        2
#define SFS_TYPE_LINK       3

// sfs文件系统超级块对象 (on disk)
typedef struct sfs_super {
    uint32_t magic;
    uint32_t blocks;
    uint32_t unused_blocks;
    char info[SFS_MAX_INFO_LEN + 1];
} SfsSuper;

// sfs文件系统磁盘inode信息 (on disk)
typedef struct sfs_disk_inode {
    union {
        struct {
            uint32_t size;
        } fileinfo;
        struct {
            uint32_t slots;
            uint32_t parent;
        } dir_info;
    };
    uint16_t type;
    uint16_t nlinks;
    uint32_t blocks;
    uint32_t direct[SFS_N_DIRECT];
    uint32_t indirect;
    // todo: 这个字段用来干嘛的？
    uint32_t db_indirect;
} SfsDiskInode;

typedef struct sfs_disk_entry {
    uint32_t ino;
    char name[SFS_MAX_FNAME_LEN + 1];
} SfsDiskEntry;

#define sfs_dentry_size      \
    sizeof(((SfsDiskEntry *)0)->name)

// sfs文件系统在内存中的inode信息 (in memory)
typedef struct sfs_inode {
    SfsDiskInode *disk_inode;
    uint32_t ino;
    uint32_t flags;
    bool dirty;
    // todo: 这个字段干嘛的？
    int reclaim_count;
    Semaphore sem;
    ListEntry inode_link;
    ListEntry hash_link;
} SfsInode;

#define SFS_removed             0

#define SetSFSInodeRemoved(sinode)      set_bit(SFS_removed, &((sinode)->flags)) 
#define ClearSFSInodeRemoved(sinode)      clear_bit(SFS_removed, &((sinode)->flags)) 
#define SFSInodeRemoved(sinode)      test_bit(SFS_removed, &((sinode)->flags)) 

#define le2sfsinode(le, member)     \
    container_of((le), SfsInode, member)

typedef struct sfs_fs {
    SfsSuper super;
    Device *dev;
    SfsBitmap *freemap;
    bool super_dirty;
    void *sfs_buffer;
    Semaphore fs_sem;
    Semaphore io_sem;
    Semaphore mutex_sem;
    // 该链表用于链接SfsInode节点
    ListEntry inode_list;
    ListEntry *hash_list;
} SfsFs;

#define SFS_HLIST_SHIFT                 10
#define SFS_HLIST_SIZE                  (1 << SFS_HLIST_SHIFT)
#define sfs_inode_hashfn(x)             (hash32(x, SFS_HLIST_SHIFT))

#define sfs_freemap_bits(super)         ROUNDUP(((super)->blocks), SFS_BLK_BITS)
// 需要多少个block来存放bitmap
#define sfs_freemap_blocks(super)       ROUNDUP_DIV(((super)->blocks), SFS_BLK_BITS)

struct fs;
struct inode;

void sfs_init(void);
int sfs_mount(const char *dev_name);

void lock_sfs_fs(SfsFs *sfs);
void lock_sfs_io(SfsFs *sfs);
void lock_sfs_mutex(SfsFs *sfs);
void unlock_sfs_fs(SfsFs *sfs);
void unlock_sfs_io(SfsFs *sfs);
void unlock_sfs_mutex(SfsFs *sfs);

int sfs_rblock(SfsFs *sfs, void *buf, uint32_t blk_no, uint32_t num_blks);
int sfs_wblock(SfsFs *sfs, void *buf, uint32_t blk_no, uint32_t num_blks);
int sfs_rbuf(SfsFs *sfs, void *buf, size_t len, uint32_t blk_no, off_t offset);
int sfs_wbuf(SfsFs *sfs, void *buf, size_t len, uint32_t blk_no, off_t offset);
int sfs_sync_super(SfsFs *sfs);
int sfs_sync_freemap(SfsFs *sfs);
int sfs_clear_block(SfsFs *sfs, uint32_t blk_no, uint32_t num_blks);

int sfs_load_inode(SfsFs *sfs, struct inode **node_store, uint32_t ino);

#endif //__KERNEL_FS_SFS_H__