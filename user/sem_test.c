#include <stdio.h>
#include <ulib.h>

void sem_test(void) {
    sem_t sem_id = sem_init(1);
    assert(sem_id > 0);

    int i, value;
    for (i = 0; i < 10; i++) {
        assert(sem_get_value(sem_id, &value) == 0);
        assert(value == i + 1 && sem_post(sem_id) == 0);
    }
    printf("post ok.\n");

    for (; i > 0; i--) {
        assert(sem_wait(sem_id) == 0);
        assert(sem_get_value(sem_id, &value) == 0 && value == i);
    }
    printf("wait ok.\n");

    int pid, ret;
    // fork没有共享sem
    if ((pid = fork()) == 0) {
        assert(sem_get_value(sem_id, &value) == 0);
        assert(value == 1 && sem_wait(sem_id) == 0);

        sleep(10);
        for (i = 0; i < 10; i ++) {
            printf("sleep %d\n", i);
            sleep(20);
        }
        assert(sem_post(sem_id) == 0);
        exit(0);
    }
    assert(pid > 0);

    sleep(10);
    for (i = 0; i < 10; i ++) {
        yield();
    }

    printf("wait semaphore...\n");

    assert(sem_wait(sem_id) == 0);
    assert(sem_get_value(sem_id, &value) == 0 && value == 0);
    printf("hold semaphore.\n");

    assert(waitpid(pid, &ret) == 0 && ret == 0);
    assert(sem_get_value(sem_id, &value) == 0 && value == 0);
    printf("fork pass.\n");
    exit(0);
}

int main(void) {
    int pid, ret;
    if ((pid = fork()) == 0) {
        sem_test();
    }
    assert(pid > 0 && waitpid(pid, &ret) == 0 && ret == 0);
    printf("semtest pass.\n");
    return 0;
}

