#include <stdio.h>
#include <ulib.h>
#include <malloc.h>

typedef struct slot_t {
    char data[4096];
    struct slot_t *next;
} Slot;

int main(void) {
    int pid, exit_code;
    if ((pid = fork()) == 0) {
        Slot *tmp = NULL; 
        Slot *head = NULL;
        int n = 0, m = 0;
        printf("I am child\n");
        printf("I am going to eat out all the memory.\n");
        while ((tmp = (Slot *)malloc(sizeof(Slot))) != NULL) {
            n++;
            m++;
            if (m == 1000) {
                printf("I ate %d slots.\n", m);
                m = 0;
            }
            tmp->next = head;
            head = tmp;
        }
        printf("n = %d, m = %d\n", n, m);
        exit(0xdead);
    }
    assert(pid > 0);
    int ret = waitpid(pid, &exit_code);
    printf("pid = %d, exit_code = 0x%x\n", pid, exit_code);
    // todo: 为什么exit code不是0xdead
    assert(ret == 0 && exit_code != 0xdead);
    printf("child is killed by kernel, en.\n");

    assert(wait() != 0);
    printf("bad_brk_test pass.\n");
    return 0;
}