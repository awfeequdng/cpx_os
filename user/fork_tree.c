#include <ulib.h>
#include <stdio.h>
#include <string.h>

#define DEPTH   4

void fork_tree(const char *current);

void fork_child(const char *current, char branch) {
    char next[DEPTH + 1];
    if (strlen(current) >= DEPTH) {
        return;
    }

    snprintf(next, DEPTH + 1, "%s%c", current, branch);
    if (fork() == 0) {
        fork_tree(next);
        yield();
        exit(0);
    }
}

void fork_tree(const char *current) {
    printf("%04x: I am '%s'\n", getpid(), current);

    fork_child(current, '0');
    fork_child(current, '1');
}

int main(void) {
    fork_tree("");
    return 0;
}