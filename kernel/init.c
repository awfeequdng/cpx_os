#include <include/stdio.h>
#include <include/console.h>

void start_kernel(void)
{
	extern char edata[], end[];

	console_init();
	putchar('h');
	putchar('e');
	putchar('l');
	putchar('l');
	putchar('o');
	putchar('\n');
	
	while(1);
}
