#ifndef __SKIP_LIST_H__
#define __SKIP_LIST_H__

#include <types.h>
#include <malloc.h>
#include <list.h>
#include <stdlib.h>

#define SKIPLIST_MAX_LEVEL       32

typedef struct {
    int level;
    int count;
    ListEntry head[SKIPLIST_MAX_LEVEL];
} SkipList;

typedef struct {
    int key;
    int val;
    ListEntry sk_link[0];
} SkipListNode;

#define le2sknode(le, member)       \
    container_of(le, SkipListNode, member)

static SkipListNode *skiplist_node_new(int level, int key, int val) {
    SkipListNode *node;
    node = (SkipListNode *)malloc(sizeof(*node) + level * sizeof(ListEntry));
    if (node != NULL) {
        node->key = key;
        node->val = val;
    }
    return node;
}

static void skiplist_node_del(SkipListNode *node) {
    free(node);
}

static SkipList *skiplist_new(void) {
    int i;
    SkipList *list = (SkipList *)malloc(sizeof(*list));
    if (list != NULL) {
        list->level = 1;
        list->count = 0;
        for (i = 0; i < ARRAY_SIZE(list->head); i++) {
            list_init(&(list->head[i]));
        }
    }
    return list;
}

static void skiplist_del(SkipList *list) {
    ListEntry *head = &(list->head[0]);
    while (!list_empty(head)) {
        ListEntry *entry = list_next(head);
        // 不需要释放链表，因为此时所有的节点都要被删除，没有必要删除
        SkipListNode *node = le2sknode(entry, sk_link[0]);
        skiplist_node_del(node);
    }
    free(list);
}

static int skiplist_random_level(void) {
    int level = 1;
    const int p = 4;    // 0.25
    while((rand() & 0xffff) * p < 0xffff) {
        level++;
    }
    return level > SKIPLIST_MAX_LEVEL ? SKIPLIST_MAX_LEVEL : level;
}

static SkipListNode *skiplist_search(SkipList *list, int key) {
    SkipListNode *node = NULL;
    int i = list->level - 1;
    ListEntry *end = &(list->head[i]);
    ListEntry *entry = end;

    for (; i >= 0; i--) {
        while ((entry = list_next(entry)) != end) {
            node = le2sknode(entry, sk_link[i]);
            // 找到第一个健值大于等于key的node
            if (node->key >= key) {
                end = &(node->sk_link[i]);
                break;
            }
        }
        if (node->key == key) {
            return node;
        }
        entry = list_prev(entry);
        // 向下一个level查找
        entry--;
        end--;
    }
    // 在level也没有找到key值元素
    return NULL;
}

static SkipListNode *skiplist_insert(SkipList *list, int key, int val) {
    int level = skiplist_random_level();
    if (level > list->level) {
        list->level = level;
    }

    SkipListNode *node = skiplist_node_new(level, key, val);
    if (node != NULL) {
        int i = list->level - 1;
        ListEntry *end = &(list->head[i]);
        ListEntry *entry = end;

        for (; i >= 0; i--) {
            while ((entry = list_next(entry)) != end) {
                SkipListNode *node = le2sknode(entry, sk_link[i]);
                if (node->key >= key) {
                    // 找到了一个比key大（或相等）的点
                    end = entry;
                    break;
                }
            }
            entry = list_prev(entry);
            if (i < level) {
                list_add(entry, &(node->sk_link[i]));
            }
            // 向下一个level查找
            entry--;
            end--;
        }
        list->count++;
    }
    return node;
}

static void skiplist_remove_node(SkipList *list, SkipListNode *node, int level) {
    int i;
    for (i = 0; i < level; i++) {
        list_del(&(node->sk_link[i]));
        if (list_empty(&(list->head[i]))) {
            list->level--;
        }
    }
    list->count--;
    skiplist_node_del(node);
}

static void skiplist_remove(SkipList *list, int key) {
    SkipListNode *node = NULL;
    int i = list->level - 1;
    ListEntry *entry = &(list->head[i]);
    ListEntry *end = entry;
    for (; i >= 0; i--) {
        while ((entry = list_next(entry)) != end) {
            node = le2sknode(entry, sk_link[i]);
            if (node->key > key) {
                end = entry;
                break;
            } else if (node->key == key) {
                skiplist_remove_node(list, node, i + 1);
            }
        }
        entry = list_prev(entry);
        entry--;
        end--;
    }
}

static void skiplist_dump(SkipList *list) {
    SkipListNode *node = NULL;
    int i = list->level - 1;
    ListEntry *end = &(list->head[i]);
    ListEntry *entry = end;
    printf("There are %d nodes in skiplist: \n", list->count);
    for (; i >= 0; i--) {
        printf("skiplist level %d:\n", i + 1);
        while ((entry = list_next(entry)) != end) {
            node = le2sknode(entry, sk_link[i]);
            printf("key:0x%08x val:0x%08x\n", node->key, node->val);
        }
        entry = &(list->head[i]);
        entry--;
        end--;
    }
}

#endif // __SKIP_LIST_H__