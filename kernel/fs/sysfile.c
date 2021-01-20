#include <sysfile.h>
#include <process.h>
#include <slab.h>
#include <fs.h>
#include <vfs.h>
#include <error.h>
#include <vmm.h>
#include <file.h>
#include <iobuf.h>
#include <dirent.h>

#define IOBUF_SIZE          4096

static int copy_path(char **to, const char *from) {
    MmStruct *mm = current->mm;
    char *buffer = NULL;
    if ((buffer = kmalloc(FS_MAX_FPATH_LEN + 1)) == NULL) {
        return -E_NO_MEM;
    }
    lock_mm(mm);
    if (!copy_string(mm, buffer, from, FS_MAX_FPATH_LEN + 1)) {
        unlock_mm(mm);
        goto failed_cleanup;
    }
    unlock_mm(mm);
    *to = buffer;
    return 0;

failed_cleanup:
    kfree(buffer);
    return -E_INVAL;
}

int sysfile_open(const char *__path, uint32_t open_flags) {
    int ret;
    char *path = NULL;
    if ((ret = copy_path(&path, __path)) != 0) {
        return ret;
    }
    ret = file_open(path, open_flags);
    kfree(path);
    return ret;
}

int sysfile_close(int fd) {
    return file_close(fd);
}

int sysfile_read(int fd, void *base, size_t len) {
    MmStruct *mm = current->mm;
    if (len == 0) {
        return 0;
    }
    if (!file_testfd(fd, true, false)) {
        return -E_INVAL;
    }
    void *buffer;
    if ((buffer = kmalloc(IOBUF_SIZE)) == NULL) {
        return -E_NO_MEM;
    }

    int ret = 0;
    size_t copied = 0;
    size_t alen;
    while (len != 0) {
        if ((alen = IOBUF_SIZE) > len) {
            alen = len;
        }
        ret = file_read(fd, buffer, alen, &alen);
        if (alen != 0) {
            lock_mm(mm);
            {
                if (copy_to_user(mm, base, buffer, alen)) {
                    assert(len >= alen);
                    base += alen;
                    len -= alen;
                    copied += alen;
                } else {
                    ret = -E_INVAL;
                }
            }
            unlock_mm(mm);
        }
        if (ret != 0 || alen == 0) {
            goto out;
        }
    }

out:
    kfree(buffer);
    if (copied != 0) {
        return copied;
    }
    // todo: 如果ret为error时呢？其中拷贝了一部分，然后后面一部分出错了呢？
    return ret;
}


int sysfile_write(int fd, void *base, size_t len) {
    MmStruct *mm = current->mm;
    if (len == 0) {
        return 0;
    }
    // todo: 可写的也可以读啊？
    if (!file_testfd(fd, false, true)) {
        return -E_INVAL;
    }
    void *buffer;
    if ((buffer = kmalloc(IOBUF_SIZE)) == NULL) {
        return -E_NO_MEM;
    }

    int ret = 0;
    size_t copied = 0, alen;
    while (len != 0) {
        if ((alen = IOBUF_SIZE) > len) {
            alen = len;
        }
        lock_mm(mm);
        {
            if (!copy_from_user(mm, buffer, base, alen, 0)) {
                ret = -E_INVAL;
            }
        }
        unlock_mm(mm);
        if (ret == 0) {
            ret = file_write(fd, buffer, alen, &alen);
            if (alen != 0) {
                assert(len >= alen);
                base += alen;
                len -= alen;
                copied += alen;
            }
        }
        if (ret != 0 || alen == 0) {
            goto out;
        }
    }

out:
    kfree(buffer);
    if (copied != 0) {
        return copied;
    }
    return ret;
}

int sysfile_seek(int fd, off_t pos, int whence) {
    return file_seek(fd, pos, whence);
}


int sysfile_fstat(int fd, Stat*__stat) {
    MmStruct *mm = current->mm;
    int ret;
    Stat __local_stat, *stat = &__local_stat;
    if ((ret = file_fstat(fd, stat)) != 0) {
        return ret;
    }

    lock_mm(mm);
    {
        if (!copy_to_user(mm, __stat, stat, sizeof(Stat))) {
            ret = -E_INVAL;
        }
    }
    unlock_mm(mm);
    return ret;
}


int sysfile_fsync(int fd) {
    return file_fsync(fd);
}

int sysfile_chdir(const char *__path) {
    int ret;
    char *path;
    if ((ret = copy_path(&path, __path)) != 0) {
        return ret;
    }
    ret = vfs_chdir(path);
    kfree(path);
    return ret;
}


int sysfile_get_cwd(char *buf, size_t len) {
    MmStruct *mm = current->mm;
    if (len == 0) {
        return -E_INVAL;
    }

    int ret = -E_INVAL;
    lock_mm(mm);
    {
        if (user_mem_check(mm, (uintptr_t)buf, len, true)) {
            IOBuf __iob;
            IOBuf *iob = iobuf_init(&__iob, buf, len, 0);
            ret = vfs_getcwd(iob);
        }
    }
    unlock_mm(mm);
    return 0;
}

int sysfile_get_dirent(int fd, DirectoryEntry *__direntp) {
    MmStruct *mm = current->mm;
    DirectoryEntry *direntp = NULL;
    if ((direntp = kmalloc(sizeof(DirectoryEntry))) == NULL) {
        return -E_NO_MEM;
    }

    int ret = 0;
    lock_mm(mm);
    {
        if (!copy_from_user(mm, &(direntp->offset), &(__direntp->offset), sizeof(direntp->offset), true)) {
            ret = -E_INVAL;
        }
    }
    unlock_mm(mm);

    if (ret != 0 || (ret = file_get_dirent(fd, direntp)) != 0) {
        goto out;
    }

    lock_mm(mm);
    {
        if (!copy_to_user(mm, __direntp, direntp, sizeof(DirectoryEntry))) {
            ret = -E_INVAL;
        }
    }
    unlock_mm(mm);

out:
    kfree(direntp);
    return ret;
}

int sysfile_dup(int fd1, int fd2) {
    return file_dup(fd1, fd2);
}