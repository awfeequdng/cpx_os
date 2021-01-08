#ifndef __INCLUDE_STDIO_H__
#define __INCLUDE_STDIO_H__

#include <stdarg.h>

// stdio.c
void putchar(int c);
int getchar(void);
int is_console(int);

// lib/printfmt.c
void printfmt(void (*)(int, void*), void *, const char *fmt, ...);
void vprintfmt(void (*)(int, void*), void *, const char *fmt, va_list);
int  snprintf(char *buf, int size, const char *fmt, ...);
int vsnprintf(char *buf, int size, const char *fmt, va_list);

// lib/print.c
int printk(const char* fmt, ...);
int vprintk(const char *fmt, va_list);

// lib/readline.c
char*	readline(const char *prompt);


#endif // __INCLUDE_STDIO_H__
