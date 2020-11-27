#ifndef __INCLUDE_CONSOLE_H__
#define __INCLUDE_CONSOLE_H__

#ifndef CPX_OS_KERNEL
# error "This is a CPX_OS kernel header; user programs should not #include it"
#endif

#include <include/types.h>

#define MONO_BASE 	0x3b4
#define MONO_BUF	0xb0000
#define CGA_BASE	0x3d4
#define CGA_BUF		0xb8000

#define CRT_ROWS	25
#define CRT_COLS	80
#define CRT_SIZE	(CRT_ROWS * CRT_COLS)

void console_init(void);
// void console_getc(void);


#endif
