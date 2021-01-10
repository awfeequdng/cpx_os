#include <stdio.h>
#include <ulib.h>
#include <malloc.h>

typedef struct slot {
    char data[4096];
    struct slot *next;   
} Slot;

void glutton(void) {
    Slot *tmp = NULL;
    Slot *head = NULL;
    int n = 0;
    printf("I am child and I will eat out all the memory.\n");
    while ((tmp = (Slot *)malloc(sizeof(Slot))) != NULL) {
        if (++n % 1000 == 0) {
            // printf("I ate %d slots.\n", n);
            sleep(50);
        }
        tmp->next = head;
        head = tmp;
    }
    exit(0xdead);
}

void sleepy(int pid) {
    int i, time = 100;
    for (i = 0; i < 20; i++) {
        sleep(time);
        printf("sleep %d x %d slices.\n", i + 1, time);
    }
    // 可以给被杀掉了的进程发送kill信号，进程被内核杀死，但是没有接收到过任何kill信号
    // 虽然进程被杀死，为ZOMBIE状态，但是还是可以接收信号
    assert(kill(pid) == 0);
    assert(kill(pid) != 0);
    exit(0);
}

int main(void) {
    unsigned int time = gettime_msec();
    int pid1;
    int pid2;
    int exit_code;
    if ((pid1 = fork()) == 0) {
        glutton();
    }
    assert(pid1 > 0);

    if ((pid2 = fork()) == 0) {
        sleepy(pid1);
    }

    if (pid2 < 0) {
        kill(pid1);
        panic("fork failed.\n");
    }

    assert(waitpid(pid2, &exit_code) == 0 && exit_code == 0);
    printf("use %04d msecs.\n", gettime_msec() - time);
    
    // 当进程由于内存不再被内核杀死时(不是收到kill信号被杀死)，进程会调用do_exit清理资源，此时进程为ZOMBIE状态，并且可以继续接收kill信号
    assert(waitpid(pid1, &exit_code) == 0 && exit_code != 0xdead);
    printf("exit_code = %08x\n", exit_code);
    
    printf("sleep pass.\n");
}