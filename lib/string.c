
#include <include/string.h>

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

#endif
