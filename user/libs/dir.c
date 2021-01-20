#include <dir.h>
#include <types.h>
#include <malloc.h>
#include <stat.h>
#include <file.h>
#include <syscall.h>

Dir *open_dir(const char *path) {
    Dir *dirp = NULL;
    if ((dirp = malloc(sizeof(Dir))) == NULL) {
        return NULL;
    }
    if ((dirp->fd = open(path, O_RDONLY)) < 0) {
        goto failed;
    }
    struct stat __stat;
    struct stat *stat = &__stat;
    if (fstat(dirp->fd, stat) != 0 || !S_ISDIR(stat->st_mode)) {
        goto failed;
    }
    dirp->dirent.offset = 0;
    return dirp;

failed:
    free(dirp);
    return NULL;
}

struct dirent *read_dir(Dir *dirp) {
    if (sys_get_dirent(dirp->fd, &(dirp->dirent)) == 0) {
        return &(dirp->dirent);
    }
    return NULL;
}

void close_dir(Dir *dirp) {
    close(dirp->fd);
    free(dirp);
}

int chdir(const char *path) {
    return sys_chdir(path);
}

int get_cwd(char *buffer, size_t len) {
    return sys_get_cwd(buffer, len);
}