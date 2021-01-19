#include <types.h>
#include <stdio.h>
#include <string.h>
#include <vfs.h>
#include <inode.h>
#include <dev.h>
#include <semaphore.h>
#include <list.h>
#include <slab.h>
#include <error.h>
#include <unistd.h>
#include <assert.h>
 
typedef struct vfs_device {
    const char *dev_name;
    Inode *dev_node;
    Fs *fs;
    bool mountable;
    ListEntry vdev_link;
} VfsDevice;

#define le2vdev(le, member)         \
    container_of((le), VfsDevice, member)

static ListEntry vdev_list;
static Semaphore vdev_list_sem;

static void lock_vdev_list(void) {
    down(&vdev_list_sem);
}

static void unlock_vdev_list(void) {
    up(&vdev_list_sem);
}

void vfs_dev_list_init(void) {
    list_init(&vdev_list);
    sem_init(&vdev_list_sem, 1);
}

// 清除所有的文件系统
void vfs_cleanup(void) {
    if (!list_empty(&vdev_list)) {
        lock_vdev_list();
        {
            ListEntry *head = &vdev_list;
            ListEntry *entry = head;
            while ((entry = list_next(entry)) != head) {
                VfsDevice *vdev = le2vdev(entry, vdev_link);
                if (vdev->fs != NULL) {
                    fsop_cleanup(vdev->fs);
                }
            }
        }
        unlock_vdev_list();
    }
}

int vfs_sync(void) {
    if (!list_empty(&vdev_list)) {
        lock_vdev_list();
        {
            ListEntry *head = &vdev_list;
            ListEntry *entry = head;
            while ((entry = list_next(entry)) != head) {
                VfsDevice *vdev = le2vdev(entry, vdev_link);
                if (vdev->fs != NULL) {
                    fsop_sync(vdev->fs);
                }
            }
        }
        unlock_vdev_list();
    }
    return 0;
}

int vfs_get_root(const char *dev_name, Inode **node_store) {
    assert(dev_name != NULL);
    int ret = -E_NO_DEV;
    if (!list_empty(&vdev_list)) {
        lock_vdev_list();
        {
            ListEntry *head = &vdev_list;
            ListEntry *entry = head;
            while ((entry = list_next(entry)) != head) {
                VfsDevice *vdev = le2vdev(entry, vdev_link);
                if (strcmp(dev_name, vdev->dev_name) == 0) {
                    Inode *found = NULL;
                    if (vdev->fs != NULL) {
                        found = fsop_get_root(vdev->fs);
                    } else if (!vdev->mountable) {
                        vop_ref_inc(vdev->dev_node);
                        found = vdev->dev_node;
                    }
                    if (found != NULL) {
                        ret = 0;
                        *node_store = found;
                    } else {
                        ret = -E_NA_DEV;
                    }
                    break;
                }
            }
        }
        unlock_vdev_list();
    }
    return ret;
}

const char *vfs_get_devname(Fs *fs) {
    assert(fs != NULL);
    ListEntry *head = &vdev_list;
    ListEntry *entry = head;
    while ((entry = list_next(entry)) != head) {
        VfsDevice *vdev = le2vdev(entry, vdev_link);
        if (vdev->fs == fs) {
            return vdev->dev_name;
        }
    }
    return NULL;
}

static bool check_devname_conflict(const char *dev_name) {
    ListEntry *head = &vdev_list;
    ListEntry *entry = head;
    while ((entry = list_next(entry)) != head) {
        VfsDevice *vdev = le2vdev(entry, vdev_link);
        if (strcmp(vdev->dev_name, dev_name) == 0) {
            return true;
        }
    }
    return false;
}


static int vfs_do_add(const char *dev_name, Inode *dev_node, Fs *fs, bool mountable) {
    assert(dev_name != NULL);
    assert((dev_node == NULL && !mountable) || (dev_node != NULL && check_inode_type(dev_node, device)));
    if (strlen(dev_name) > FS_MAX_DNAME_LEN) {
        return -E_TOO_BIG;
    }

    int ret = -E_NO_MEM;
    char *s_dev_name = NULL;
    if ((s_dev_name = strdup(dev_name)) == NULL) {
        return ret;
    }

    VfsDevice *vdev = NULL;
    if ((vdev = kmalloc(sizeof(VfsDevice))) == NULL) {
        goto failed_cleanup_name;
    }

    ret = -E_EXISTS;
    lock_vdev_list();
    if (check_devname_conflict(s_dev_name)) {
        unlock_vdev_list();
        goto failed_cleanup_vdev;
    }
    vdev->dev_name = s_dev_name;
    vdev->dev_node = dev_node;
    vdev->mountable = mountable;
    vdev->fs = fs;
    list_add(&vdev_list, &(vdev->vdev_link));
    unlock_vdev_list();
    return 0;
failed_cleanup_vdev:
    kfree(vdev);
failed_cleanup_name:
    kfree(s_dev_name);
    return ret;
}

int vfs_add_fs(const char *dev_name, Fs *fs) {
    return vfs_do_add(dev_name, NULL, fs, 0);
}

int vfs_add_dev(const char *dev_name, Inode *dev_node, bool mountable) {
    return vfs_do_add(dev_name, dev_node, NULL, mountable);
}

static int find_mount(const char *dev_name, VfsDevice **vdev_store) {
    assert(dev_name != NULL);
    ListEntry *head = &vdev_list;
    ListEntry *entry = head;
    while ((entry = list_next(entry)) != head) {
        VfsDevice *vdev = le2vdev(entry, vdev_link);
        if (vdev->mountable && strcmp(vdev->dev_name, dev_name) == 0) {
            *vdev_store = vdev;
            return 0;
        }
    }
    return -E_NO_DEV;
}

int vfs_mount(const char *dev_name, int (*mount_func)(Device *dev, Fs **fs_store)) {
    int ret;
    lock_vdev_list();
    VfsDevice *vdev = NULL;
    if ((ret = find_mount(dev_name, &vdev)) != 0) {
        // 设备没找到或者设备不能被挂载
        goto out;
    }
    if (vdev->fs != NULL) {
        // todo: 为什么fs必须等于NULL
        // 是不是在挂载后fs才会有值？？？
        ret = -E_BUSY;
        goto out;
    }
    // 设备必须要是能挂载的设备
    assert(vdev->dev_name != NULL && vdev->mountable);

    Device *dev = vop_info(vdev->dev_node, device);
    if ((ret = mount_func(dev, &(vdev->fs))) == 0) {
        // vdev 挂载后fs必须不为NULL
        assert(vdev->fs != NULL);
        printk("vfs: mount %s.\n", vdev->dev_name);
    }

out:
    unlock_vdev_list();
    return ret;
}

int vfs_unmount(const char *dev_name) {
    int ret;
    lock_vdev_list();
    VfsDevice *vdev = NULL;
    if ((ret = find_mount(dev_name, &vdev)) != 0) {
        goto out;
    }
    // 挂载后fs不为NULL
    if (vdev->fs == NULL) {
        ret = -E_INVAL;
        goto out;
    }
    assert(vdev->dev_name != NULL && vdev->mountable);

    if ((ret = fsop_sync(vdev->fs)) != 0) {
        goto out;
    }
    if ((ret = fsop_unmount(vdev->fs)) == 0) {
        vdev->fs = NULL;
        printk("vfs: unmount %s.\n", vdev->dev_name);
    }
out:
    unlock_vdev_list();
    return ret;
}

int vfs_unmount_all(void) {
    if (!list_empty(&vdev_list)) {
        lock_vdev_list();
        {
            ListEntry *head = &vdev_list;
            ListEntry *entry = head;
            while ((entry = list_next(entry)) != head) {
                VfsDevice *vdev = le2vdev(entry, vdev_link);
                if (vdev->mountable && vdev->fs != NULL) {
                    int ret;
                    if ((ret = fsop_sync(vdev->fs)) != 0) {
                        printk("vfs: warning: sync failed for %s: %e.\n", vdev->dev_name, ret);
                        continue;
                    }
                    if ((ret = fsop_unmount(vdev->fs)) != 0) {
                        printk("vfs: warning: unmount failed for %s: %e.\n", vdev->dev_name, ret);
                        continue;
                    }
                    vdev->fs = NULL;
                    printk("vfs: unmount %s.\n", vdev->dev_name);
                }
            }
        }
        unlock_vdev_list();
    }
    return 0;
}