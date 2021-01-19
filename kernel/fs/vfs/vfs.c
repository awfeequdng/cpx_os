#include <vfs.h>
#include <stdio.h>
#include <string.h>
#include <inode.h>
#include <semaphore.h>
#include <error.h>
#include <slab.h>

static Semaphore bootfs_sem;
static Inode *bootfs_node = NULL;

Fs *__alloc_fs(int type) {
    Fs *fs = NULL;
    if ((fs = kmalloc(sizeof(Fs))) != NULL) {
        fs->fs_type = type;
    }
    return fs;
}

void vfs_init(void) {
    sem_init(&bootfs_sem, 1);
    vfs_dev_list_init();
}

static void lock_bootfs(void) {
    down(&bootfs_sem);
}

static void unlock_bootfs(void) {
    up(&bootfs_sem);
}

static void change_bootfs(Inode *node) {
    Inode *old = NULL;
    lock_bootfs();
    {
        old = bootfs_node;
        bootfs_node = node;
    }
    unlock_bootfs();
    if (old != NULL) {
        vop_ref_dec(old);
    }
}

int vfs_set_bootfs(char *fsname) {
    Inode *node = NULL;
    if (fsname != NULL) {
        char *s;
        // todo: 这是什么意思？
        if ((s = strchr(fsname, ':')) == NULL || s[1] != 0) {
            return -E_INVAL;
        }
        int ret;
        if ((ret = vfs_chdir(fsname)) != 0) {
            return ret;
        }
        if ((ret = vfs_get_current_dir(&node)) != 0) {
            return ret;
        }
    }
    change_bootfs(node);
    return 0;
}

int vfs_get_bootfs(Inode **node_store) {
    Inode *node = NULL;
    if (bootfs_node != NULL) {
        lock_bootfs();
        {
            if ((node = bootfs_node) != NULL) {
                // todo: 为什么要在这儿加1，而不是在赋值后加1
                vop_ref_inc(bootfs_node);
            }
        }
        unlock_bootfs();
    }
    if (node == NULL) {
        return -E_NOENT;
    }
    assert(node_store != NULL);
    *node_store = node;
    return 0;
}