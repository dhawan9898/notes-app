#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>

struct node { int value; struct node *next; };

static long sum_recursive(const struct node *n) { return n ? n->value + sum_recursive(n->next) : 0; }   /* not a tail call: addition after the call */
static long sum_tail(const struct node *n, long acc) { return n ? sum_tail(n->next, acc + n->value) : acc; } /* tail call: -O2 turns it into a loop */
static long sum_iterative(const struct node *n) { long s = 0; for (; n; n = n->next) s += n->value; return s; }

int main(int argc, char **argv) {
    long count = argc > 1 ? atol(argv[1]) : 100000;
    struct node *head = NULL;
    for (long i = 0; i < count; i++) { struct node *n = malloc(sizeof *n); n->value = 1; n->next = head; head = n; }
    printf("list of %ld nodes\n", count);
    printf("iterative: %ld\n", sum_iterative(head));
    printf("tail-recursive (-O2 makes it a loop): %ld\n", sum_tail(head, 0));
    fflush(stdout);
    printf("recursive: %ld\n", sum_recursive(head));
    return 0;
}
