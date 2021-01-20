#include <stdio.h>
#include <types.h>
#include <syscall.h>
#include <unistd.h>

static void putch(int c, int *cnt) {
    sys_putc(c);
    (*cnt)++;
}

int vprintf(const char *fmt, va_list ap) {
    int cnt = 0;
    vprintfmt((void *)putch, NO_FD, &cnt, fmt, ap);
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

static void fputch(int c, int *cnt, int fd) {
    char ch = c;
    write(fd, &ch, sizeof(char));
    (*cnt)++;
}

int vfprintf(int fd, const char *fmt, va_list ap) {
    int cnt = 0;
    vprintfmt((void *)fputch, fd, &cnt, fmt, ap);
    return cnt;
}

int fprintf(int fd, const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    int cnt = vfprintf(fd, fmt, ap);
    va_end(ap);

    return cnt;
}