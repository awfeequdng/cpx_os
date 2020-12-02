#include <include/types.h>
#include <include/stdio.h>
#include <include/stdarg.h>

static void putc(int c, int *cnt)
{
	putchar(c);
	*cnt++;
}

int vprintk(const char *fmt, va_list ap)
{
	int cnt = 0;
	vprintfmt((void *)putc, &cnt, fmt, ap);
	return cnt;
}

int printk(const char *fmt, ...)
{
	va_list ap;
	int cnt;

	va_start(ap, fmt);
	cnt = vprintk(fmt, ap);
	va_end(ap);

	return cnt;
}
