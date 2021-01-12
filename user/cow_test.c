#include <stdio.h>
#include <ulib.h>
#include <string.h>
#include <malloc.h>

typedef struct slot_t {
    char data[4096];
    struct slot_t *next;
} Slot;

Slot *expand(int num) {
    Slot *tmp = NULL;
    Slot *head = NULL;
    while (num > 0) {
        tmp = (Slot *)malloc(sizeof(Slot));
        tmp->data[0] = (char)0x11;
        tmp->next = head;
        head = tmp;
        num--;
    }
    return head;
}

Slot *head = NULL;

void parent_test(void) {
    Slot *p = head;
    while (p != NULL) {
        p->data[0] = (char)0xdd;
        p = p->next;
    }

    p = head;
    while (p != NULL) {
        // printf("p->data[0] = %x\n", p->data[0]);
        assert(p->data[0] == (char)0xdd);
        p = p->next;
    }
}

void sweeper(void) {
    // parent_test();

    Slot *p = head;
    int i = 0;
    while (p != NULL) {
        p->data[0] = (char)0xEF;
        p = p->next;
    }
    p = head;
    i = 0;
    while (p != NULL) {
        assert(p->data[0] == (char)0xEF);
        p = p->next;
    }
    exit(0xbeaf);
}

int main(void) {
    printf("before expand\n");
    head = expand(60);

    int pid, exit_code;
    if ((pid = fork()) == 0) {
        // sleep(5000);
        printf("I am the child and then exiting, pid = %d\n", getpid());
        sweeper();
        exit(0xbeaf);
    }
    sleep(1000);
    assert(pid > 0);
    printf("-----------------fork ok.\n");
    parent_test();
    
    assert(waitpid(pid, &exit_code) == 0 && exit_code == 0xbeaf);
    printf("after waitpid\n");
    
    // parent_test();

    printf("cow_test pass.\n");
}