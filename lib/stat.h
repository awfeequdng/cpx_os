#ifndef __LIB_STAT_H__
#define __LIB_STAT_H__

#include <types.h>

typedef struct stat {
    uint32_t st_mode;
    size_t st_nlinks;
    size_t st_blocks;
    size_t st_size;
} Stat;

#define S_IFMT          070000
#define S_IFREG         010000
#define S_IFDIR         020000
#define S_IFLNK         030000
#define S_IFCHR         040000
#define S_IFBLK         050000

#define S_ISREG(mode)       (((mode) & S_IFMT) == S_IFREG)
#define S_ISDIR(mode)       (((mode) & S_IFMT) == S_IFDIR)
#define S_ISLNK(mode)       (((mode) & S_IFMT) == S_IFLNK)
#define S_ISCHR(mode)       (((mode) & S_IFMT) == S_IFCHR)
#define S_ISBLK(mode)       (((mode) & S_IFMT) == S_IFBLK)

#endif // __LIB_STAT_H__