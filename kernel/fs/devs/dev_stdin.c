#include <types.h>
#include <stdio.h>
#include <wait.h>
#include <sync.h>
#include <process.h>
#include <schedule.h>
#include <dev.h>
#include <vfs.h>
#include <iobuf.h>
#include <inode.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>
#include <queue.h>

#define STDIN_BUF_SIZE          4096

static uintptr_t stdin_buffer[STDIN_BUF_SIZE];
static Queue __stdin_que;
static Queue *que = &__stdin_que;

static WaitQueue __wait_queue, *wait_queue = &__wait_queue;

void dev_stdin_write(char c) {
    bool flag;
    if (c != '\0') {
        local_intr_save(flag);
        {
            queue_push(que, c);
            if (!wait_queue_empty(wait_queue)) {
                wakeup_queue(wait_queue, WT_KBD, true);
            }
        }
        local_intr_restore(flag);
    }
}

static int dev_stdin_read(char *buf, size_t len) {
    int ret = 0;
    bool flag;
    local_intr_save(flag);
    {
        for (; ret < len; ret++) {
        try_again:
            if (!queue_empty(que)) {
                *buf++ = queue_pop(que);
            } else {
                Wait __wait, *wait = &__wait;
                wait_current_set(wait_queue, wait, WT_KBD);
                local_intr_restore(flag);

                schedule();

                local_intr_save(flag);
                wait_current_del(wait_queue, wait);
                if (wait->wakeup_flags == WT_KBD) {
                    goto try_again;
                }
                warn("warn: wakeup by %d\n", wait->wakeup_flags);
                break;
            }
        }
    }
    local_intr_restore(flag);
    return ret;
}

static int stdin_open(Device *dev, uint32_t open_flags) {
    if (open_flags != O_RDONLY) {
        return -E_INVAL;
    }
    return 0;
}

static int stdin_close(Device *dev) {
    return 0;
}

static int stdin_io(Device *dev, IOBuf *iob, bool write) {
    if (!write) {
        int ret;
        if ((ret = dev_stdin_read(iob->io_base, iob->io_resid)) > 0) {
            iob->io_resid -= ret;
        }
        return ret;
    }
    return -E_INVAL;
}

static int stdin_ioctl(Device *dev, int op, void *data) {
    return -E_INVAL;
}

static void stdin_device_init(Device *dev) {
    dev->d_blocks = 0;
    dev->d_blocksize = 1;
    dev->d_open = stdin_open;
    dev->d_close = stdin_close;
    dev->d_io = stdin_io;
    dev->d_ioctl = stdin_ioctl;

    queue_init(que, stdin_buffer, ARRAY_SIZE(stdin_buffer));
    wait_queue_init(wait_queue);
}

void dev_init_stdin(void) {
    Inode *node = NULL;
    if ((node = dev_create_inode()) == NULL) {
        panic("stdin: dev_create_node.\n");
    }
    stdin_device_init(vop_info(node, device));

    int ret;
    if ((ret = vfs_add_dev("stdin", node, false)) != 0) {
        panic("stdin: vfs_add_dev: %e.\n", ret);
    }
}