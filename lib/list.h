#ifndef __LIBS_LIST_H__
#define __LIBS_LIST_H__

#include <types.h>

typedef struct list_entry {
    struct list_entry *prev, *next;
} ListEntry;

static inline void list_init(ListEntry *entry) {
    entry->prev = entry->next = entry;
}

static inline void __list_add(ListEntry *entry, ListEntry *prev, ListEntry *next) {
    prev->next = next->prev = entry;
    entry->prev = prev;
    entry->next = next;
}

static inline void __list_del(ListEntry *prev, ListEntry *next) {
    prev->next = next;
    next->prev = prev;
}

static inline void list_add_after(ListEntry *head, ListEntry *entry) {
    __list_add(entry, head, head->next);
}

// 在head后面添加一个元素
static inline void list_add(ListEntry *head, ListEntry *entry) {
    list_add_after(head, entry);
}

static inline void list_add_before(ListEntry *head, ListEntry *entry) {
    __list_add(entry, head->prev, head);
}

static inline void list_del(ListEntry *entry) {
    __list_del(entry->prev, entry->next);
}

static inline void list_del_init(ListEntry *entry) {
    list_del(entry);
    list_init(entry);
}

static inline bool list_empty(ListEntry *head) {
    return head->next == head;
}

static inline ListEntry *list_next(ListEntry *entry) {
    return entry->next;
}

static inline ListEntry *list_prev(ListEntry *entry) {
    return entry->prev;
}

#endif // __LIBS_LIST_H__