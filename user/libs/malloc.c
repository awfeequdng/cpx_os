#include <malloc.h>
#include <ulib.h>
#include <syscall.h>
#include <types.h>

typedef union header {
    struct {
        union header *ptr;
        size_t size;
    } s;
    uint32_t align[16];
} Header;

static Header base;
static Header *freep = NULL;

static bool more_core(size_t num_uints) {
    static_assert(sizeof(Header) == 0x40);
    static uintptr_t brk = 0;
    if (brk == 0) {
        if (sys_brk(&brk) != 0 || brk == 0) {
            return false;
        }
    }
    uintptr_t new_brk = brk + num_uints * sizeof(Header);
    if (sys_brk(&new_brk) != 0 || new_brk <= brk) {
        return false;
    }
    Header *p = (Header *)brk;
    p->s.size = (new_brk - brk) / sizeof(Header);
    free((void *)(p + 1));
    brk = new_brk;
    return true;
} 

void *malloc(size_t size) {
    Header *p = NULL;
    Header *prevp = NULL;
    size_t num_uints;
    // size需要多少个Header大小的块，再加上一个Header头的大小，即总共需要申请多少个Header
    num_uints =((size + sizeof(Header) - 1) / sizeof(Header)) + 1;
    if ((prevp = freep) == NULL) {
        // 当前没有可用内存
        base.s.ptr = freep = prevp = &base;
        base.s.size = 0;
    }

    for (p = prevp->s.ptr; ; prevp = p, p = p->s.ptr) {
        if (p->s.size >= num_uints) {
            if (p->s.size == num_uints) {
                prevp->s.ptr = p->s.ptr;
            } else {
                Header *np = prevp->s.ptr = (p + num_uints);
                np->s.ptr = p->s.ptr;
                np->s.size = p->s.size - num_uints;
                p->s.size = num_uints;
            }
            freep = prevp;
            return (void *)(p + 1);
        }
        // 整个链表都查询了一遍没有找到合适的大小
        if (p == freep) {
            if (!more_core(num_uints)) {
                return NULL;
            }
        }
    }
}

void free(void *ptr) {
    Header *bp = ((Header *)ptr) - 1, *p;
    for (p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr) {
        if (p >= p->s.ptr && (bp > p || bp < p->s.ptr)) {
            break;
        }
    }
    if (bp + bp->s.size != p->s.ptr) {
        bp->s.ptr = p->s.ptr;
    } else {
        bp->s.size += p->s.ptr->s.size;
        bp->s.ptr = p->s.ptr->s.ptr;
    }

    if (p + p->s.size != bp) {
        p->s.ptr = bp;
    } else {
        p->s.size += bp->s.size;
        p->s.ptr = bp->s.ptr;
    }
    
    freep = p;
}