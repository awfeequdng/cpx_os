#include <types.h>
#include <string.h>
#include <dev.h>
#include <sfs.h>
#include <bitmap.h>
#include <assert.h>
#include <iobuf.h>
#include <inode.h>

// 从blkno处读取一个block的数据
static int sfs_rwblock_noblock(SfsFs *sfs, void *buf, uint32_t blkno, bool write, bool check) {
    assert((blkno != 0 || !check) && blkno < sfs->super.blocks);
    IOBuf __iob, *iob = iobuf_init(&__iob, buf, SFS_BLK_SIZE, blkno * SFS_BLK_SIZE);
    return dop_io(sfs->dev, iob, write);
}

static int sfs_rwblock(SfsFs *sfs, void *buf, uint32_t blkno, uint32_t nblks, bool write) {
    int ret = 0;
    lock_sfs_io(sfs);
    {
        while (nblks != 0) {
            if ((ret = sfs_rwblock_noblock(sfs, buf, blkno, write, true)) != 0) {
                break;
            }
            blkno++;
            nblks--;
            buf += SFS_BLK_SIZE;
        }
    }
    lock_sfs_io(sfs);
}

int sfs_rblock(SfsFs *sfs, void *buf, uint32_t blkno, uint32_t nblks) {
    return sfs_rwblock(sfs, buf, blkno, nblks, false);
}

int sfs_wblock(SfsFs *sfs, void *buf, uint32_t blkno, uint32_t nblks) {
    return sfs_rwblock(sfs, buf, blkno, nblks, true);
}

int sfs_rbuf(SfsFs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset) {
    assert(offset >= 0 && offset < SFS_BLK_SIZE && offset + len <= SFS_BLK_SIZE);
    int ret;
    lock_sfs_io(sfs);
    {
        if ((ret = sfs_rwblock_noblock(sfs, sfs->sfs_buffer, blkno, false, true)) == 0) {
            memcpy(buf, sfs->sfs_buffer + offset, len);
        }
    }
    unlock_sfs_io(sfs);
    return ret;
}

int sfs_wbuf(SfsFs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset) {
    assert(offset >= 0 && offset < SFS_BLK_SIZE && offset + len <= SFS_BLK_SIZE);
    int ret;
    lock_sfs_io(sfs);
    {   
        // 先读后写（保证不会覆盖非写区域内的数据）
        if ((ret = sfs_rwblock_noblock(sfs, sfs->sfs_buffer, blkno, false, true)) == 0) {
            memcpy(sfs->sfs_buffer + offset, buf, len);
            ret = sfs_rwblock_noblock(sfs, sfs->sfs_buffer, blkno, true, true);
        }
    }
    unlock_sfs_io(sfs);
    return ret;
}

int sfs_sync_super(SfsFs *sfs) {
    int ret;
    
    lock_sfs_io(sfs);
    {
        memset(sfs->sfs_buffer, 0, SFS_BLK_SIZE);
        memcpy(sfs->sfs_buffer, &(sfs->super), sizeof(sfs->super));
        ret = sfs_rwblock_noblock(sfs, sfs->sfs_buffer, SFS_SUPER_BLK_NO, true, false);
    }
    unlock_sfs_io(sfs);
    return ret;
}

int sfs_sync_freemap(SfsFs *sfs) {
    uint32_t nblks = sfs_freemap_blocks(&(sfs->super));
    return sfs_wblock(sfs, bitmap_get_data(sfs->freemap, NULL), SFS_FREEMAP_BLK_NO, nblks);
}

int sfs_clear_block(SfsFs *sfs, uint32_t blkno, uint32_t nblks) {
    int ret;
    lock_sfs_io(sfs);
    {
        memset(sfs->sfs_buffer, 0, SFS_BLK_SIZE);
        while (nblks != 0) {
            // 不能清除block 0，也就是超级块不能被清除
            if ((ret = sfs_rwblock_noblock(sfs, sfs->sfs_buffer, blkno, true, true)) != 0) {
                break;
            }
            blkno++;
            nblks--;
        }
    }
    unlock_sfs_io(sfs);
    return ret;
}
