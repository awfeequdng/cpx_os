#include <ulib.h>
#include <stdio.h>

const int MAX_CHILD = 32;

int main(void) {
    int i, pid;
    for (i = 0; i < MAX_CHILD; i++) {
        if ((pid = fork()) == 0) {
            printf("I am child %d\n", i);
            exit(0);
        }
        assert(pid > 0);
    }

    if (i > MAX_CHILD) {
        panic("fork claimed to work %i times!\n", i);
    }

    while (i > 0) {
        if (wait() != 0) {
            panic("wait stopped early\n");
        }
        i--;
    }
    // 此时没有child了
    if (wait() == 0) {
        panic("wait got too many\n");
    }

    printf("fork_test pass.\n");
    return 0;
}