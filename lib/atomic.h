#ifndef __LIBS_ATOMIC_H__
#define __LIBS_ATOMIC_H__

#define LOCK_PREFIX ""

typedef struct {
    volatile int counter;
} atomic_t;


static inline int atomic_read(const atomic_t *v) {
    return v->counter;
}

static inline void atomic_set(atomic_t *v, int i) {
    v->counter = i;
}

static inline void atomic_add(atomic_t *v, int i) {
    asm volatile(LOCK_PREFIX"addl %1, %0": "+m"(v->counter) : "ir" (i));
}

static inline void atomic_sub(atomic_t *v, int i) {
    asm volatile(LOCK_PREFIX"subl %1, %0" : "+m" (v->counter) : "ir" (i));
}

static inline bool atomic_sub_test_zero(atomic_t *v, int i) {
    unsigned char c = 0;
    // 这可不是原子指令，有问题啊，后面需要修改
    asm volatile(LOCK_PREFIX"subl %2, %0; sete %1" : "+m" (v->counter), "=qm" (c) : "ir" (i));
    return c != 0;
}

static inline void atomic_inc(atomic_t *v) {
    asm volatile(LOCK_PREFIX"incl %0": "+m"(v->counter));
}

static inline void atomic_dec(atomic_t *v) {
    asm volatile(LOCK_PREFIX"decl %0": "+m" (v->counter));
}

static inline bool atomic_inc_test_zero(atomic_t *v) {
    unsigned int c;
    asm volatile(LOCK_PREFIX"incl %0; sete %1" : "+m"(v->counter), "=qm" (c) :: "memory");
    return c != 0;
}


static inline bool atomic_dec_test_zero(atomic_t *v) {
    unsigned int c;
    asm volatile(LOCK_PREFIX"decl %0; sete %1" : "+m"(v->counter), "=qm" (c) :: "memory");
    return c != 0;
}

static inline int atomic_add_return(atomic_t *v, int i) {
    int __i = i;
    asm volatile(LOCK_PREFIX"xaddl %0, %1" : "+r"(i), "+m"(v->counter) :: "memory");
    return i + __i;
}

static inline int atomic_sub_return(atomic_t *v, int i) {
    return atomic_add_return(v, -i);
}

static inline void set_bit(int nr, volatile void *addr) {
    asm volatile("btsl %1, %0" : "=m" (*(volatile long *)addr) : "Ir" (nr));
}

static inline void clear_bit(int nr, volatile void *addr) {
    asm volatile("btrl %1, %0" : "=m" (*(volatile long *)addr) : "Ir" (nr));
}

static inline void change_bit(int nr, volatile void *addr) {
    asm volatile("btcl %1, %0" : "=m" (*(volatile long *)addr) : "Ir" (nr));
}

// sbbl位带cf的减法
// 将nr位置1，然后返回这一位上的旧值
/*
BT、BTS、BTR、BTC: 位测试指令
;BT(Bit Test):                 位测试
;BTS(Bit Test and Set):        位测试并置位
;BTR(Bit Test and Reset):      位测试并复位
;BTC(Bit Test and Complement): 位测试并取反
;它们的结果影响 CF，也就是会将原来的位赋值到CF中再做位操作
*/
static inline bool test_and_set_bit(int nr, volatile void *addr) {
    int oldbit;
    asm volatile("btsl %2, %1; sbbl %0, %0" : "=r" (oldbit), "=m"(*(volatile long *)addr) : "Ir" (nr) : "memory");
    return oldbit != 0;
}

static inline bool test_and_clear_bit(int nr, volatile void * addr) {
    int oldbit;
    asm volatile("btrl %2, %1; sbbl %0, %0" : "=r" (oldbit), "=m" (*(volatile long *)addr) : "Ir" (nr) : "memory");
    return oldbit != 0;
}

static inline bool test_and_change_bit(int nr, volatile void *addr) {
    int oldbit;
    asm volatile("btcl %2, %1; sbbl %0, %0" : "=r" (oldbit), "=m" (*(volatile long *)addr) : "Ir" (nr) : "memory");
    return oldbit != 0;
}

static inline bool test_bit(int nr, volatile void *addr) {
    int oldbit;
    asm volatile("btl %2, %1; sbbl %0, %0" : "=r" (oldbit): "m" (*(volatile long *)addr), "Ir" (nr));
    return oldbit != 0;
}


#endif // __LIBS_ATOMIC_H__