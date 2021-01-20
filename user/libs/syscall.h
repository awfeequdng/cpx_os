#ifndef __USER_LIBS_SYSCALL_H__
#define __USER_LIBS_SYSCALL_H__

#include <types.h>

int sys_exit(int error_code);
int sys_fork(void);
int sys_wait(int pid, int *store);
int sys_yield(void);
int sys_sleep(unsigned int time);
int sys_kill(int pid);
size_t sys_gettime(void);
int sys_getpid(void);
int sys_brk(uintptr_t *brk_store);
int sys_putc(int c);
int sys_page_dir(void);
int sys_mmap(uintptr_t *addr_store, size_t len, uint32_t mmap_flags);
int sys_munmap(uintptr_t addr, size_t len);
int sys_shmem(uintptr_t *addr_store, size_t len, uint32_t mmap_flags);
sem_t sys_sem_init(int value);
int sys_sem_post(sem_t sem_id);
int sys_sem_wait(sem_t sem_id);
int sys_sem_get_value(sem_t sem_id, int *value_store);


struct stat;
struct dirent;

int sys_open(const char *path, uint32_t open_flags);
int sys_close(int fd);
int sys_read(int fd, void *base, size_t len);
int sys_write(int fd, void *base, size_t len);
int sys_seek(int fd, off_t pos, int whence);
int sys_fstat(int fd, struct stat *stat);
int sys_fsync(int fd);
int sys_chdir(const char *path);
int sys_get_cwd(char *buffer, size_t len);
int sys_get_dirent(int fd, struct dirent *dirent);
int sys_dup(int fd1, int fd2);

#endif // __USER_LIBS_SYSCALL_H__