#include <types.h>
#include <dev.h>
#include <vfs.h>
#include <iobuf.h>
#include <error.h>
#include <assert.h>
#include <inode.h>

static int null_open(Device *dev, uint32_t open_flags) {
    return 0;
}

static int null_close(Device *dev) {
    return 0;
}

static int null_io(Device *dev, IOBuf *iob, bool write) {
    if (write) {
        iob->io_resid = 0;
    }
    return 0;
}

static int null_ioctl(Device *dev, int op, void *data) {
    return -E_INVAL;
}

static void null_device_init(Device *dev) {
    dev->d_blocks = 0;
    dev->d_blocksize = 1;
    dev->d_open = null_open;
    dev->d_close = null_close;
    dev->d_io = null_io;
    dev->d_ioctl = null_ioctl;
}

void dev_init_null(void) {
    Inode *node = NULL;
    if ((node = dev_create_inode()) == NULL) {
        panic("null: dev_create_node.\n");
    }
    null_device_init(vop_info(node, device));

    int ret;
    if ((ret = vfs_add_dev("null", node, 0)) != 0) {
        panic("null: vfs_add_dev: %e.\n", ret);
    }
}