#ifndef __USER_LIBS_DIR_H__
#define __USER_LIBS_DIR_H__

#include <types.h>
#include <dirent.h>

typedef struct {
    int fd;
    struct dirent dirent;
} Dir;

Dir *open_dir(const char *path);
struct dirent *read_dir(Dir *dirp);
void close_dir(Dir *dirp);
int chdir(const char *path);
int get_cwd(char *buffer, size_t len);
int mkdir(const char *path);
int link(const char *old_path, const char *new_path);
int rename(const char *old_path, const char *new_path);
int unlink(const char *path);


#endif //__USER_LIBS_DIR_H__