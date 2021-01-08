#include <ulib.h>
#include <stdio.h>

int main(void) {
    int i;
    printf("Hello, I am process %d.\n", getpid());
    for (i = 0; i < 5; i++) {
        yield();
        printf("Back in process %d, iteration %d.\n", getpid(), i);
    }
    printf("All done in process %d.\n", getpid());
    printf("yield pass.\n");
    return 0;
}