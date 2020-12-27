#ifndef __LIBS_LIST_H__
#define __LIBS_LIST_H__

#include <types.h>
typedef struct list_entry {
    struct list_entry *prev, *next;
} list_entry_t;

static inline void list_init(list_entry_t *entry) {
    entry->prev = entry->next = entry;
}

static inline void __list_add(list_entry_t *entry, list_entry_t *prev, list_entry_t *next) {
    prev->next = next->prev = entry;
    entry->prev = prev;
    entry->next = next;
}

static inline void __list_del(list_entry_t *prev, list_entry_t *next) {
    prev->next = next;
    next->prev = prev;
}

static inline void list_add_after(list_entry_t *head, list_entry_t *entry) {
    __list_add(entry, head, head->next);
}

// 在head后面添加一个元素
static inline void list_add(list_entry_t *head, list_entry_t *entry) {
    list_add_after(head, entry);
}

static inline void list_add_before(list_entry_t *head, list_entry_t *entry) {
    __list_add(entry, head->prev, head);
}

static inline void list_del(list_entry_t *entry) {
    __list_del(entry->prev, entry->next);
}

static inline void list_del_init(list_entry_t *entry) {
    list_del(entry);
    list_init(entry);
}

static inline bool list_empty(list_entry_t *head) {
    return head->next == head;
}

static inline list_entry_t *list_next(list_entry_t *entry) {
    return entry->next;
}

static inline list_entry_t *list_prev(list_entry_t *entry) {
    return entry->prev;
}

#endif // __LIBS_LIST_H__