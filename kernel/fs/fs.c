#include <types.h>
#include <slab.h>
#include <semaphore.h>
#include <vfs.h>
#include <dev.h>
#include <file.h>
#include <inode.h>
#include <assert.h>

void fs_init(void) {
    vfs_init();
    dev_init();
}

void fs_cleanup(void) {
    vfs_cleanup();
}

void lock_fs(struct fs_struct *fs_struct) {
    down(&(fs_struct->fs_sem));
}

void unlock_fs(struct fs_struct *fs_struct) {
    up(&(fs_struct->fs_sem));
}

FsStruct *fs_create(void) {
    static_assert((int)FS_STRUCT_NENTRY > 128);
    FsStruct *fs_struct = NULL;
    if ((fs_struct = kmalloc(sizeof(FsStruct) + FS_STRUCT_BUFSIZE)) != NULL) {
        fs_struct->pwd = NULL;
        fs_struct->filemap = (void *)(fs_struct + 1);
        atomic_set(&(fs_struct->fs_count), 1);
        sem_init(&(fs_struct->fs_sem), 1);
        filemap_init(fs_struct->filemap);
    }
    return fs_struct;
}

void fs_destory(FsStruct *fs_struct) {
    assert(fs_struct != NULL && fs_count(fs_struct) == 0);
    if (fs_struct->pwd != NULL) {
        vop_ref_dec(fs_struct->pwd);
    }
    int i;
    File *file = fs_struct->filemap;
    for (i = 0; i < FS_STRUCT_NENTRY; i++) {
        if (file->status == FD_OPENED) {
            filemap_close(file);
        }
        assert(file->status == FD_NONE);
    }
    kfree(fs_struct);
}

int dup_fs(FsStruct *to, FsStruct *from) {
    assert(to != NULL && from != NULL);
    assert(fs_count(to) == 0 && fs_count(from) > 0);
    if ((to->pwd = from->pwd) != NULL) {
        vop_ref_inc(to->pwd);
    }
    int i;
    File *to_file = to->filemap;
    File *from_file = from->filemap;
    for (i = 0; i < FS_STRUCT_NENTRY; i++) {
        if (from_file->status == FD_OPENED) {
            to_file->status = FD_INIT;
            filemap_dup(to_file, from_file);
        }
    }
    return 0;
}