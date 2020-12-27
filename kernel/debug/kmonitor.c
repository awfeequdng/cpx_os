#include <kmonitor.h>

#include <memlayout.h>

#include <stdio.h>
#include <string.h>

#define CMDBUF_SIZE 80

struct Command {
	const char *name;
	const char *desc;
	int (*func)(int argc, char **argv, struct TrapFrame *tf);
};


static struct Command commands[] = {
	{ "help", "Display this list of commands", monitor_help },
	{ "kernel_info", "Display information about the kernel", monitor_kernel_info },
	{ "backtrace", "Display backtrace information", monitor_backtrace },
};

int monitor_help(int argc, char **argv, struct TrapFrame *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		printk("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}


int monitor_kernel_info(int argc, char **argv, struct TrapFrame *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	printk("Special kernel symbols:\n");
//	printk("  _start                  %08x (phys)\n", _start);
	printk("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNEL_BASE);
	printk("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNEL_BASE);
	printk("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNEL_BASE);
	printk("  end    %08x (virt)  %08x (phys)\n", end, end - KERNEL_BASE);
	printk("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}


int monitor_backtrace(int argc, char **argv, struct TrapFrame *tf)
{
	return 0;
}

#define WHITE_SPACE "\t\r\n"
#define MAX_ARGS 16
static int run_cmd(char *buf, struct TrapFrame *tf)
{
	int argc;
	char *argv[MAX_ARGS];
	while (1) {
		while (*buf && strchr(WHITE_SPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		if (argc == MAX_ARGS - 1) {
			printk("too many arguments (max %d)\n", MAX_ARGS);
			return 0;
		}

		argv[argc++] = buf;
		while (*buf && !strchr(WHITE_SPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	if (argc == 0)
		return 0;
	for (int i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	printk("unknown command '%s'\n", argv[0]);
	return 0;
}

void monitor(struct TrapFrame *tf)
{
	char *buf;
	printk("welcome to the cpx_os kernel monitor!\n");
	printk("Type 'help' for a list of commands.\n");

	while(1) {
		buf = readline("K> ");
		printk("\n");
		if (buf != NULL)
			if (run_cmd(buf, tf) < 0)
				break;
	}
}
