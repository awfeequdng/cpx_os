#include <stdio.h>
#include <console.h>


void putchar(int c)
{
	console_putc(c);
}

int getchar(void)
{
	return console_getc();
}
