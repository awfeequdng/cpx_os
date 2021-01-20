#ifndef __USER_LIBS_STDIO_H__
#define __USER_LIBS_STDIO_H__

#include <stdarg.h>

// stdio.c
void putchar(int c);
int puts(const char *str);
int printf(const char* fmt, ...);
int vprintf(const char *fmt, va_list);

// lib/printfmt.c
void printfmt(void (*)(int, void*, int), int, void *, const char *fmt, ...);
void vprintfmt(void (*)(int, void*, int), int, void *, const char *fmt, va_list);
int  snprintf(char *buf, int size, const char *fmt, ...);
int vsnprintf(char *buf, int size, const char *fmt, va_list);


#endif // __USER_LIBS_STDIO_H__
