#include <stdio.h>
#include <types.h>
#include <syscall.h>

static void putch(int c, int *cnt) {
    sys_putc(c);
    (*cnt)++;
}

int vprintf(const char *fmt, va_list ap) {
    int cnt = 0;
    vprintfmt((void *)putch, &cnt, fmt, ap);
    return cnt;
}

int printf(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    int cnt = vprintf(fmt, ap);
    va_end(ap);

    return cnt;
}

int puts(const char *str) {
    int cnt = 0;
    char c;
    while ((c = *str++) != '\0') {
        putch(c, &cnt);
    }
    putch('\n', &cnt);
    return cnt;
}

void putchar(int c) {
    int cnt = 0;
    putch(c, &cnt);
}