#include <trap.h>
#include <stdio.h>

void trap(struct TrapFrame *tf)
{
	printk("fall in trap\n");
}
