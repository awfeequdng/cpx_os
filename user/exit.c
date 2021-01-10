#include <stdio.h>
#include <ulib.h>

int magic = -0x10384;

int main(void) {
    int pid, code;
    printf("I am the parent. Forking the child...\n");
    if ((pid = fork()) == 0) {
        printf("I am the child.\n");
        // 让出cpu
        yield();
        yield();
        yield();
        yield();
        yield();
        yield();
        yield();
        exit(magic);
    }
    assert(pid > 0);
    printf("I am the parent, waiting now...\n");
    int ret;
    ret = waitpid(pid, &code);
    printf("pid = %d, ret = %d, code = %d\n", pid, ret, code);
    assert(ret == 0 && code == magic);
    // 当前进程没有子进程了
    assert(waitpid(pid, &code) != 0 && wait() != 0);
    printf("waitpid %d ok.\n", pid);
    printf("exit pass.\n");
    return 0;
}