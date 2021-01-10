#ifndef __USER_LIBS_ULIB_H__
#define __USER_LIBS_ULIB_H__

void __warn(const char *file, int line, const char *fmt, ...);
void __panic(const char *file, int line, const char *fmt, ...) __attribute__((noreturn));

#define warn(...)           \
        __warn(__FILE__, __LINE__, __VA_ARGS__);

#define panic(...)           \
        __panic(__FILE__, __LINE__, __VA_ARGS__);

#define assert(x)   \
    do {    \
        if (!(x)) {    \
            panic("assertion failed: %s", #x);    \
        }   \
    } while (0)

#define static_assert(x)        \
    switch (x) { case 0: case x: ; }

void exit(int error_code) __attribute__((noreturn));
int fork(void);
int wait(void);
int waitpid(int pid, int *store);
int sleep(unsigned int time);
unsigned int gettime_msec(void);
void yield(void);
int kill(int pid);
int getpid(void);
void print_page_dir(void);

#endif // __USER_LIBS_ULIB_H__