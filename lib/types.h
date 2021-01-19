#ifndef __INCLUDE_TYPES_H__
#define __INCLUDE_TYPES_H__

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef int bool;

enum { false, true };

#define BYTE_BITS		8

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

typedef int32_t intptr_t;
typedef unsigned long uintptr_t;
typedef unsigned long phyaddr_t;

typedef uint32_t ppn_t;

typedef uint32_t size_t;

typedef int32_t ssize_t;

typedef int32_t off_t;

typedef uint32_t pte_t;
typedef uint32_t pde_t;
typedef pte_t swap_entry_t;

typedef uintptr_t sem_t;

#define MIN(_a, _b) 	\
({			\
 	typeof(_a) __a = (_a);	\
	typeof(_b) __b = (_b);	\
	__a <= __b ? __a : __b;	\
 })

#define MAX(_a, _b)	\
({			\
 	typeof(_a) __a = (_a);	\
	typeof(_b) __b = (_b);	\
	__a <= __b ? __b : __a;	\
})

#define ROUNDDOWN(a, n)	\
({			\
 	uint32_t __a = (uint32_t)(a);	\
	(typeof(a)) (__a - __a % (n));	\
})

#define ROUNDUP(a, n)	\
({			\
 	uint32_t __n = (uint32_t)(n);	\
 	(typeof(a)) (ROUNDDOWN((uint32_t)(a) + __n - 1, __n));	\
})

#define ROUNDUP_DIV(a, n)	\
({			\
 	uint32_t __n = (uint32_t)(n);	\
 	(typeof(a)) (((a) + __n - 1) / __n);	\
})

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define offsetof(type, member) ((void*) (&((type*)0)->member))

#define container_of(ptr, type, member) ({	\
	typeof(((type *)0)->member) *__ptr = (ptr);	\
	(type *) ((void*)__ptr - offsetof(type, member));	\
})

#endif // __INCLUDE_TYPES_H__
