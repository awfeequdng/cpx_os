#include <iobuf.h>
#include <inode.h>
#include <stat.h>
#include <error.h>
#include <assert.h>
#include <fs.h>
#include <process.h>
#include <file.h>
#include <unistd.h>
#include <vfs.h>
#include <dirent.h>

#define testfd(fd)      ((fd) >= 0 && (fd) < FS_STRUCT_NENTRY)

File *get_filemap(void) {
    FsStruct *fs_struct = current->fs_struct;
    assert(fs_struct != NULL && fs_count(fs_struct) > 0);
    return fs_struct->filemap;
}

void filemap_init(File *filemap) {
    int fd;
    File *file = filemap;
    for (fd = 0; fd < FS_STRUCT_NENTRY; fd++, file++) {
        atomic_set(&(file->open_count), 0);
        file->status = FD_NONE;
        file->fd = fd;
    }
}

static int filemap_alloc(int fd, File **file_store) {
    File *file = get_filemap();
    if (fd == NO_FD) {
        for (fd = 0; fd <FS_STRUCT_NENTRY; fd++, file++) {
            if (file->status == FD_NONE) {
                goto found;
            }
        }
        return -E_MAX_OPEN;
    } else {
        if (testfd(fd)) {
            file += fd;
            if (file->status == FD_NONE) {
                goto found;
            }
            return -E_BUSY;
        } else {
            return -E_INVAL;
        }
    }
found:
    assert(fopen_count(file) == 0);
    file->status = FD_INIT;
    file->node = NULL;
    if (file_store != NULL) {
        *file_store = file;
    }
    return 0;
}

static void filemap_free(File *file) {
    assert(file->status == FD_INIT || file->status == FD_CLOSED);
    assert(fopen_count(file) == 0);
    if (file->status == FD_CLOSED) {
        vfs_close(file->node);
    }
    file->status = FD_NONE;
}

static void filemap_acquire(File *file) {
    assert(file->status == FD_OPENED);
    fopen_count_inc(file);
}

static void filemap_release(File *file) {
    assert(file->status == FD_OPENED || file->status == FD_CLOSED);
    assert(fopen_count(file) > 0);
    if (fopen_count_dec(file) == 0) {
        filemap_free(file);
    }
}

void filemap_open(File *file) {
    assert(file->status == FD_INIT && file->node != NULL);
    file->status = FD_OPENED;
    fopen_count_inc(file);
}

void filemap_close(File *file) {
    assert(file->status == FD_OPENED);
    assert(fopen_count(file) > 0);
    file->status = FD_CLOSED;
    if (fopen_count_dec(file) == 0) {
        filemap_free(file);
    }
}

void filemap_dup(File *to, File *from) {
    assert(to->status == FD_INIT && from->status == FD_OPENED);
    to->pos = from->pos;
    to->readable = from->readable;
    to->writable = from->writable;
    Inode *node = from->node;
    // to 和 from指向同一个node，每个文件的元信息是相同的
    vop_ref_inc(node);
    vop_open_inc(node);
    to->node = node;
    filemap_open(to);
}

static inline int fd2file(int fd, File **file_store) {
    if (testfd(fd)) {
        assert(file_store != NULL);
        File *file = get_filemap() + fd;
        if (file->status == FD_OPENED && file->fd == fd) {
            *file_store = file;
            return 0;
        }
    }
    return -E_INVAL;
}

bool file_testfd(int fd, bool readable, bool writable) {
    int ret;
    File *file = NULL;
    if ((ret = fd2file(fd, &file)) != 0) {
        return false;
    }
    if (readable && !file->readable) {
        return false;
    }
    if (writable && !file->writable) {
        return false;
    }
    return true;
}

int file_open(char *path, uint32_t open_flags) {
    bool readable = false;
    bool writable = false;
    // O_RDONLY O_WRONLY O_RDWR
    switch (open_flags & O_ACCMODE) {
        case O_RDONLY:  
            readable = true;
            break;
        case O_WRONLY:
            writable = true;
            break;
        case O_RDWR:
            writable = true;
            readable = true;
            break;
        default:
            return -E_INVAL;
    }
    int ret;
    File *file = NULL;
    if ((ret = filemap_alloc(NO_FD, &file)) != 0) {
        return ret;
    }

    Inode *node = NULL;
    if ((ret = vfs_open(path, open_flags, &node)) != 0) {
        filemap_free(file);
        return ret;
    }

    file->pos = 0;
    if (open_flags & O_APPEND) {
        Stat __stat, *stat = &__stat;
        if ((ret = vop_fstat(node, stat)) != 0) {
            vfs_close(node);
            filemap_free(file);
            return ret;
        }
        file->pos = stat->st_size;
    }
    file->node = node;
    file->writable = writable;
    file->readable = readable;
    filemap_open(file);
    return file->fd;
}

int file_close(int fd) {
    int ret;
    File *file = NULL;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    filemap_close(file);
    return 0;
}

int file_read(int fd, void *base, size_t len, size_t *copied_store) {
    int ret;
    File *file = NULL;
    
    assert(copied_store != NULL);
    *copied_store = 0;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    if (!file->readable) {
        return -E_INVAL;
    }
    filemap_acquire(file);
    IOBuf __iob, *iob = iobuf_init(&__iob, base, len, file->pos);
    ret = vop_read(file->node, iob);
    size_t copied = iobuf_used(iob);
    if (file->status == FD_OPENED) {
        // todo: 为什么是需要判断在OPENED时才设置pos的值，没有打开文件时能拷贝数据吗？
        file->pos += copied;
    }
    *copied_store = copied;
    filemap_release(file);
    return ret;
}

int file_write(int fd, void *base, size_t len, size_t *copied_store) {
    int ret;
    File *file = NULL;
    assert(copied_store != NULL);
    *copied_store = 0;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    if (!file->writable) {
        return -E_INVAL;
    }
    filemap_acquire(file);

    IOBuf __iob;
    IOBuf *iob = iobuf_init(&__iob, base, len, file->pos);
    ret = vop_write(file->node, iob);

    size_t copied = iobuf_used(iob);
    if (file->status == FD_OPENED) {
        // todo: 为什么要判断FD_OPENED?
        file->pos += copied;
    }
    *copied_store = copied;
    filemap_release(file);
    return ret;
}

int file_fstat(int fd, Stat *stat) {
    int ret;
    File *file = NULL;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    filemap_acquire(file);
    ret = vop_fstat(file->node, stat);
    filemap_release(file);
    return ret;
}

int file_dup(int fd1, int fd2) {
    int ret;
    File *file1 = NULL;
    File *file2 = NULL;
    if ((ret = fd2file(fd1, &file1)) != 0) {
        return ret;
    }
    if ((ret = filemap_alloc(fd2, &file2)) != 0) {
        return ret;
    }
    filemap_dup(file2, file1);
    return file2->fd;
}

int file_seek(int fd, off_t pos, int whence) {
    Stat __stat;
    Stat *stat = &__stat;
    int ret;
    File *file = NULL;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    filemap_acquire(file);

    switch (whence)
    {
    case LSEEK_SET:
        break;
    case LSEEK_CUR:
        pos += file->pos;
        break;
    case LSEEK_END:
        if ((ret = vop_fstat(file->node, stat)) == 0) {
            pos += stat->st_size;
        }
        break;
    default:
        ret = -E_INVAL;
    }

    if (ret == 0) {
        if ((ret = vop_tryseek(file->node, pos)) == 0) {
            file->pos = pos;
        }
    }
    filemap_release(file);
    return ret;
}

int file_fsync(int fd) {
    int ret;
    File *file = NULL;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    filemap_acquire(file);
    ret = vop_fsync(file->node);
    filemap_release(file);
    return ret;
}

int file_get_dirent(int fd, DirectoryEntry *direntp) {
    int ret;
    File *file = NULL;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    filemap_acquire(file);
    IOBuf __iob;
    IOBuf *iob = iobuf_init(&__iob, direntp->name, sizeof(direntp->name), direntp->offset);
    if ((ret = vop_getdirentry(file->node, iob)) == 0) {
        direntp->offset += iobuf_used(iob);
    }
    filemap_release(file);
    return ret;
}