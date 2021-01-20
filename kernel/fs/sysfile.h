#ifndef __KERNEL_FS_SFSFILE_H__
#define __KERNEL_FS_SFSFILE_H__

#include <types.h>

struct stat;
struct dirent;

int sysfile_open(const char *open, uint32_t open_flags);
int sysfile_close(int df);
int sysfile_read(int df, void *base, size_t len);
int sysfile_write(int df, void *base, size_t len);
int sysfile_seek(int fd, off_t pos, int whence);
int sysfile_fstat(int fd, struct stat *stat);
int sysfile_fsync(int fd);
int sysfile_chdir(const char *path);
int sysfile_get_cwd(char *buf, size_t len);
int sysfile_get_dirent(int fd, struct dirent *direntp);
int sysfile_dup(int fd1, int fd2);
int sysfile_mkdir(const char *path);
int sysfile_link(const char *path1, const char *path2);
int sysfile_rename(const char *path1, const char *path2);
int sysfile_unlink(const char *path);
#endif //__KERNEL_FS_SFSFILE_H__