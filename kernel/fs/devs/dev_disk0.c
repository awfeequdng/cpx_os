#include <types.h>
#include <mmu.h>
#include <slab.h>
#include <semaphore.h>
#include <ide.h>
#include <inode.h>
#include <dev.h>
#include <vfs.h>
#include <iobuf.h>
#include <error.h>
#include <assert.h>

#define DISK0_BLK_SIZE                   PAGE_SIZE
#define DISK0_BUF_SIZE                   (4 * DISK0_BLK_SIZE)
#define DISK0_BLK_N_SECT                 (DISK0_BLK_SIZE / SECT_SIZE)

static char *disk0_buffer;
static Semaphore disk0_sem;

static void lock_disk0(void) {
    down(&(disk0_sem));
}

static void unlock_disk0(void) {
    up(&(disk0_sem));
}

static int disk0_open(Device *dev, uint32_t open_flags) {
    return 0;
}

static int disk0_close(Device *dev) {
    return 0;
}

static void disk0_read_blks_nolock(uint32_t blkno, uint32_t nblks) {
    int ret;
    uint32_t sect_no = blkno * DISK0_BLK_N_SECT;
    uint32_t nsecs = nblks * DISK0_BLK_N_SECT;
    if ((ret = ide_read_secs(DISK0_DEV_NO, sect_no, disk0_buffer, nsecs)) != 0) {
        panic("disk0: read blkno = %d (sectno = %d), nblks = %d (nsecs = %d): 0x%08x.\n",
                blkno, sect_no, nblks, nsecs, ret);
    }
}

static void disk0_write_blks_nolock(uint32_t blkno, uint32_t nblks) {
    int ret;
    uint32_t sect_no = blkno * DISK0_BLK_N_SECT;
    uint32_t nsecs = nblks * DISK0_BLK_N_SECT;
    if ((ret = ide_write_secs(DISK0_DEV_NO, sect_no, disk0_buffer, nsecs)) != 0) {
        panic("disk0: write blkno = %d (sectno = %d), nblks = %d (nsecs = %d): 0x%08x.\n",
                blkno, sect_no, nblks, nsecs, ret);
    }
}

static int disk0_io(Device *dev, IOBuf *iob, bool write) {
    off_t offset = iob->io_offset;
    size_t resid = iob->io_resid;
    uint32_t blkno = offset / DISK0_BLK_SIZE;
    uint32_t nblks = resid / DISK0_BLK_SIZE;

    // don't allow I/O that isn't block-aligned
    if ((offset % DISK0_BLK_SIZE) != 0 || (resid % DISK0_BLK_SIZE) != 0) {
        return -E_INVAL;
    }

    // don't allow I/O past the end of disk0
    if (blkno + nblks > dev->d_blocks) {
        return -E_INVAL;
    }

    // read/write nothing??
    if (nblks == 0) {
        return 0;
    }

    lock_disk0();
    while (resid != 0) {
        size_t copied, alen = DISK0_BUF_SIZE;
        if (write) {
            iobuf_move(iob, disk0_buffer, alen, false, &copied);
            assert(copied != 0 && copied <= resid && copied % DISK0_BLK_SIZE == 0);
            nblks = copied / DISK0_BLK_SIZE;
            disk0_write_blks_nolock(blkno, nblks);
        }
        else {
            if (alen > resid) {
                alen = resid;
            }
            nblks = alen / DISK0_BLK_SIZE;
            disk0_read_blks_nolock(blkno, nblks);
            iobuf_move(iob, disk0_buffer, alen, true, &copied);
            assert(copied == alen && copied % DISK0_BLK_SIZE == 0);
        }
        resid -= copied, blkno += nblks;
    }
    unlock_disk0();
    return 0;
}

static int disk0_ioctl(Device *dev, int op, void *data) {
    return -E_UNIMP;
}

static void disk0_device_init(Device *dev) {
    static_assert(DISK0_BLK_SIZE % SECT_SIZE == 0);
    if (!ide_device_valid(DISK0_DEV_NO)) {
        panic("disk0 device isn't available.\n");
    }
    dev->d_blocks = ide_device_size(DISK0_DEV_NO) / DISK0_BLK_N_SECT;
    dev->d_blocksize = DISK0_BLK_SIZE;
    dev->d_open = disk0_open;
    dev->d_close = disk0_close;
    dev->d_io = disk0_io;
    dev->d_ioctl = disk0_ioctl;
    sem_init(&(disk0_sem), 1);

    static_assert(DISK0_BUF_SIZE % DISK0_BLK_SIZE == 0);
    if ((disk0_buffer = kmalloc(DISK0_BUF_SIZE)) == NULL) {
        panic("disk0 alloc buffer failed.\n");
    }
}

void dev_init_disk0(void) {
    struct inode *node;
    if ((node = dev_create_inode()) == NULL) {
        panic("disk0: dev_create_node.\n");
    }
    disk0_device_init(vop_info(node, device));

    int ret;
    if ((ret = vfs_add_dev("disk0", node, true)) != 0) {
        panic("disk0: vfs_add_dev: %e.\n", ret);
    }
}

