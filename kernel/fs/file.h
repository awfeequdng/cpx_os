#ifndef __KERNEL_FS_FILE_H__
#define __KERNEL_FS_FILE_H__

#include <types.h>
#include <fs.h>
#include <atomic.h>
#include <process.h>
#include <assert.h>
#include <stat.h>

struct inode;
struct dirent;

typedef struct file {
    enum {
        FD_NONE, FD_INIT, FD_OPENED, FD_CLOSED,
    }status;
    bool readable;
    bool writable;
    int fd;
    off_t pos;
    struct inode *node;
    atomic_t open_count;
} File;

void filemap_init(File *file_map);
void filemap_open(File *file);
void filemap_close(File *file);
void filemap_dup(File *to, File *from);
bool file_testfd(int fd, bool readable, bool writable);

int file_open(char *path, uint32_t open_flags);
int file_close(int fd);
int file_read(int fd, void *base, size_t len, size_t *copied_store);
int file_write(int fd, void *base, size_t len, size_t *copied_store);
int file_fstat(int fd, Stat *stat);
int file_dup(int fd1, int fd2);
int file_seek(int fd, off_t pos, int whence);
int file_fsync(int fd);
int file_get_dirent(int fd, struct dirent *direntp);


static inline int fopen_count(File *file) {
    return atomic_read(&(file->open_count));
}

static inline int fopen_count_inc(File *file) {
    return atomic_add_return(&(file->open_count), 1);
}

static inline int fopen_count_dec(File *file) {
    return atomic_sub_return(&(file->open_count), 1);
}

#endif // __KERNEL_FS_FILE_H__