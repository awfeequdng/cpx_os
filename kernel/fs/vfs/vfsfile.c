#include <types.h>
#include <vfs.h>
#include <string.h>
#include <error.h>
#include <unistd.h>
#include <assert.h>
#include <inode.h>
#include <iobuf.h>

int vfs_open(char *path, uint32_t open_flags, Inode **node_store) {
    bool can_write = false;
    switch (open_flags & O_ACCMODE) {
        case O_RDONLY:
            break;
        case O_WRONLY:
        case O_RDWR:
            can_write = 1;
            break;
        default:
            return -E_INVAL;
    }
    if (open_flags & O_TRUNC) {
        if (!can_write) {
            return -E_INVAL;
        }
    }

    int ret;
    Inode *dir = NULL;
    Inode *node = NULL;
    if (open_flags & O_CREAT) {
        char *name = NULL;
        bool excl = ((open_flags & O_EXCL) != 0);
        // todo: 为什么要lookup parent
        if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0) {
            return ret;
        }
        ret = vop_create(dir, name, excl, &node);
        // todo: 为什么要减1
        vop_ref_dec(dir);
    } else {
        ret = vfs_lookup(path, &node);
    }

    if (ret != 0) {
        return ret;
    } 
    assert(node != NULL);
    if ((ret = vop_open(node, open_flags)) != 0) {
        vop_ref_dec(node);
        return ret;
    }

    vop_open_inc(node);
    if (open_flags & O_TRUNC) {
        if ((ret = vop_truncate(node, 0)) != 0) {
            vop_open_dec(node);
            vop_ref_dec(node);
        }
    }
    assert(node_store != NULL);
    *node_store = node;
    return 0;
}

int vfs_close(Inode *node) {
    // todo:
    vop_open_dec(node);
    vop_ref_dec(node);
    return 0;
}

// todo: 这个函数是用来干嘛的？
int vfs_unlink(char *path) {
    int ret;
    char *name = NULL;
    Inode *dir = NULL;
    if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0) {
        return ret;
    }
    ret = vop_unlink(dir, name);
    vop_ref_dec(dir);
    return ret;
}

int vfs_rename(char *old_path, char *new_path) {
    int ret;
    char *old_name = NULL;
    char *new_name = NULL;
    Inode *old_dir = NULL;
    Inode *new_dir = NULL;
    if ((ret = vfs_lookup_parent(old_path, &old_dir, &old_name)) != 0) {
        return ret;
    }
    if ((ret = vfs_lookup_parent(new_path, &new_dir, &new_name)) != 0) {
        vop_ref_dec(old_dir);
        return ret;
    }

    // 重命名需要相同文件系统下面进行，不能夸文件系统
    if (old_dir->in_fs == NULL || old_dir->in_fs != new_dir->in_fs) {
        ret = -E_XDEV;
    } else {
        ret = vop_rename(old_dir, old_name, new_dir, new_name);
    }
    vop_ref_dec(old_dir);
    vop_ref_dec(new_dir);
    return ret;
}

int vfs_link(char *old_path, char *new_path) {
    int ret;
    char *new_name = NULL;
    Inode *old_node = NULL;
    Inode *new_dir = NULL;
    if ((ret = vfs_lookup(old_path, &old_node)) != 0) {
        return ret;
    }
    if ((ret = vfs_lookup_parent(new_path, &new_dir, &new_name)) != 0) {
        vop_ref_dec(old_node);
        return ret;
    }

    if (old_node->in_fs == NULL || old_node->in_fs != new_dir->in_fs) {
        ret = -E_XDEV;
    } else {
        ret = vop_link(new_dir, new_name, old_node);
    }

    vop_ref_dec(old_node);
    vop_ref_dec(new_dir);
    return ret;
}

int vfs_symlink(char *old_path, char *new_path) {
    int ret;
    char *new_name = NULL;
    Inode *new_dir;
    if ((ret = vfs_lookup_parent(new_path, &new_dir, &new_name)) != 0) {
        return ret;
    }
    ret = vop_symlink(new_dir, new_name, old_path);
    vop_ref_dec(new_dir);
    return ret;
}

int vfs_readlink(char *path, IOBuf *iob) {
    int ret;
    Inode *node = NULL;
    if ((ret = vfs_lookup(path, &node)) != 0) {
        return ret;
    }
    ret = vop_readlink(node, iob);
    vop_ref_dec(node);
    return ret;
}

int vfs_mkdir(char *path) {
    int ret;
    char *name = NULL;
    Inode *dir = NULL;
    if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0) {
        return ret;
    }
    ret = vop_mkdir(dir, name);
    vop_ref_dec(dir);
    return ret;
}