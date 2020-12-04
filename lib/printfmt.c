#include <types.h>
#include <error.h>
#include <stdarg.h>
#include <string.h>

static const char *error_string[MAX_ERROR] = {
	[E_INVAL] = "invalid parameter",
};


// todo: 这段除法的意图是什么？？
#define do_div(n, base) ({                                          \
            unsigned long __upper, __low, __high, __mod, __base;    \
            __base = (base);                                        \
            asm("" : "=a" (__low), "=d" (__high) : "A" (n));        \
            __upper = __high;                                       \
            if (__high != 0) {                                      \
                __upper = __high % __base;                          \
                __high = __high / __base;                           \
            }                                                       \
            asm("divl %2" : "=a" (__low), "=d" (__mod)              \
                : "rm" (__base), "0" (__low), "1" (__upper));       \
            asm("" : "=A" (n) : "a" (__low), "d" (__high));         \
            __mod;                                                  \
        })


static void printnum(void (*putc)(int, void*), void *putbuf,
		unsigned long long num,
		unsigned base, int width, int padc)
{
	unsigned long long result = num;
	unsigned mod = do_div(result, base);
	
	if (num >= base) {
		// 程序报如下错误：'undefined reference to __umoddi3'，说明处理器不支持求余操作
		printnum(putc, putbuf, result, base, width - 1, padc);
	} else {
		while (--width > 0)
			putc(padc, putbuf);
	}
	putc("0123456789abcdef"[mod], putbuf);
}

static unsigned long long getuint(va_list *ap, int lflag)
{
	if (lflag >= 2)
		return va_arg(*ap, unsigned long long);
	else if (lflag)
		return va_arg(*ap, unsigned long);
	else
		return va_arg(*ap, unsigned int);
}

static long long getint(va_list *ap, int lflag)
{
        if (lflag >= 2)
                return va_arg(*ap, long long);
        else if (lflag)
                return va_arg(*ap, long);
        else
                return va_arg(*ap, int);
}


void printfmt(void (*putc)(int, void*), void *putbuf, const char *fmt, ...);

void vprintfmt(void (*putc)(int, void *), void *putbuf, const char *fmt, va_list ap)
{
	register const char *p;
	register int c, err;
	unsigned long long num;
	char padc;
	int base, width, precision, altflag, lflag;
	while (1) {
		while ((c = *(unsigned char *) fmt++) != '%') {
			if (c == '\0')
				return;
			putc(c, putbuf);
		}
		// process a %-escape sequence
		padc = ' ';
		width = -1;
		precision = -1;
		altflag = 0;
		lflag = 0;
	reswitch:
		switch (c = *(unsigned char *) fmt++) {
		// flag to pad on the right
		case '-':
			padc = '-';
			goto reswitch;
		case '0':
			padc = '0';
			goto reswitch;
		// width field
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			for (precision = 0;; fmt++) {
				precision = precision * 10 + c - '0';
				c = *fmt; // fmt 不需要设置为下一个元素吗？
				if (c < '0' || c > '9')
					break; // 当break后，fmt指向的值和c相同
			}
			goto process_precision;

		case '*':
			precision = va_arg(ap, int);
			goto process_precision;

		case '.':
			if (width < 0)
				width = 0;
			goto reswitch;

		case '#':
			altflag = 1;
			goto reswitch;
		
		process_precision:
			if (width < 0)
				width = precision, precision = -1;
			goto reswitch;

		// long flag (doubled for long long)
		case 'l':
			lflag++;
			goto reswitch;

		// character
		case 'c':
			putc(va_arg(ap, int), putbuf);
			break;
		// error message
		case 'e':
			err = va_arg(ap, int);
			if (err < 0)
				err = -err;
			if (err >= MAX_ERROR || (p = error_string[err]) == NULL)
				printfmt(putc, putbuf, "error %d", err);
			else
				printfmt(putc, putbuf, "%s", p);
			break;
		// string
		case 's':
			if ((p = va_arg(ap, char *)) == NULL)
				p = "(null)";
			if (width > 0 && padc != '-')
				for (width -= strnlen(p, precision); width > 0; width--)
					putc(padc, putbuf);
			for (; (c = *p++) != '\0' && (precision < 0 || --precision >= 0); width--)
				if (altflag && (c < ' ' || c > '~'))
					putc('?', putbuf);
				else
					putc(c, putbuf);

			for (; width > 0; width--)
				putc(' ', putbuf);
			break;
		case 'd':
			num = getint(&ap, lflag);
			if ((long long) num < 0) {
				putc('-', putbuf);
				num = -(long long) num;
			}
			base = 10;
			goto number;
		case 'u':
			num = getuint(&ap, lflag);
			base = 10;
			goto number;

		case 'o':
			num = getuint(&ap, lflag);
			base = 8;
			goto number;

		case 'p':
			putc('0', putbuf);
			putc('x', putbuf);
			num = (unsigned long long)(uintptr_t) va_arg(ap, void *);
			base = 16;
			goto number;
		case 'x':
			num = getuint(&ap, lflag);
			base = 16;
		number:
			printnum(putc, putbuf, num, base, width, padc);
			break;

		// escaped '%' character
		case '%':
			putc(c, putbuf);
			break;

		// unrecognized escape sequence - just print it literally
		default:
			putc('%', putbuf);
			for (fmt--; fmt[-1] != '%'; fmt--);


		}
	}
}

void printfmt(void (*putc)(int, void*), void *putbuf, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintfmt(putc, putbuf, fmt, ap);
	va_end(ap);
}

struct sprintbuf {
	char *buf;
	char *ebuf;
	int cnt;
};

static void sprintputc(int c, struct sprintbuf *b)
{
	
	if (b->buf < b->ebuf) {
		*(b->buf)++ = c;
		b->cnt++;
	}
}

int vsnprintf(char *buf, int n, const char *fmt, va_list ap)
{
	// 最后保留一个字符用于填充\0结束符
	struct sprintbuf b = {buf, buf + n - 1, 0};

	if (buf == NULL || n < 1)
		return -E_INVAL;
	vprintfmt((void*)sprintputc, &b, fmt, ap);
	*b.buf = '\0';

	return b.cnt;	
}

int snprintf(char *buf, int n, const char *fmt, ...)
{
	va_list ap;
	int rc;
	
	va_start(ap, fmt);
	rc = vsnprintf(buf, n, fmt, ap);
	va_end(ap);

	return rc;
}
