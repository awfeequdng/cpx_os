#ifndef __KERNEL_FS_VFS_VFS_H__
#define __KERNEL_FS_VFS_VFS_H__

#include <types.h>
#include <fs.h>
#include <sfs.h>

struct inode;
struct device;
struct iobuf;

typedef struct fs {
    union {
        SfsFs __sfs_info;
    } fs_info;
    enum {
        fs_type_sfs_info = 0x5679,
    } fs_type;
    int (*fs_sync)(struct fs *fs);
    struct inode *(*fs_get_root)(struct fs *fs);
    int (*fs_unmount)(struct fs *fs);
    void (*fs_cleanup)(struct fs *fs);
} Fs;

#define __fs_type(type)             fs_type_##type##_info

#define check_fs_type(fs, type)     ((fs)->fs_type == __fs_type(type))

#define __fsop_info(_fs, type)      \
    ({                              \
        Fs *__fs = (_fs);             \
        assert(__fs != NULL && check_fs_type(__fs, type));    \
        &(__fs->fs_info.__##type##_info);       \
    })

#define fsop_info(fs, type)     __fsop_info(fs, type)

#define info2fs(info, type)     \
    container_of((info), Fs, fs_info.__##type##_info)

Fs *__alloc_fs(int type);

#define alloc_fs(type)          __alloc_fs(__fs_type(type))

#define fsop_sync(fs)          ((fs)->fs_sync(fs))
#define fsop_get_root(fs)      ((fs)->fs_get_root(fs))
#define fsop_unmount(fs)       ((fs)->fs_unmount(fs))
#define fsop_cleanup(fs)       ((fs)->fs_cleanup(fs))

void vfs_init(void);
void vfs_cleanup(void);
void vfs_dev_list_init(void);

// vfs 底层操作，直接操作inode
int vfs_set_current_dir(struct inode *dir);
int vfs_get_current_dir(struct inode **dir_store);
int vfs_sync(void);
int vfs_get_root(const char *devname, struct inode **root_store);
const char *vfs_get_devname(Fs *fs);

// vfs高层操作接口
int vfs_open(char *path, uint32_t open_flags, struct inode **inode_store);
int vfs_close(struct inode *node);
int vfs_link(char *old_path, char *new_path);
int vfs_symlink(char *old_path, char *new_path);
int vfs_readlink(char *path, struct iobuf *iob);
int vfs_mkdir(char *path);
int vfs_unlink(char *path);
int vfs_rename(char *old_path, char *new_path);
int vfs_chdir(char *path);
int vfs_getcwd(struct iobuf *iob);

int vfs_lookup(char *path, struct inode **node_store);
int vfs_lookup_parent(char *path, struct inode **node_store, char **ednp);


// vfs混杂操作
int vfs_set_bootfs(char *fsname);
int vfs_get_bootfs(struct inode **node_store);
int vfs_add_fs(const char *devname, struct fs *fs);
int vfs_add_dev(const char *devname, struct inode *devnode, bool mountable);
int vfs_mount(const char *devname, int (*mount_func)(struct device *dev, struct fs **fs_store));
int vfs_unmount(const char *devname);
int vfs_unmount_all(void);

#endif // __KERNEL_FS_VFS_VFS_H__