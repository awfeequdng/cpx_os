#include <types.h>
#include <stdio.h>
#include <dev.h>
#include <vfs.h>
#include <iobuf.h>
#include <inode.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>

static int stdout_open(Device *dev, uint32_t open_flags) {
    if (open_flags != O_WRONLY) {
        return -E_INVAL;
    }
    return 0;
}

static int stdout_close(Device *dev) {
    return 0;
}

static int stdout_io(Device *dev, IOBuf *iob, bool write) {
    if (write) {
        char *data = iob->io_base;
        for (; iob->io_resid != 0; iob->io_resid--) {
            putchar(*data++);
        }
        return 0;
    }
    return -E_INVAL;
}

static int stdout_ioctl(Device *dev, int op, void *data) {
    return -E_INVAL;
}

static void stdout_device_init(Device *dev) {
    dev->d_blocks = 0;
    dev->d_blocksize = 1;
    dev->d_open = stdout_open;
    dev->d_close = stdout_close;
    dev->d_io = stdout_io;
    dev->d_ioctl = stdout_ioctl;
}

void dev_init_stdout(void) {
    Inode *node = NULL;
    if ((node = dev_create_inode()) == NULL) {
        panic("stdout: dev_create_inode.\n");
    }
    stdout_device_init(vop_info(node, device));

    int ret;
    if ((ret = vfs_add_dev("stdout", node, false)) != 0) {
        panic("stdout: vfs_add_dev: %e.\n", ret);
    }
}