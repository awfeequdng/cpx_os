#include <stdlib.h>
#include <x86.h>

static unsigned long long rand_seed = 1;

// 返回一个伪随机整数
int rand(void) {
    rand_seed = (rand_seed * 0x5deece66dll + 0xbll) & ((1ll << 48) - 1);
    unsigned long long r = (rand_seed >> 12);
    return (int)do_div(r, RAND_MAX + 1);
}

void srand(unsigned int seed) {
    rand_seed = seed;
}