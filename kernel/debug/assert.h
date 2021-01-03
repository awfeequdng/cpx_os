#ifndef __KERNEL_DEBUG_ASSERT_H__
#define __KERNEL_DEBUG_ASSERT_H__

void __warn(const char *file, int line, const char *fmt, ...);
void __panic(const char *file, int line, const char *fmt, ...) __attribute__((noreturn));

#define warn(...)                                       \
    __warn(__FILE__, __LINE__, __VA_ARGS__)

#define panic(...)                                      \
    __panic(__FILE__, __LINE__, __VA_ARGS__)

#define assert(x)                                       \
    do {                                                \
        if (!(x)) {                                     \
            panic("assertion failed: %s", #x);          \
        }                                               \
    } while (0)

// static_assert(x) x等於false時，會在編譯時產生一個錯誤
// 即static_assert这个关键字，用来做编译期间的断言，因此叫作静态断言
#define static_assert(x)                                \
    switch (x) { case 0: case (x): ; }

#endif //__KERNEL_DEBUG_ASSERT_H__

