#include <stdio.h>
#include <ulib.h>
#include <types.h>

#define A_SIZE  (1024 * 1024)

uint32_t big_array[A_SIZE];

int main(void) {
    printf("making sure bss works right...\n");
    int i;
    for (i = 0; i < A_SIZE; i++) {
        if (big_array[i] != 0) {
            panic("big_array[%d] isn't cleared\n", i);
        }
    }
    for (i = 0; i < A_SIZE; i++) {
        big_array[i] = i;
    }

    for (i = 0; i < A_SIZE; i++) {
        if (big_array[i] != i) {
            panic("big_array[%d] didn't hold its value\n");
        }
    }

    printf("Yes, good. Now doint a wild write off the end...\n");
    printf("test_bss may pass.\n");
    big_array[A_SIZE + 1024] = 0;
    asm volatile("int $0x14");
    panic("FAIL: \n");

}