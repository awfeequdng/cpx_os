#ifndef __INCLUDE_STDIO_H__
#define __INCLUDE_STDIO_H__

#include <include/stdarg.h>

// console.c
void putchar(int c);

// lib/printfmt.c
void printfmt(void (*)(int, void*), void *, const char *fmt, ...);
void vprintfmt(void (*)(int, void*), void *, const char *fmt, va_list);
int  snprintf(char *buf, int size, const char *fmt, ...);
int vsnprintf(char *buf, int size, const char *fmt, va_list);

// lib/print.c
int printk(const char* fmt, ...);
int vprintk(const char *fmt, va_list);




#endif // __INCLUDE_STDIO_H__
