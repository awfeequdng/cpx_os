#include <sfs.h>

void lock_sfs_fs(SfsFs *sfs) {
    down(&(sfs->fs_sem));
}

void lock_sfs_io(SfsFs *sfs) {
    down(&(sfs->io_sem));
}

void lock_sfs_mutex(SfsFs *sfs) {
    down(&(sfs->mutex_sem));
}

void unlock_sfs_fs(SfsFs *sfs) {
    up(&(sfs->fs_sem));
}

void unlock_sfs_io(SfsFs *sfs) {
    up(&(sfs->io_sem));
}

void unlock_sfs_mutex(SfsFs *sfs) {
    up(&(sfs->mutex_sem));
}



