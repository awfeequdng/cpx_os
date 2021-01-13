#include <ulib.h>
#include <stdio.h>
#include <thread.h>

int test(void *arg) {
    printf("child ok.\n");
    return 0xbee;
}

int main(void) {
    Thread tid;
    assert(thread(test, NULL, &tid) == 0);
    printf("thread ok.\n");

    int exit_code;
    assert(thread_wait(&tid, &exit_code) == 0 && exit_code == 0xbee);

    printf("thread_test pass.\n");
    return 0;
}