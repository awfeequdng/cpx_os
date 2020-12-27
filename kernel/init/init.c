#include <stdio.h>
#include <string.h>

#include <console.h>
#include <kmonitor.h>
#include <assert.h>
#include <x86.h>
#include <clock.h>
#include <pic_irq.h>
#include <pmm.h>

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
	// 在这个函数中调用了pic_enable将serial中断使能
	// 本应该是不生效的，只是pic_enable用一个局部变量记录下了这个中断掩码，
	// 然后最后开启pic中断控制器时将这个记录下来的掩码设置到控制器了，
	// 这才使得seriel中断能够生效
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

	// 重新初始化分段，使用pmm对段重新进行初始化，在初始化之前已经开启了分页
	// 也就是说，我们通过lgdt加载的是虚拟地址，而不是物理地址
	// 为什么加载的是虚拟地址程序也不跑飞？？
	pmm_init();

	// 初始化中断描述符表，此处已经开启了分页，加载的中断描述符地址应该是虚拟地址，
	// 不是物理地址，所以应该找不到中断描述符地址才对？此处为什么没有错误？
	idt_init();

	// clock_init();

	// 开启总中断
	sti();

	// panic("before monitor\n");

	while(1)
		monitor(NULL);
}
