#include <types.h>
#include <stdarg.h>
#include <ulib.h>
#include <error.h>
#include <stdio.h>

void __panic(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("user panic at %s:%d:\n    ", file, line);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    exit(-E_PANIC);
}

void __warn(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("user warn at %s:%d:\n    ", file, line);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}