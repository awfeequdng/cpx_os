#include <stdio.h>
#include <string.h>

#include <console.h>
#include <kmonitor.h>
#include <assert.h>
#include <x86.h>

void printk_test(void)
{
	printk("6828 decimal is %o octal!\n", 6828);

	int x = -1, y = -3, z = -4;
	printk("x %d, y %x, z %d\n", x, y, z);

	unsigned int i = 0x00646c72;
	printk("H%x Wo%s\n", 57616, &i);

	printk("x=%d y=%d\n", 3);
}

void bss_test(char*start, int cnt)
{

	for (int i = 0; i < cnt; i++) {
		printk("%x ",((unsigned*)start)[i]);
	}
	printk("\n");
}

void start_kernel(void)
{
    extern char edata[], end[];
	// bss段清零
	memset(edata, 0, end - edata);

	// 终端初始化，显示和键盘输入初始化
  	console_init();
	
	int cnt = (unsigned)(end - edata);
	printk("\nedata = 0x%x, end - edata= %d\n\n", edata, cnt);
	
	bss_test(edata - 16, 32);
	bss_test(end - 16, 32);

	printk("edata = %x, end = %x\n", edata, end);
	putchar('h');
	putchar('e');
	putchar('l');
	putchar('l');
	putchar('o');
	putchar('\n');

	printk("hello: using printk!\n");
	
	printk_test();
	
	pic_init();	// 初始化中断控制器

	// 初始化中断描述符表
	idt_init();

	// 开启总中断
	sti();

	// panic("before monitor\n");

	while(1)
		monitor(NULL);
}
