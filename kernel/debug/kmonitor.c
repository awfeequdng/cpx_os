#include <kmonitor.h>
#include <kdebug.h>

#include <stdio.h>
#include <string.h>

struct Command {
    const char *name;
    const char *desc;
    // return -1 to force monitor to exit
    int(*func)(int argc, char **argv, struct TrapFrame *tf);
};

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

static struct Command commands[] = {
    {"help", "Display this list of commands.", monitor_help},
    {"kernel_info", "Display information about the kernel.", monitor_kernel_info},
	// {"backtrace", "Print backtrace of stack frame.", monitor_backtrace},
};

#define MAXARGS         16
#define WHITESPACE      " \t\n\r"

static int parse(char *buf, char **argv) {
    int argc = 0;
    while (1) {
        while (*buf != '\0' && strchr(WHITESPACE, *buf) != NULL) {
            *buf ++ = '\0';
        }
        if (*buf == '\0') {
            break;
        }

        if (argc == MAXARGS - 1) {
            printk("Too many arguments (max %d).\n", MAXARGS);
        }
        argv[argc ++] = buf;
        while (*buf != '\0' && strchr(WHITESPACE, *buf) == NULL) {
            buf ++;
        }
    }
    return argc;
}

static int run_cmd(char *buf, struct TrapFrame *tf) {
    char *argv[MAXARGS];
    int argc = parse(buf, argv);
    if (argc == 0) {
        return 0;
    }
    int i;
    for (i = 0; i < ARRAY_SIZE(commands); i ++) {
        if (strcmp(commands[i].name, argv[0]) == 0) {
            return commands[i].func(argc - 1, argv + 1, tf);
        }
    }
    printk("Unknown command '%s'\n", argv[0]);
    return 0;
}

void monitor(struct TrapFrame *tf) {
    printk("Welcome to the kernel debug monitor!!\n");
    printk("Type 'help' for a list of commands.\n");

    char *buf;
    while (1) {
        if ((buf = readline("K> ")) != NULL) {
            printk("\n");
            if (run_cmd(buf, tf) < 0) {
                break;
            }
        }
    }
}

int monitor_help(int argc, char **argv, struct TrapFrame *tf) {
    int i;
    for (i = 0; i < ARRAY_SIZE(commands); i ++) {
        printk("%s - %s\n", commands[i].name, commands[i].desc);
    }
    return 0;
}

int monitor_kernel_info(int argc, char **argv, struct TrapFrame *tf) {
    print_kerninfo();
    return 0;
}

/* *
 * mon_backtrace - call print_stackframe in kern/debug/kdebug.c to
 * print a backtrace of the stack.
 * */
int monitor_backtrace(int argc, char **argv, struct TrapFrame *tf) {
    print_stackframe();
    return 0;
}

