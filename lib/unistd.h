#ifndef __LIB_UNISTD_H__
#define __LIB_UNISTD_H__

#define T_SYSCALL       0x80

// syscall number
#define SYS_exit            1
#define SYS_fork            2
#define SYS_wait            3
#define SYS_exec            4
#define SYS_clone           5
#define SYS_exit_thread     9
#define SYS_yield           10
#define SYS_sleep           11
#define SYS_kill            12
#define SYS_gettime         17
#define SYS_getpid          18
#define SYS_brk             19
#define SYS_mmap            20
#define SYS_munmap          21
#define SYS_shmem           22
#define SYS_putc            30
#define SYS_pgdir           31
#define SYS_sem_init        40
#define SYS_sem_post        41
#define SYS_sem_wait        42
#define SYS_sem_get_value   43

#define SYS_open            100
#define SYS_close           101
#define SYS_read            102
#define SYS_write           103
#define SYS_seek            104
#define SYS_fstat           110
#define SYS_fsync           111
#define SYS_chdir           120
#define SYS_getcwd          121
#define SYS_mkdir           122
#define SYS_link            123
#define SYS_rename          124
#define SYS_readlink        125
#define SYS_symlink         126
#define SYS_unlink          127
#define SYS_getdirentry     128
#define SYS_dup             130


// SYS_fork flags
#define CLONE_VM        0x00000100  // 如果进程之间共享内存，则设置这个标志
#define CLONE_THREAD    0x00000200  // 线程组
#define CLONE_SEM       0x00000400  // 信号量
#define CLONE_FS        0x00000800   // 共享打开的文件

// SYS_mmap flags
#define MMAP_WRITE      0x00000100  
#define MMAP_STACK      0x00000200


#define O_RDONLY            0
#define O_WRONLY            1
#define O_RDWR              2

#define O_CREAT             0x00000004
#define O_EXCL              0x00000008
#define O_TRUNC             0x00000010
#define O_APPEND            0x00000020
// additonal related definition
#define O_ACCMODE           3   // mask for O_RDONLY / O_WRONLY / O_RDWR

#define NO_FD               -0x9527 // invalid fd

#define LSEEK_SET           0
#define LSEEK_CUR           1
#define LSEEK_END           2

// 目录名长度
#define FS_MAX_DNAME_LEN    31
// 文件名长度
#define FS_MAX_FNAME_LEN    255
// 最长路径名
#define FS_MAX_FPATH_LEN    4095

#endif // __LIB_UNISTD_H__