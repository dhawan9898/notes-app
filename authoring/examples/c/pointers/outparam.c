// Pointers to pointers: out-parameters, a list edited through `struct node **`, and the shape of argv.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Wrong: `out` is a copy of the caller's pointer, so the caller never sees the new buffer (and it leaks).
static int make_buffer_wrong(size_t n, char *out) {
    out = malloc(n);
    if (!out) return -1;
    memset(out, 'x', n);
    return 0;
}

// Right: receive the address of the caller's pointer and store through it.
static int make_buffer(size_t n, char **out) {
    char *p = malloc(n);
    if (!p) return -1;
    memset(p, 'x', n - 1);
    p[n - 1] = '\0';
    *out = p;                          // the only write the caller observes
    return 0;
}

struct node { int value; struct node *next; };

static void push(struct node **head, int v) {
    struct node *n = malloc(sizeof *n);
    n->value = v;
    n->next = *head;
    *head = n;
}

// `pp` points at whichever pointer currently leads to the node: the head variable or a previous node's `next`.
// The head is therefore not a special case.
static int remove_value(struct node **pp, int v) {
    for (; *pp; pp = &(*pp)->next) {
        if ((*pp)->value == v) {
            struct node *dead = *pp;
            *pp = dead->next;
            free(dead);
            return 1;
        }
    }
    return 0;
}

static void print_list(const char *label, const struct node *n) {
    printf("%-22s", label);
    for (; n; n = n->next) printf(" %d", n->value);
    printf("\n");
}

int main(int argc, char **argv) {
    char *buf = NULL;
    make_buffer_wrong(8, buf);
    printf("after make_buffer_wrong: buf is %s\n", buf ? "set" : "still NULL (the callee changed its own copy)");
    if (make_buffer(8, &buf) == 0) printf("after make_buffer:       buf = \"%s\"\n", buf);
    free(buf);

    struct node *head = NULL;
    for (int v = 1; v <= 5; v++) push(&head, v);
    print_list("list:", head);
    remove_value(&head, 5);            // the head node
    print_list("removed 5 (the head):", head);
    remove_value(&head, 3);            // a middle node
    print_list("removed 3 (middle):", head);
    remove_value(&head, 1);            // the tail
    print_list("removed 1 (tail):", head);
    while (head) remove_value(&head, head->value);

    // argv is a pointer to the first element of an array of argc + 1 pointers; the last one is NULL (C11 5.1.2.2.1).
    printf("argc = %d, argv[argc] is %s\n", argc, argv[argc] == NULL ? "NULL" : "not NULL");
    for (char **p = argv + 1; *p; p++) printf("  arg %td: \"%s\" (%zu chars)\n", p - argv, *p, strlen(*p));
    return 0;
}
