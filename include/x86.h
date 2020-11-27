#ifndef __INCLUDE_X86_H__
#define __INCLUDE_X86_H__

#include <include/types.h>

static inline void breakpoint(void)
{
	asm volatile("int3");
}

static inline uint8_t inb(int port)
{
	uint8_t data;
	asm volatile("inb %w1, %0": "=a" (data) : "d" (port));
	return data;
}

static inline void insb(int port, void *addr, int cnt)
{
	asm volatile("cld\n\trepne\n\tinsb"
		: "=D" (addr), "=c" (cnt)
		: "d" (port), "0" (addr), "1" (cnt)
		: "memory", "cc");
}

static inline uint16_t inw(int port)
{
	uint16_t data;
	asm volatile("inw %w1, %0": "=a" (data) : "d" (port));
	return data;
}

static inline void insw(int port, void *addr, int cnt)
{
	asm volatile("cld\n\trepne\n\tinsw"
		     : "=D" (addr), "=c" (cnt)
		     : "d" (port), "0" (addr), "1" (cnt)
		     : "memory", "cc");
}

static inline uint32_t inl(int port)
{
	uint32_t data;
	asm volatile("inl %w1, %0" : "=a" (data) : "d" (port));
	return data;
}

static inline void insl(int port, void *addr, int cnt)
{
	asm volatile("cld\n\trepne\n\tinsl"
		: "=D" (addr), "=c" (cnt)
		: "d" (port), "0" (addr), "1" (cnt)
		: "memory", "cc");
}

static inline void outb(int port, uint8_t data)
{
	asm volatile("outb %0, %w1":: "a" (data), "d" (port));
}


static inline void outsb(int port, const void *addr, int cnt)
{
	asm volatile("cld\n\trepne\n\toutsb"
		     : "=S" (addr), "=c" (cnt)
		     : "d" (port), "0" (addr), "1" (cnt)
		     : "cc");
}

static inline void outw(int port, uint16_t data)
{
	asm volatile("outw %0,%w1" : : "a" (data), "d" (port));
}

static inline void outsw(int port, const void *addr, int cnt)
{
	asm volatile("cld\n\trepne\n\toutsw"
		     : "=S" (addr), "=c" (cnt)
		     : "d" (port), "0" (addr), "1" (cnt)
		     : "cc");
}

static inline void outsl(int port, const void *addr, int cnt)
{
	asm volatile("cld\n\trepne\n\toutsl"
		     : "=S" (addr), "=c" (cnt)
		     : "d" (port), "0" (addr), "1" (cnt)
		     : "cc");
}

static inline void outl(int port, uint32_t data)
{
	asm volatile("outl %0,%w1" : : "a" (data), "d" (port));
}

static inline void invlpg(void *addr)
{
	asm volatile("invlpg (%0)" :: "r" (addr): "memory");
}

static inline void lidt(void *p)
{
	asm volatile("lidt (%0)" :: "r" (p));
}

static inline void lgdt(void *p)
{
	asm volatile("lgdt (%0)" :: "r" (p));
}

#endif // __INCLUDE_X86_H__
