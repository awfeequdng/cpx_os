#include <types.h>
#include <string.h>
#include <vfs.h>
#include <inode.h>
#include <iobuf.h>
#include <stat.h>
#include <process.h>
#include <error.h>
#include <assert.h>

static Inode *get_cwd_nolock(void) {
    return current->fs_struct->pwd;
}

static void set_cwd_nolock(Inode *pwd) {
    current->fs_struct->pwd = pwd;
}

static void lock_current_fs(void) {
    lock_fs(current->fs_struct);
}

static void unlock_current_fs(void) {
    unlock_fs(current->fs_struct);
}

int vfs_get_current_dir(Inode **dir_store) {
    Inode *node = NULL;
    if ((node = get_cwd_nolock()) != NULL) {
        vop_ref_inc(node);
        *dir_store = node;
        return 0;
    }
    return -E_NOENT;
}

int vfs_set_current_dir(Inode *dir) {
    int ret = 0;
    lock_current_fs();
    Inode *old_dir = NULL;
    if ((old_dir = get_cwd_nolock()) != dir) {
        if (dir != NULL) {
            uint32_t type;
            if ((ret = vop_gettype(dir, &type)) != 0) {
                goto out;
            }
            if (!S_ISDIR(type)) {
                ret = -E_NOTDIR;
                goto out;
            }
            vop_ref_inc(dir);
        }
        set_cwd_nolock(dir);
        if (old_dir != NULL) {
            vop_ref_dec(old_dir);
        }
    }
out:
    unlock_current_fs();
    return ret;
}

int vfs_chdir(char *path) {
    int ret;
    Inode *node = NULL;
    if ((ret = vfs_lookup(path, &node)) == 0) {
        ret = vfs_set_current_dir(node);
        vop_ref_dec(node);
    }
    return ret;
}

int vfs_getcwd(IOBuf *iob) {
    int ret;
    Inode *node = NULL;
    if ((ret = vfs_get_current_dir(&node)) != 0) {
        return ret;
    }
    assert(node->in_fs != NULL);
    const char *dev_name = vfs_get_devname(node->in_fs);
    if ((ret = iobuf_move(iob, (char *)dev_name, strlen(dev_name), true, NULL)) != 0) {
        goto out;
    }
    // 将路径信息拷贝到iob中
    ret = vop_namefile(node, iob);

out:
    vop_ref_dec(node);
    return ret;
}