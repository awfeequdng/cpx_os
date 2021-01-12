#include <stdio.h>
#include <ulib.h>
#include <string.h>
#include <malloc.h>
#include <types.h>

const int SIZE = 5 * 1024 * 1024;
char *buffer;

int pid[10] = {0}, pids;

void do_yield(void) {
    int i;
    for (i = 0; i < 5; i++) {
        yield();
    }
}



void work(int num, bool print) {
#define debug_printf(...)   \
    do {                    \
        if (print) {        \
            printf(__VA_ARGS__);    \
        }                           \
    } while (0)

    do_yield();
    int i, j;
    for (i = 0; i < SIZE; i++) {
        assert(buffer[i] == (char)(i * i));
    }

    debug_printf("pid = %d, check cow ok.\n", num);

    char c = (char)num;

    for (i = 0; i < 5; i++, c++) {
        debug_printf("pid = %d, round %d\n", num, i);
        memset(buffer, c, SIZE);
        for (j = 0; j < SIZE; j++) {
            assert(buffer[i] == c);
        }
    }

    do_yield();

    debug_printf("pid = %d, child check ok.\n", num);

#undef debug_printf
}


int main(void) {
    assert((buffer = malloc(SIZE)) != NULL);
    printf("buffer size = %08x\n", SIZE);

    pids = ARRAY_SIZE(pid);

    int i;
    for (i = 0; i < SIZE; i++) {
        buffer[i] = (char)(i * i);
    }

    for (i = 0; i < pids; i++) {
        if ((pid[i] = fork()) == 0) {
            sleep((pids - 1) * 10);
            printf("child %d fork ok, pid = %d.\n", i, getpid());
            work(getpid(), true);
            exit(0xbee);
        }
        if (pid[i] < 0) {
            goto failed;
        }
    }
    printf("parent init ok.\n");

    for (i = 0; i < pids; i++) {
        int exit_code, ret;
        if ((ret = waitpid(pid[i], &exit_code)) == 0) {
            if (exit_code == 0xbee) {
                continue;
            }
        }
        printf("wait failed: %d, pid = %d, ret = %d, exit = %x.\n",
               i, pid[i], ret, exit_code);
        goto failed;
    }

    printf("wait ok.\n");
    for (i = 0; i < SIZE; i++) {
        assert(buffer[i] == (char)(i * i));
    }

    printf("check buffer ok.\n");
    printf("swap_test pass.\n");
    return 0;

failed:
    for (i = 0; i < pids; i++) {
        if (pid[i] > 0) {
            kill(pid[i]);
        }
    }
    panic("===============fail==============\n");
}