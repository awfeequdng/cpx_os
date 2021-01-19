#ifndef __LIB_DIRENT_H__
#define __LIB_DIRENT_H__

#include <types.h>
#include <unistd.h>

typedef struct dirent {
    off_t offset;
    char name[FS_MAX_FNAME_LEN + 1];
} DirectoryEntry;

#endif //__LIB_DIRENT_H__