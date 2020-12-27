
#include <string.h>

#define ASM 1

int strlen(const char *s)
{
	int n;
	for (n = 0; *s != '\0'; s++)
		n++;
	return n;
}

int strnlen(const char *s, size_t size)
{
	int n;
	for (n = 0; size > 0 && *s != '\0'; s++, size--)
		n++;
	return n;
}

char *strcpy(char *dst, const char *src)
{
	char *ret;

	ret = dst;
	while ((*dst++ = *src++) != '\0')
	       /* do nothing */;
	return ret;	
}

char *strcat(char *dst, const char *src)
{
	int len = strlen(dst);
	strcpy(dst + len, src);
	return dst;
}

char *strncpy(char *dst, const char *src, size_t size)
{
	size_t i;
	char *ret;

	ret = dst;
	for (i = 0; i < size; i++) {
		*dst++ = *src;
		if (*src != '\0')
			src++;
	}
	return ret;
}


#if ASM

void *memset(void *v, int c, size_t n)
{
	if (n == 0)
		return v;
	if ((int)v%4 == 0 && n%4 == 0) {
		c &= 0xFF;
		c = (c<<24)|(c<<16)|(c<<8)|c;
		asm volatile("cld; rep stosl\n"
			:: "D" (v), "a" (c), "c" (n/4)
			: "cc", "memory");
	} else
		asm volatile("cld; rep stosb\n"
			:: "D" (v), "a" (c), "c" (n)
			: "cc", "memory");
	return v;
}

void * memmove(void *dst, const void *src, size_t n)
{
	const char *s;
	char *d;
	
	s = src;
	d = dst;
	// 原地址的尾部和目的地址的头有重叠, 从最后一个字符向前拷贝，避免字符被覆盖
	if (s < d && s + n > d) {
		s += n;
		d += n;
		// 此处还可以优化，以后再改
		if ((int)s % 4 == 0 && (int)d % 4 == 0 && n == 4) 
			// std设置字符串拷贝的方向向低地址增长
			asm volatile("std; rep movsl\n"
				:: "D" (d -4), "S" (s - 4), "c" (n / 4)
				: "memory", "cc");
		else
			asm volatile("std; rep movsb\n"
				:: "D" (d - 1), "S" (s - 1), "c" (n)
				: "memory", "cc");

		// 字符串拷贝完成后清除方向
		asm volatile("cld" ::: "cc");
	} else {
		if ((int)s%4 == 0 && (int)d%4 == 0 && n%4 == 0)
			asm volatile("cld; rep movsl\n"
				:: "D" (d), "S" (s), "c" (n/4) 
				: "cc", "memory");
		else
			asm volatile("cld; rep movsb\n"
				:: "D" (d), "S" (s), "c" (n) 
				: "cc", "memory");
	}
	return dst;
}

#else // ASM


void *memset(void *v, int c, size_t n)
{
	char *p;
	int m;

	p = v;
	m = n;
	while (--m >= 0)
		*p++ = c;

	m += 1;
	return v;
}

void *memmove(void *dst, const void *src, size_t n)
{
	const char *s;
	char *d;

	s = src;
	d = dst;
	if (s < d && s + n > d) {
		s += n;
		d += n;
		while (n-- > 0)
			*--d = *--s;
	} else
		while (n-- > 0)
			*d++ = *s++;

	return dst;
}
#endif

int strcmp(const char *p, const char *q)
{
	while (*p && *p == *q)
		p++, q++;
	return (int) ((unsigned char)*p - (unsigned char)*q);
}

int strncmp(const char *p, const char *q, size_t n)
{
	while (n > 0 && *p && *p == *q)
		n--, p++, q++;
	if (n == 0)
		return 0;
	else
		return (int) ((unsigned char)*p - (unsigned char)*q);
}

char *strchr(const char *s, char c)
{
	for (; *s; s++)
		if (*s == c)
			return (char *)s;
	return 0;
}


/* *
 * strfind - locates first occurrence of character in string
 * @s:      the input string
 * @c:      character to be located
 *
 * The strfind() function is like strchr() except that if @c is
 * not found in @s, then it returns a pointer to the null byte at the
 * end of @s, rather than 'NULL'.
 * */
char *strfind(const char *s, char c) {
    while (*s != '\0') {
        if (*s == c) {
            break;
        }
        s ++;
    }
    return (char *)s;
}