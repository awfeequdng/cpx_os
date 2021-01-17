#ifndef __LIB_QUEUE_H__
#define __LIB_QUEUE_H__
#include <types.h>
#include <assert.h>

typedef struct {
    uintptr_t *buf;
    volatile off_t wpos;
    volatile off_t rpos;
    size_t size;
} Queue;

static inline void queue_init(Queue *que, uintptr_t *buf, size_t size) {
    que->buf = buf;
    que->wpos = que->rpos = 0;
    que->size = size;
}

static inline bool queue_full(Queue *que) {
    return ((que->wpos + 1) % que->size) == que->rpos;
}

static inline void queue_push(Queue *que, uintptr_t c) {
    if (!queue_full(que)) {
        que->buf[que->wpos] = c;
        que->wpos = (que->wpos + 1) % que->size;
    }
}

static inline bool queue_empty(Queue *que) {
    return que->rpos == que->wpos;
}

static inline uintptr_t queue_pop(Queue *que) {
    assert(!queue_empty(que));
    uintptr_t data = que->buf[que->rpos];
    que->rpos = (que->rpos + 1) % que->size;
    return data;
}






#endif // __LIB_QUEUE_H__