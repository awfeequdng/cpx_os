#include <ulib.h>
#include <stdio.h>
#include <thread.h>

const int FORK_NUM = 125;

void do_yield(void) {
    int i;
    for (i = 0; i < 20; i ++) {
        sleep(10);
        yield();
    }
}

void process_main(void) {
    do_yield();
}

int thread_main(void *arg) {
    int i, pid;
    for (i = 0; i < FORK_NUM; i++) {
        if ((pid = fork()) == 0) {
            process_main();
            exit(0);
        }
    }
    do_yield();
}

int main(void) {
    Thread tids[10];

    int i;
    int n = ARRAY_SIZE(tids);
    for (i = 0; i < n; i++) {
        if (thread(thread_main, NULL, tids + i) != 0) {
            goto failed;
        }
    }

    int count = 0;
    while (wait() == 0) {
        count++;
    }

    // 所有的线程都以线程leader为父进程
    assert(count == (FORK_NUM + 1) * n);
    return 0;

failed:
    for (i = 0; i < n; i++) {
        if (tids[i].pid > 0) {
            kill(tids[i].pid);
        }
    }
    panic("FAIL:\n");
}