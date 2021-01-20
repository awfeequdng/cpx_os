#include <sfs.h>
#include <assert.h>
#include <stdio.h>
#include <types.h>
#include <error.h>
#include <vfs.h>
#include <slab.h>
#include <iobuf.h>
#include <inode.h>

static int sfs_sync(Fs *fs) {
    SfsFs *sfs = fsop_info(fs, sfs);
    lock_sfs_fs(sfs);
    {
        ListEntry *head = &(sfs->inode_list);
        ListEntry *entry = head;
        while ((entry = list_next(entry)) != head) {
            SfsInode *sfs_inode = le2sfsinode(entry, inode_link);
            vop_fsync(info2node(sfs_inode, sfs_inode));
        }
    }
    unlock_sfs_fs(sfs);

    int ret;
    if (sfs->super_dirty) {
        sfs->super_dirty = false;
        if ((ret = sfs_sync_super(sfs)) != 0) {
            sfs->super_dirty = true;
            return ret;
        }
        // todo: 为什么superblock为dirty时，freemap也需要被写入到磁盘？
        if ((ret = sfs_sync_freemap(sfs)) != 0) {
            sfs->super_dirty = true;
            return ret;
        }
    }
    return 0;
}

static Inode *sfs_get_root(Fs *fs) {
    Inode *node = NULL;
    int ret;
    if ((ret = sfs_load_inode(fsop_info(fs, sfs), &node, SFS_ROOT_BLK_NO)) != 0) {
        panic("load sfs root failed: %e", ret);
    }
    return node;
}

static int sfs_unmount(Fs *fs) {
    SfsFs *sfs = fsop_info(fs, sfs);
    if (!list_empty(&(sfs->inode_list))) {
        return -E_BUSY;
    }
    assert(!sfs->super_dirty);
    bitmap_destory(sfs->freemap);
    kfree(sfs->sfs_buffer);
    kfree(sfs->hash_list);
    // 最后将fs释放
    kfree(fs);
    return 0;
}

static void sfs_cleanup(Fs *fs) {
    SfsFs *sfs = fsop_info(fs, sfs);
    uint32_t blocks = sfs->super.blocks;
    uint32_t unused_blocks = sfs->super.unused_blocks;
    printk("sfs: cleanup: '%s' (%d/%d/%d)\n",
           sfs->super.info,
           blocks - unused_blocks,
           unused_blocks,
           blocks);
    int i, ret;
    // 不断重复执行文件系统的sync功能， 直到所有数据都被写入到磁盘为止；
    // 我们不能无限尝试，最多尝试32次，如果还没有同步完所有数据，我们提示错误
    for (i = 0; i < 32; i++) {
        if ((ret = fsop_sync(fs)) == 0) {
            break;
        }
    }
    if (ret != 0) {
        warn("sfs: sync error: '%s': %e.\n", sfs->super.info, ret);
    }
}

static int sfs_init_read(Device *dev, uint32_t blkno, void *blk_buffer) {
    IOBuf __iob, *iob = iobuf_init(&__iob, blk_buffer, SFS_BLK_SIZE, blkno * SFS_BLK_SIZE);
    return dop_io(dev, iob, false);
}

static int sfs_init_freemap(Device *dev, SfsBitmap *bitmap, uint32_t blkno, uint32_t nblks, void *blk_buffer) {
    size_t len;
    void *data = bitmap_get_data(bitmap, &len);
    assert(data != NULL && len == nblks * SFS_BLK_SIZE);
    while (nblks != 0) {
        int ret;
        if ((ret = sfs_init_read(dev, blkno, data)) != 0) {
            return ret;
        }
        blkno++;
        nblks--;
        data += SFS_BLK_SIZE;
    }
    return 0;
}

static int sfs_do_mount(Device *dev, Fs **fs_store) {
    static_assert(SFS_BLK_SIZE >= sizeof(SfsSuper));
    static_assert(SFS_BLK_SIZE >= sizeof(SfsDiskInode));
    static_assert(SFS_BLK_SIZE >= sizeof(SfsDiskEntry));

    if (dev->d_blocksize != SFS_BLK_SIZE) {
        return -E_NA_DEV;
    }

    Fs *fs = NULL;
    if ((fs = alloc_fs(sfs)) == NULL) {
        return -E_NO_MEM;
    }

    SfsFs *sfs = fsop_info(fs, sfs);
    sfs->dev = dev;

    int ret = -E_NO_MEM;
    void *sfs_buffer = NULL;
    if ((sfs->sfs_buffer = sfs_buffer = kmalloc(SFS_BLK_SIZE)) == NULL) {
        goto failed_cleanup_fs;
    }

    if ((ret = sfs_init_read(dev, SFS_SUPER_BLK_NO, sfs_buffer)) != 0) {
        goto failed_cleanup_sfs_buffer;
    }

    ret = -E_INVAL;
    SfsSuper *super = sfs_buffer;

    if (super->magic != SFS_MAGIC) {
        printk("sfs: wrong magic in superblock. (0x%08x should be 0x%08x).\n",
            super->magic, SFS_MAGIC);
            goto failed_cleanup_sfs_buffer;
    }

    if (super->blocks > dev->d_blocks) {
        printk("sfs: fs has %u blocks, device has %u blocks.\n",
            super->blocks, dev->d_blocks);
            goto failed_cleanup_sfs_buffer;
    }
    super->info[SFS_MAX_INFO_LEN] = '\0';

    sfs->super = *super;
    
    ret = -E_NO_MEM;

    uint32_t i;

    ListEntry *hash_list = NULL;
    if ((sfs->hash_list = hash_list =kmalloc(sizeof(ListEntry) * SFS_HLIST_SIZE)) == NULL) {
        goto failed_cleanup_sfs_buffer;
    }
    for (i = 0; i < SFS_HLIST_SIZE; i++) {
        list_init(hash_list + i);
    }

    SfsBitmap *bitmap = NULL;
    uint32_t freemap_size_nbits = sfs_freemap_bits(super);
    if ((sfs->freemap = bitmap = bitmap_create(freemap_size_nbits)) == NULL) {
        goto failed_cleanup_hash_list;
    }
    uint32_t freemap_size_nblks = sfs_freemap_blocks(super);
    if ((ret = sfs_init_freemap(dev, bitmap, SFS_FREEMAP_BLK_NO, freemap_size_nblks, sfs_buffer)) != 0) {
        goto failed_cleanup_freemap;
    }

    uint32_t blocks = sfs->super.blocks;
    uint32_t unused_blocks = 0;
    for (i = 0; i < freemap_size_nbits; i++) {
        if (bitmap_test(bitmap, i)) {
            unused_blocks++;
        }
    }

    assert(unused_blocks == sfs->super.unused_blocks);

    sfs->super_dirty = 0;
    sem_init(&(sfs->fs_sem), 1);
    sem_init(&(sfs->io_sem), 1);
    sem_init(&(sfs->mutex_sem), 1);

    list_init(&(sfs->inode_list));
    printk("sfs: mount: '%s' (%d/%d/%d)\n", sfs->super.info,
           blocks - unused_blocks,
           unused_blocks,
           blocks);
    
    fs->fs_sync = sfs_sync;
    fs->fs_get_root = sfs_get_root;
    fs->fs_unmount = sfs_unmount;
    fs->fs_cleanup = sfs_cleanup;

    *fs_store = fs;
    return 0;
failed_cleanup_freemap:
    bitmap_destory(bitmap);
failed_cleanup_hash_list:
    kfree(hash_list);
failed_cleanup_sfs_buffer:
    kfree(sfs_buffer);
failed_cleanup_fs:
    kfree(fs);
    return ret;
}

int sfs_mount(const char *dev_name) {
    return vfs_mount(dev_name, sfs_do_mount);
}

#include <process.h>
void sfs_print_bitmap(void) {
    FsStruct *fs_struct = current->fs_struct;
        printk("fs_struct = %p\n", fs_struct);
    if (fs_struct == NULL || fs_count(fs_struct) == 0) {
        return;
    }
    Fs *fs = NULL;
    if (fs_struct->pwd != NULL) {
        fs = fs_struct->pwd->in_fs;
    } else {
        Inode *node_store = NULL;
        if (vfs_get_bootfs(&node_store) != 0) {
            printk("node_store = NULL\n");
            return;
        }
        fs = node_store->in_fs;
    }
    
    if (check_fs_type(fs, sfs)) {
        printk("sfs\n");
        SfsFs * sfs_fs = fsop_info(fs, sfs);
        if (sfs_fs->freemap == NULL) {
            return;
        }
        int i;
        int n = sfs_fs->freemap->nwords;
        printk("n = %d\n", n);
        for (i = 0; i < n; i++) {
            if (i % 8 == 0) {
                printk("\n");
            }
            printk("%08x ", sfs_fs->freemap->map[i]);
        }
        printk("\n");
    }
}