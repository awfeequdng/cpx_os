
#include <include/string.h>

#define ASM 1


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
