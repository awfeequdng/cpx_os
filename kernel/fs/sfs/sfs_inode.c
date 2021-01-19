#include <sfs.h>
#include <error.h>
#include <assert.h>
#include <inode.h>
#include <slab.h>
#include <vfs.h>
#include <iobuf.h>
#include <stat.h>
#include <string.h>

static const InodeOperations sfs_node_dir_ops;
static const InodeOperations sfs_node_file_ops;

static inline int trylock_sfs_inode(SfsInode *sfs_inode) {
    if (!SFSInodeRemoved(sfs_inode)) {
        down(&(sfs_inode->sem));
        if (!SFSInodeRemoved(sfs_inode)) {
            return 0;
        }
        up(&(sfs_inode->sem));
    }
    // inode不能是移除了的节点
    return -E_NOENT;
}

static inline void unlock_sfs_inode(SfsInode *sfs_inode) {
    up(&(sfs_inode->sem));
}

static const InodeOperations *sfs_get_ops(uint16_t type) {
    switch (type) {
        case SFS_TYPE_DIR:
            return &sfs_node_dir_ops;
        case SFS_TYPE_FILE:
            return &sfs_node_file_ops;
    }
    panic("invalid file type %d.\n", type);
}

static ListEntry *sfs_hash_list(SfsFs *sfs, uint32_t ino) {
    return sfs->hash_list + sfs_inode_hashfn(ino);
}

static void sfs_set_links(SfsFs *sfs, SfsInode *sfs_inode) {
    list_add(&(sfs->inode_list), &(sfs_inode->inode_link));
    list_add(sfs_hash_list(sfs, sfs_inode->ino), &(sfs_inode->hash_link));
}

static void sfs_remove_links(SfsInode *sfs_inode) {
    list_del(&(sfs_inode->inode_link));
    list_del(&(sfs_inode->hash_link));
}

static bool sfs_block_inuse(SfsFs *sfs, uint32_t ino) {
    if (ino != 0 && ino < sfs->super.blocks) {
        return !bitmap_test(sfs->freemap, ino);
    }
    panic("sfs_block_inuse: called out of range (0, %u) %u.\n", sfs->super.blocks, ino);
}

static int sfs_block_alloc(SfsFs *sfs, uint32_t *ino_store) {
    int ret;
    if ((ret = bitmap_alloc(sfs->freemap, ino_store)) !=0) {
        return ret;
    }
    assert(sfs->super.unused_blocks > 0);
    sfs->super.unused_blocks--;
    sfs->super_dirty = 1;
    assert(sfs_block_inuse(sfs, *ino_store));
    return sfs_clear_block(sfs, *ino_store, 1);
}

static void sfs_block_free(SfsFs *sfs, uint32_t ino) {
    assert(sfs_block_inuse(sfs, ino));
    bitmap_free(sfs->freemap, ino);
    sfs->super.unused_blocks++;
    sfs->super_dirty = true;
}

static int sfs_create_inode(SfsFs *sfs, SfsDiskInode *disk_inode, uint32_t ino, Inode **node_store) {
    Inode *node = NULL;
    if ((node = alloc_inode(sfs_inode)) != NULL) {
        vop_init(node, sfs_get_ops(disk_inode->type), info2fs(sfs, sfs));
        SfsInode *sfs_inode = vop_info(node, sfs_inode);
        sfs_inode->disk_inode = disk_inode;
        sfs_inode->ino = ino;
        sfs_inode->dirty = 0;
        sfs_inode->flags = 0;
        sfs_inode->reclaim_count = 1;
        sem_init(&(sfs_inode->sem), 1);
        *node_store = node;
        return 0;
    }
    return -E_NO_MEM;
}

static Inode *lookup_sfs_nolock(SfsFs *sfs, uint32_t ino) {
    Inode *node = NULL;
    ListEntry *head = sfs_hash_list(sfs, ino);
    ListEntry *entry = head;
    while ((entry = list_next(entry)) != head) {
        SfsInode *sfs_inode = le2sfsinode(entry, hash_link);
        if (sfs_inode->ino == ino) {
            node = info2node(sfs_inode, sfs_inode);
            if (vop_ref_inc(node) == 1) {
                // todo: 这是何意
                sfs_inode->reclaim_count++;
            }
            return node;
        }
    }
    return NULL;
}

int sfs_load_inode(SfsFs *sfs, Inode **node_store, uint32_t ino) {
    Inode *node = NULL;
    
    lock_sfs_fs(sfs);
    if ((node = lookup_sfs_nolock(sfs, ino)) != NULL) {
        goto out_unlock;
    }
    int ret = -E_NO_MEM;
    SfsDiskInode *disk_inode = NULL;
    if ((disk_inode = kmalloc(sizeof(SfsDiskInode))) == NULL) {
        goto failed_unlock;
    }
    assert(sfs_block_inuse(sfs, ino));
    if ((ret = sfs_rbuf(sfs, disk_inode, sizeof(SfsDiskInode), ino, 0)) != 0) {
        goto failed_cleanup_disk_inode;
    }

    // todo: 为什么不是为0
    assert(disk_inode->nlinks != 0);
    if ((ret = sfs_create_inode(sfs, disk_inode, ino, &node)) != 0) {
        goto failed_cleanup_disk_inode;
    }
    sfs_set_links(sfs, vop_info(node, sfs_inode));

out_unlock:
    unlock_sfs_fs(sfs);
    *node_store = node;
    return 0;

failed_cleanup_disk_inode:
    kfree(disk_inode);
failed_unlock:
    unlock_sfs_fs(sfs);
    return ret;
}

// 功能： 从indirect索引的block中获得数据块索引号
// index: 文件内数据块的索引
// indirect_blkno: 文件间接索引块的索引号
// create: 文件是否需要向上增长一个索引块大小
// blkno_store: 数据块所在的block索引号
static int sfs_block_get_indirect_nolock(SfsFs *sfs, uint32_t *indirect_blkno, uint32_t index, bool create, uint32_t *blkno_store) {
    assert(index < SFS_BLK_N_ENTRY);
    int ret;
    uint32_t indirect;
    uint32_t blkno = 0;
    off_t offset = index * sizeof(uint32_t);
    // indirect不为0，说明之前已经为indirect分配了block
    if ((indirect = *indirect_blkno) != 0) {
        // 将indirect中第index个block索引号读出来存放在in
        if ((ret = sfs_rbuf(sfs, &blkno, sizeof(uint32_t), indirect, offset)) != 0) {
            return ret;
        }
        if (blkno != 0 || !create) {
            // blkno不为0说明之前已经在这个blkno处存放了数据；
            // blkno为0并且create不为true，表明读取超过了限制而又不创建新的block，直接返回
            goto out;
        }
    } else {
        if (!create) {
            // indirect为0，说明没有空间了也不创建（create为false），直接返回
            goto out;
        }
        // 位indirect分配索引号
        if ((ret = sfs_block_alloc(sfs, &blkno)) != 0) {
            return ret;
        }
    }
    if ((ret = sfs_block_alloc(sfs, &blkno)) != 0) {
        goto failed_cleanup;
    }
    // 将新申请的数据块blkno索引号写入indirect的offset处
    if ((ret = sfs_wbuf(sfs, &blkno, sizeof(uint32_t), indirect, offset)) != 0) {
        sfs_block_free(sfs, &blkno);
        goto failed_cleanup;
    }

out:
    if (indirect != *indirect_blkno) {
        // 不相等说明之前indirect为0，现在分配了一个新的indirect索引号
        *indirect_blkno = indirect;
    }
    *blkno_store = blkno;
    return 0;
failed_cleanup:
    if (indirect != *indirect_blkno) {
        sfs_block_free(sfs, indirect);
    }
    return ret;
}

// 获取文件index索引处的数据块索引（文件索引和数据块索引不是同一个概念）
static int sfs_block_get_nolock(SfsFs *sfs, SfsInode *sfs_inode, uint32_t index, bool create, uint32_t *blkno_store) {
    SfsDiskInode *disk_inode = sfs_inode->disk_inode;
    int ret;
    uint32_t indirect;
    uint32_t blkno = 0;
    // index小于直接索引的最大值，直接申请一个新的blkno即可（如果需要创建的话）
    // 然后将这个新的blkno放在直接索引数组内
    if (index < SFS_N_DIRECT) {
        if ((blkno = disk_inode->direct[index]) == 0 && create) {
            if ((ret = sfs_block_alloc(sfs, &blkno)) != 0) {
                return ret;
            }
            disk_inode->direct[index] = blkno;
            sfs_inode->dirty = true;
        }
        goto out;
    }

    index -= SFS_N_DIRECT;
    if (index < SFS_BLK_N_ENTRY) {
        // index在间接索引范围内
        indirect = disk_inode->indirect;
        if ((ret = sfs_block_get_indirect_nolock(sfs, &indirect, index, create, &blkno)) != 0) {
            return ret;
        }
        if (indirect != disk_inode->indirect) {
            assert(disk_inode->indirect == 0);
            disk_inode->indirect = indirect;
            sfs_inode->dirty = true;
        }
        goto out;
    }
    index -= SFS_BLK_N_ENTRY;
    // index在间接的间接索引处（double indirect）
    indirect = disk_inode->db_indirect;
    // index / SFS_BLK_N_ENTRY 的值必须小于SFS_BLK_N_ENTRY；
    // 也就是说一个文件的最大大小为：12 * 4k + 1024 * 4k + 1024 * 1024 * 4k
    if ((ret = sfs_block_get_indirect_nolock(sfs, &indirect, index / SFS_BLK_N_ENTRY, create, &blkno)) != 0) {
        return ret;
    }
    if (indirect != disk_inode->db_indirect) {
        assert(disk_inode->db_indirect == 0);
        disk_inode->db_indirect = indirect;
        sfs_inode->dirty = true;
    }
    if ((indirect = blkno) != 0) {
        if ((ret = sfs_block_get_indirect_nolock(sfs, &indirect, index % SFS_BLK_N_ENTRY, create, &blkno)) != 0) {
            return ret;
        }
    }

out:
    assert(blkno == 0 || sfs_block_inuse(sfs, blkno));
    *blkno_store = blkno;
    return 0;
}

static int sfs_block_free_indirect_nolock(SfsFs *sfs, uint32_t indirect, uint32_t index) {
    assert(sfs_block_inuse(sfs, indirect) && index < SFS_BLK_N_ENTRY);
    int ret;
    uint32_t blkno, zero = 0;
    off_t offset = index * sizeof(uint32_t);
    if ((ret = sfs_rbuf(sfs, &blkno, sizeof(uint32_t), indirect, offset)) != 0) {
        return ret;
    }
    if (blkno != 0) {
        if ((ret = sfs_wbuf(sfs, &zero, sizeof(uint32_t), indirect, offset)) != 0) {
            return ret;
        }
        sfs_block_free(sfs, blkno);
    }
    return 0;
}

static int sfs_block_free_nolock(SfsFs *sfs, SfsInode *sfs_inode, uint32_t index) {
    SfsDiskInode *disk_inode = sfs_inode->disk_inode;
    int ret;
    uint32_t indirect, blkno;
    if (index < SFS_N_DIRECT) {
        if ((blkno = disk_inode->direct[index]) != 0) {
            sfs_block_free(sfs, blkno);
            disk_inode->direct[index] = 0;
            sfs_inode->dirty = true;
        }
        return 0;
    }

    index -= SFS_N_DIRECT;
    if (index < SFS_BLK_N_ENTRY) {
        if ((indirect = disk_inode->indirect) != 0) {
            if ((ret = sfs_block_free_indirect_nolock(sfs, indirect, index)) != 0) {
                return ret;
            }
        } else {
            warn("indirect = 0, index = %d\n", index);
        }
        
        return 0;
    }
    index -= SFS_BLK_N_ENTRY;
    if ((indirect = disk_inode->db_indirect) != 0) {
        if ((ret = sfs_block_get_indirect_nolock(sfs, &indirect, index / SFS_BLK_N_ENTRY, 0, &blkno)) != 0) {
            return ret;
        }
        if ((indirect = blkno) != 0) {
            if ((ret = sfs_block_free_indirect_nolock(sfs, indirect, index % SFS_BLK_N_ENTRY)) != 0) {
                return ret;
            }
        }
    }
    return 0;
}

static int sfs_block_load_nolock(SfsFs *sfs, SfsInode *sfs_inode, uint32_t index, uint32_t *blkno_store) {
    SfsDiskInode *disk_inode = sfs_inode->disk_inode;
    // index == disk_inode->blocks表示要创建一个新的block
    assert(index <= disk_inode->blocks);
    int ret;
    uint32_t blkno;
    bool create = (index == disk_inode->blocks);
    if ((ret = sfs_block_get_nolock(sfs, sfs_inode, index, create, &blkno)) != 0) {
        return ret;
    }
    assert(sfs_block_inuse(sfs, blkno));
    if (create) {
        disk_inode->blocks++;
    }
    if (blkno_store != NULL) {
        *blkno_store = blkno;
    }
    return 0;
}

static int sfs_block_truncate_nolock(SfsFs *sfs, SfsInode *sfs_inode) {
    SfsDiskInode *disk_inode = sfs_inode->disk_inode;
    assert(disk_inode->blocks != 0);
    int ret;
    if ((ret = sfs_block_free_nolock(sfs, sfs_inode, disk_inode->blocks - 1)) != 0) {
        return ret;
    }
    disk_inode->blocks--;
    sfs_inode->dirty = true;
    return 0;
}

static int sfs_dirent_read_nolock(SfsFs *sfs, SfsInode *sfs_inode, int slot, SfsDiskEntry *entry) {
    assert(sfs_inode->disk_inode->type == SFS_TYPE_DIR);
    assert(slot < sfs_inode->disk_inode->blocks);
    int ret;
    uint32_t blkno;
    if ((ret = sfs_block_load_nolock(sfs, sfs_inode, slot, &blkno)) != 0) {
        return ret;
    }
    assert(sfs_block_inuse(sfs, blkno));
    if ((ret = sfs_rbuf(sfs, entry, sizeof(SfsDiskEntry), blkno, 0)) != 0) {
        return ret;
    }
    entry->name[SFS_MAX_FNAME_LEN] = '\0';
    return 0;
}

// 在sfs文件系统中，一个block用来存放一个disk_entry（好浪费啊！！！）
static int sfs_dirent_search_nolock(SfsFs *sfs, SfsInode *sfs_inode, const char *name, uint32_t *blkno_store, int *slot, int *empty_slot) {
    assert(strlen(name) <= SFS_MAX_FNAME_LEN);
    SfsDiskEntry *entry = NULL;
    if ((entry = kmalloc(sizeof(SfsDiskEntry))) == NULL) {
        return -E_NO_MEM;
    }

#define set_pvalue(x, v)    do { if ((x) != NULL) { *(x) = (v); } } while (0)

    int ret, i;
    int nslots = sfs_inode->disk_inode->blocks;
    set_pvalue(empty_slot, nslots);
    for (i = 0; i < nslots; i++) {
        if ((ret = sfs_dirent_read_nolock(sfs, sfs_inode, i, entry)) != 0) {
            goto out;
        }
        if (entry->ino == 0) {
            // 该目录项的inode索引为0，说明是一个空目录项，可以在新建立文件或目录时使用
            set_pvalue(empty_slot, i);
            continue;
        }
        if (strcmp(name, entry->name) == 0) {
            // 找到了name这个目录项
            set_pvalue(slot, i);
            set_pvalue(blkno_store, entry->ino);
            goto out;
        }
    }
#undef set_pvalue
    ret = -E_NOENT;
out:
    kfree(entry);
    return ret;
}

static int sfs_dirent_find_inode_nolock(SfsFs *sfs, SfsInode *sfs_inode, uint32_t blkno, SfsDiskEntry *entry) {
    int ret, i;
    int nslots = sfs_inode->disk_inode->blocks;
    for (i = 0; i < nslots; i++) {
        if ((ret = sfs_dirent_read_nolock(sfs, sfs_inode, i, entry)) != 0) {
            return ret;
        }
        if (entry->ino == blkno) {
            return 0;
        }
    }
    return -E_NOENT;
}

static int sfs_load_parent(SfsFs *sfs, SfsInode *sfs_inode, Inode **parent_store) {
    return sfs_load_inode(sfs, parent_store, sfs_inode->disk_inode->dir_info.parent);
}

static int sfs_lookup_once(SfsFs *sfs, SfsInode *sfs_inode, const char *name, Inode **node_store, int *slot) {
    int ret;
    if ((ret = trylock_sfs_inode(sfs_inode)) != 0) {
        return ret;
    }
    uint32_t blkno;
    ret = sfs_dirent_search_nolock(sfs, sfs_inode, name, &blkno, slot, NULL);
    unlock_sfs_inode(sfs_inode);
    if (ret != 0) {
        return ret;
    }
    return sfs_load_inode(sfs, node_store, blkno);
}

static int sfs_open_dir(Inode *node, uint32_t open_flags) {
    switch(open_flags & O_ACCMODE) {
        case O_RDONLY:
            break;
        case O_WRONLY:
        case O_RDWR:
        default:
            return -E_ISDIR;
    }
    if (open_flags & O_APPEND) {
        return -E_ISDIR;
    }
    return 0;
}

static int sfs_open_file(Inode *node, uint32_t open_flags) {
    return 0;
}

static int sfs_close(Inode *node) {
    return vop_fsync(node);
}

static int sfs_io_nolock(SfsFs *sfs, SfsInode *sfs_inode, void *buf, off_t offset, size_t *alenp, bool write) {
    SfsDiskInode *disk_inode = sfs_inode->disk_inode;
    assert(disk_inode->type != SFS_TYPE_DIR);
    off_t blkoff;
    off_t end_pos = offset + *alenp;
    *alenp = 0;
    if (offset < 0 || offset >= SFS_MAX_FILE_SIZE || offset > end_pos) {
        return -E_INVAL;
    }
    if (offset == end_pos) {
        return 0;
    }
    if (end_pos > SFS_MAX_FILE_SIZE) {
        end_pos = SFS_MAX_FILE_SIZE;
    }

    if (!write) {
        if (offset >= disk_inode->fileinfo.size) {
            return 0;
        }
        if (end_pos > disk_inode->fileinfo.size) {
            end_pos = disk_inode->fileinfo.size;
        }
    }

    int (*sfs_buf_op)(SfsFs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset);
    int (*sfs_block_op)(SfsFs *sfs, void *buf, uint32_t blkno, uint32_t nblks);
    if (write) {
        sfs_buf_op = sfs_wbuf;
        sfs_block_op = sfs_wblock;
    } else {
        sfs_buf_op = sfs_rbuf;
        sfs_block_op = sfs_rblock;
    }

    int ret = 0;
    size_t size;
    size_t alen = 0;
    uint32_t ino;
    uint32_t index = offset / SFS_BLK_SIZE;
    uint32_t nblks = end_pos / SFS_BLK_SIZE - index;

    if ((blkoff = offset % SFS_BLK_SIZE) != 0) {
        size = (nblks != 0) ? (SFS_BLK_SIZE - blkoff) : (end_pos - offset);
        if ((ret = sfs_block_load_nolock(sfs, sfs_inode, index, &ino)) != 0) {
            goto out;
        }
        if ((ret = sfs_buf_op(sfs, buf, size, ino, blkoff)) != 0) {
            goto out;
        }
        alen += size;
        if (nblks == 0) {
            goto out;
        }
        buf += size;
        index++;
        nblks--;
    }
    size = SFS_BLK_SIZE;
    while (nblks != 0) {
        if ((ret = sfs_block_load_nolock(sfs, sfs_inode, index, &ino)) != 0) {
            goto out;
        }
        if ((ret = sfs_block_op(sfs, buf, ino, 1)) != 0) {
            goto out;
        }
        alen += size;
        buf += size;
        index++;
        nblks--;
    }

    if ((size = end_pos % SFS_BLK_SIZE) != 0) {
        if ((ret = sfs_block_load_nolock(sfs, sfs_inode, index, &ino)) != 0) {
            goto out;
        }
        if ((ret = sfs_buf_op(sfs, buf, size, ino, 0)) != 0) {
            goto out;
        }
        alen += size;
    }

out:
    *alenp = alen;
    if (offset + alen > disk_inode->fileinfo.size) {
        disk_inode->fileinfo.size = offset + alen;
        sfs_inode->dirty = true;
    }
    return ret;
}

static inline int sfs_io(Inode *node, IOBuf *iob, bool write) {
    SfsFs *sfs = fsop_info(vop_fs(node), sfs);
    SfsInode *sfs_inode = vop_info(node, sfs_inode);
    int ret;
    if ((ret = trylock_sfs_inode(sfs_inode)) != 0) {
        return ret;
    }
    size_t alen = iob->io_resid;
    ret = sfs_io_nolock(sfs, sfs_inode, iob->io_base, iob->io_offset, &alen, write);
    if (alen != 0) {
        iobuf_skip(iob, alen);
    }
    unlock_sfs_inode(sfs_inode);
    return ret;
}

static int sfs_read(Inode *node, IOBuf *iob) {
    return sfs_io(node, iob, 0);
}

static int sfs_write(Inode *node, IOBuf *iob) {
    return sfs_io(node, iob, true);
}

static int sfs_fstat(Inode *node, Stat *stat) {
    int ret;
    memset(stat, 0, sizeof(Stat));
    if ((ret = vop_gettype(node, &(stat->st_mode))) != 0) {
        return ret;
    }
    SfsDiskInode *disk_inode = vop_info(node, sfs_inode)->disk_inode;
    stat->st_nlinks = disk_inode->nlinks;
    stat->st_blocks = disk_inode->blocks;
    if (disk_inode->type != SFS_TYPE_DIR) {
        stat->st_size = disk_inode->fileinfo.size;
    } else {
        // 加2是因为加上了当前目录'.'和父目录'..'这个
        stat->st_size = (disk_inode->dir_info.slots + 2) * sfs_dentry_size;
    }
    return 0;
}

static int sfs_fsync(Inode *node) {
    SfsFs *sfs = fsop_info(vop_fs(node), sfs);
    SfsInode *sfs_inode = vop_info(node, sfs_inode);
    if (sfs_inode->disk_inode->nlinks == 0 || !(sfs_inode->dirty)) {
        return 0;
    }
    int ret;
    if ((ret = trylock_sfs_inode(sfs_inode)) != 0) {
        return ret;
    }
    if (sfs_inode->dirty) {
        sfs_inode->dirty = false;
        if ((ret = sfs_wbuf(sfs, sfs_inode->disk_inode, sizeof(SfsDiskInode), sfs_inode->ino, 0)) != 0) {
            sfs_inode->dirty = true;
        }
    }
    unlock_sfs_inode(sfs_inode);
    return ret;
}

// 获取node这个文件的完整路径名（从根目录开始的路径）
static int sfs_name_file(Inode *node, IOBuf *iob) {
    SfsDiskEntry *entry = NULL;
    // todo: io_resid为什么是小于等于2，不应该是1吗
    if (iob->io_resid <= 2 || (entry = kmalloc(sizeof(SfsDiskEntry))) == NULL) {
        return -E_NO_MEM;
    }
    SfsFs *sfs = fsop_info(vop_fs(node), sfs);
    SfsInode *sfs_inode = vop_info(node, sfs_inode);

    int ret;
    uint32_t ino;
    char *ptr = iob->io_base + iob->io_resid;
    size_t alen, resid = iob->io_resid - 2;
    
    vop_ref_inc(node);
    while ((ino = sfs_inode->ino) != SFS_ROOT_BLK_NO) {
        Inode *parent = NULL;
        if ((ret = sfs_load_parent(sfs, sfs_inode, &parent)) != 0) {
            goto failed;
        }
        vop_ref_dec(node);
        node = parent;
        sfs_inode = vop_info(node, sfs_inode);
        assert(ino != sfs_inode->ino && sfs_inode->disk_inode->type == SFS_TYPE_DIR);

        if ((ret = trylock_sfs_inode(sfs_inode)) != 0) {
            goto failed;
        }
        ret = sfs_dirent_find_inode_nolock(sfs, sfs_inode, ino, entry);
        unlock_sfs_inode(sfs_inode);

        if (ret != 0) {
            goto failed;
        }

        if ((alen = strlen(entry->name) + 1) > resid) {
            goto failed_nomem;
        }
        resid -= alen;
        ptr -= alen;
        memcpy(ptr, entry->name, alen - 1);
        ptr[alen - 1] = '/';
    }
    vop_ref_dec(node);
    alen = iob->io_resid - resid - 2;
    ptr = memmove(iob->io_base + 1, ptr, alen);
    ptr[-1] = '/';
    ptr[alen] = '\0';
    iobuf_skip(iob, alen);
    kfree(entry);
    return 0;

failed_nomem:
    ret = -E_NO_MEM;
failed:
    vop_ref_dec(node);
    kfree(entry);
    return ret;
}

static int sfs_get_dirent_sub_nolock(SfsFs *sfs, SfsInode *sfs_inode, int slot, SfsDiskEntry *entry) {
    int ret, i;
    int nslots = sfs_inode->disk_inode->blocks;
    for (i = 0; i < nslots; i++) {
        if ((ret = sfs_dirent_read_nolock(sfs, sfs_inode, i, entry)) != 0) {
            return ret;
        }
        if (entry->ino != 0) {
            if (slot == 0) {
                return 0;
            }
            slot--;
        }
    }
    return -E_NOENT;
}


static int sfs_get_dirent(Inode *node, IOBuf *iob) {
    SfsDiskEntry *entry = NULL;
    if ((entry = kmalloc(sizeof(SfsDiskEntry))) == NULL) {
        return -E_NO_MEM;
    }
    SfsFs *sfs = fsop_info(vop_fs(node), sfs);
    SfsInode *sfs_inode = vop_info(node, sfs_inode);
    off_t offset = iob->io_offset;
    if (offset < 0 || offset % sfs_dentry_size != 0) {
        kfree(entry);
        return -E_INVAL;
    }
    int ret;
    int slot = offset / sfs_dentry_size;
    if (slot >= sfs_inode->disk_inode->dir_info.slots + 2) {
        kfree(entry);
        return -E_NOENT;
    }
    switch (slot)
    {
    case 0:
        strcpy(entry->name, ".");
        break;
    case 1:
        strcpy(entry->name, "..");
        break;
    default:
        if ((ret = trylock_sfs_inode(sfs_inode)) != 0) {
            goto out;
        }
        ret = sfs_get_dirent_sub_nolock(sfs, sfs_inode, slot - 2, entry);
        unlock_sfs_inode(sfs_inode);
        if (ret != 0) {
            goto out;
        }
        break;
    }
    ret = iobuf_move(iob, entry->name, sfs_dentry_size, true, NULL);
out:
    kfree(entry);
    return ret;
}

static int sfs_reclaim(Inode *node) {
    SfsFs *sfs = fsop_info(vop_fs(node), sfs);
    SfsInode *sfs_inode = vop_info(node, sfs_inode);
    lock_sfs_fs(sfs);

    int ret = -E_BUSY;
    assert(sfs_inode->reclaim_count > 0);
    if ((--sfs_inode->reclaim_count) != 0) {
        goto failed_unlock;
    }
    assert(inode_ref_count(node) == 0 && inode_open_count(node) == 0);

    if (sfs_inode->disk_inode->nlinks == 0) {
        uint32_t nblks;
        for (nblks = sfs_inode->disk_inode->blocks; nblks != 0; nblks--) {
            sfs_block_truncate_nolock(sfs, sfs_inode);
        }
    } else if (sfs_inode->dirty) {
        if ((ret = vop_fsync(node)) != 0) {
            goto failed_unlock;
        }
    }
    sfs_remove_links(sfs_inode);
    unlock_sfs_fs(sfs);

    if (sfs_inode->disk_inode->nlinks == 0) {
        sfs_block_free(sfs, sfs_inode->ino);
        uint32_t indirect;
        if ((indirect = sfs_inode->disk_inode->indirect) != 0) {
            sfs_block_free(sfs, indirect);
        }
        if ((indirect = sfs_inode->disk_inode->db_indirect) != 0) {
            int i;
            for (i = 0; i < SFS_BLK_N_ENTRY; i++) {
                sfs_block_free_indirect_nolock(sfs, indirect, i);
            }
            sfs_block_free(sfs, indirect);
        }
    }
    kfree(sfs_inode->disk_inode);
    vop_kill(node);
    return 0;
failed_unlock:
    unlock_sfs_fs(sfs);
    return ret;
}

static int sfs_get_type(Inode *node, uint32_t *type_store) {
    SfsDiskInode *disk_inode = vop_info(node, sfs_inode)->disk_inode;
    switch (disk_inode->type)
    {
    case SFS_TYPE_DIR:
        *type_store = S_IFDIR;
        return 0;
    case SFS_TYPE_FILE:
        *type_store = S_IFREG;
        return 0;
    case SFS_TYPE_LINK:
        *type_store = S_IFLNK;
        return 0;
    }
    panic("invalid file type %d.\n", disk_inode->type);

}

static int sfs_try_seek(Inode *node, off_t pos) {
    if (pos < 0 || pos >= SFS_MAX_FILE_SIZE) {
        return -E_INVAL;
    }
    SfsDiskInode *disk_inode = vop_info(node, sfs_inode)->disk_inode;
    if (pos > disk_inode->fileinfo.size) {
        return vop_truncate(node, pos);
    }
    return 0;
}

static int sfs_truncate_file(Inode *node, off_t len) {
    if (len < 0 || len > SFS_MAX_FILE_SIZE) {
        return -E_INVAL;
    }
    SfsFs *sfs = fsop_info(vop_fs(node), sfs);
    SfsInode *sfs_inode = vop_info(node, sfs_inode);
    SfsDiskInode *disk_inode = sfs_inode->disk_inode;
    assert(disk_inode->type != SFS_TYPE_DIR);

    int ret = 0;
    uint32_t nblks;
    uint32_t tblks = ROUNDUP_DIV(len, SFS_BLK_SIZE);
    if (disk_inode->fileinfo.size == len) {
        assert(tblks == disk_inode->blocks);
        return 0;
    }
    nblks = disk_inode->blocks;
    if (nblks < tblks) {
        while (nblks != tblks) {
            if ((ret = sfs_block_load_nolock(sfs, sfs_inode, nblks, NULL)) != 0) {
                goto out_unlock;
            }
            nblks++;
        }
    } else if (tblks < nblks) {
        while (tblks != nblks) {
            if ((ret = sfs_block_truncate_nolock(sfs, sfs_inode)) != 0) {
                goto out_unlock;
            }
            nblks--;
        }
    }
    assert(disk_inode->blocks == tblks);
    disk_inode->fileinfo.size = len;
    sfs_inode->dirty = true;

out_unlock:
    unlock_sfs_inode(sfs_inode);
    return ret;
}

static char *sfs_lookup_sub_path(char *path) {
    if ((path = strchr(path, '/')) != NULL) {
        while (*path == '/') {
            *path ++ = '\0';
        }
        if (*path == '\0') {
            return NULL;
        }
    }
    return path;
}

static int sfs_lookup(Inode *node, char *path, Inode **node_store) {
    SfsFs *sfs = fsop_info(vop_fs(node), sfs);
    assert(*path != '\0' && *path != '/');
    vop_ref_inc(node);
    do {
        SfsInode *sfs_inode = vop_info(node, sfs_inode);
        if (sfs_inode->disk_inode->type != SFS_TYPE_DIR) {
            vop_ref_dec(node);
            return -E_NOTDIR;
        }
        char *sub_path = NULL;
    next:
        sub_path = sfs_lookup_sub_path(path);
        if (strcmp(path, ".") == 0) {
            if ((path = sub_path) != NULL) {
                goto next;
            }
            break;
        }

        int ret;
        Inode *sub_node = NULL;
        if (strcmp(path, "..") == 0) {
            ret = sfs_load_parent(sfs, sfs_inode, &sub_node);
        } else {
            if (strlen(path) > SFS_MAX_FNAME_LEN) {
                vop_ref_dec(node);
                return -E_TOO_BIG;
            }
            ret = sfs_lookup_once(sfs, sfs_inode, path, &sub_node, NULL);
        }
        vop_ref_dec(node);
        if (ret != 0) {
            return ret;
        }
        node = sub_node;
        path = sub_path;
    } while (path != NULL);
    *node_store = node;
    return 0;
}

static const struct inode_ops sfs_node_dir_ops = {
    .vop_magic                      = VOP_MAGIC,
    .vop_open                       = sfs_open_dir,
    .vop_close                      = sfs_close,
    .vop_read                       = NULL_VOP_ISDIR,
    .vop_write                      = NULL_VOP_ISDIR,
    .vop_fstat                      = sfs_fstat,
    .vop_fsync                      = sfs_fsync,
    .vop_mkdir                      = NULL,
    .vop_link                       = NULL,
    .vop_rename                     = NULL,
    .vop_readlink                   = NULL_VOP_ISDIR,
    .vop_symlink                    = NULL_VOP_UNIMP,
    .vop_namefile                   = sfs_name_file,
    .vop_getdirentry                = sfs_get_dirent,
    .vop_reclaim                    = sfs_reclaim,
    .vop_ioctl                      = NULL_VOP_INVAL,
    .vop_gettype                    = sfs_get_type,
    .vop_tryseek                    = NULL_VOP_ISDIR,
    .vop_truncate                   = NULL_VOP_ISDIR,
    .vop_create                     = NULL,
    .vop_unlink                     = NULL,
    .vop_lookup                     = sfs_lookup,
    .vop_lookup_parent              = NULL,
};

static const struct inode_ops sfs_node_file_ops = {
    .vop_magic                      = VOP_MAGIC,
    .vop_open                       = sfs_open_file,
    .vop_close                      = sfs_close,
    .vop_read                       = sfs_read,
    .vop_write                      = sfs_write,
    .vop_fstat                      = sfs_fstat,
    .vop_fsync                      = sfs_fsync,
    .vop_mkdir                      = NULL_VOP_NOTDIR,
    .vop_link                       = NULL_VOP_NOTDIR,
    .vop_rename                     = NULL_VOP_NOTDIR,
    .vop_readlink                   = NULL_VOP_NOTDIR,
    .vop_symlink                    = NULL_VOP_NOTDIR,
    .vop_namefile                   = NULL_VOP_NOTDIR,
    .vop_getdirentry                = NULL_VOP_NOTDIR,
    .vop_reclaim                    = sfs_reclaim,
    .vop_ioctl                      = NULL_VOP_INVAL,
    .vop_gettype                    = sfs_get_type,
    .vop_tryseek                    = sfs_try_seek,
    .vop_truncate                   = sfs_truncate_file,
    .vop_create                     = NULL_VOP_NOTDIR,
    .vop_unlink                     = NULL_VOP_NOTDIR,
    .vop_lookup                     = NULL_VOP_NOTDIR,
    .vop_lookup_parent              = NULL_VOP_NOTDIR,
};

