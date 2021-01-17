#ifndef __KERNEL_FS_DEVS_DEV_H__
#define __KERNEL_FS_DEVS_DEV_H__

#include <types.h>

struct inode;
struct iobuf;

typedef struct device {
    size_t d_blocks;
    size_t d_blocksize;
    int (*d_open)(struct device *dev, uint32_t open_flags);
    int (*d_close)(struct device *dev);
    int (*d_io)(struct device *dev, struct iobuf *iob, bool write);
    int (*d_ioctl)(struct device *dev, int op, void *data);
} Device;

#define dop_open(dev, open_flags)       ((dev)->d_open(dev, open_flags))
#define dop_close(dev)                  ((dev)->d_close(dev))
#define dop_io(dev, iob, write)         ((dev)->d_io(dev, iob, write))
#define dop_ioctl(dev, op, data)        ((dev)->d_ioctl(dev, op, data))

void dev_init(void);
struct inode *dev_create_inode(void);

#endif // __KERNEL_FS_DEVS_DEV_H__
