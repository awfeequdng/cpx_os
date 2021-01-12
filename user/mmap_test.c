#include <ulib.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    const int SIZE = 4096;
    void *mapped[10] = {0};

    uintptr_t addr = 0;

    assert(mmap(NULL, SIZE, 0) != 0 && mmap((void *)0xc0000000, SIZE, 0) != 0);

    int i;
    for (i = 0; i < 10; i++) {
        assert(mmap(&addr, SIZE, MMAP_WRITE) == 0 && addr != 0);
        mapped[i] = (void *)addr;
        addr = 0;
    }

    printf("mmap step1 ok.\n");

    for (i = 0; i < 10; i++) {
        assert(munmap((uintptr_t)mapped[i], SIZE) == 0);
    }

    printf("munmap step1 ok.\n");

    addr = 0x80000000;
    assert(mmap(&addr, SIZE, MMAP_WRITE) == 0);
    mapped[0] = (void *)addr;

    addr = 0x80001000;
    assert(mmap(&addr, SIZE, MMAP_WRITE) == 0);
    mapped[1] = (void *)addr;

    printf("mmap step2 ok.\n");

    addr = 0x80001800;
    assert(mmap(&addr, 100, 0) != 0);
    
    printf("mmap step3 ok.\n");

    assert(munmap((uintptr_t)mapped[0], SIZE * 2 + 100) == 0);
    printf("munmap ste2 ok.\n");

    addr = 0;
    assert(mmap(&addr, 128, MMAP_WRITE) == 0 && addr != 0);
    mapped[0] = (void *)addr;

    char *buffer = mapped[0];
    // 内核是按照页来分配地址空间的，虽然申请的是128个字节，但实际给的是4096
    for (i = 0; i < 4000; i++) {
        buffer[i] = (char)(i * i);
    }

    for (i = 0; i < 4000; i++) {
        buffer[i] == (char)(i * i);
    }

    printf("mmap_test pass.\n");

    return 0;
}