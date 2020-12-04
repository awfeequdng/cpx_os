#ifndef __INCLUDE_CONSOLE_H__
#define __INCLUDE_CONSOLE_H__

#ifndef CPX_OS_KERNEL
# error "This is a CPX_OS kernel header; user programs should not #include it"
#endif

void console_init(void);
int console_getc(void);
void console_putc(int);

#endif
