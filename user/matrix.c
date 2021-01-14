#include <ulib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MATRIX_SIZE     10

static int matrix1[MATRIX_SIZE][MATRIX_SIZE];
static int matrix2[MATRIX_SIZE][MATRIX_SIZE];
static int matrix3[MATRIX_SIZE][MATRIX_SIZE];

void work(unsigned int times) {
    int i, j, k, size = MATRIX_SIZE;
    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            matrix1[i][j] = matrix2[i][j] = 1;
        }
    }

    yield();

    printf("pid %d is running (%d times).\n", getpid(), times);

    while (times--) {
        for (i = 0; i < size; i++) {
            for (j = 0; j < size; j++) {
                matrix3[i][j] = 0;
                for (k = 0; k < size; k++) {
                    matrix3[i][j] += matrix1[i][k] * matrix2[k][j];
                }
            }
        }
        for (i = 0; i < size; i++) {
            for (j = 0; j < size; j++) {
                matrix1[i][j] = matrix2[i][j] = matrix3[i][j];
            }
        }
    }
    printf("pid %d done.\n", getpid());
    exit(0);
}

// const int TOTAL = 20;
#define TOTAL   20

int main(void) {
    int pids[TOTAL] = {0};

    int i;
    for (i = 0; i < TOTAL; i++) {
        if ((pids[i] = fork()) == 0) {
            srand(i * i);
            int times = (((unsigned int)rand()) % TOTAL);
            times = (times * times + 10) * 100;
            work(times);
        }
        if (pids[i] < 0) {
            goto failed;
        }
    }

    printf("fork ok\n");

    for (i = 0; i < TOTAL; i++) {
        if (wait() != 0) {
            printf("wait failed\n");
            goto failed;
        }
    }

    printf("matrix pass.\n");
    return 0;

failed:
    for (i = 0; i < TOTAL; i++) {
        if (pids[i] > 0) {
            kill(pids[i]);
        } 
    }
    panic("FAIL:\n");

}