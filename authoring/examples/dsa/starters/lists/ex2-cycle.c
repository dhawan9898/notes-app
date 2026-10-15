// Exercise 2: does a singly linked list contain a cycle? Use O(1) extra memory.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node { int v; struct Node *next; } Node;

/* ===== Your code: edit below this line ===== */
int has_cycle(const Node *head) {
    (void)head;
    return 0;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    Node n[6];
    for (int i = 0; i < 6; i++) { n[i].v = i; n[i].next = i < 5 ? &n[i + 1] : NULL; }
    check_int("has_cycle(0 -> 1 -> 2 -> 3 -> 4 -> 5)", has_cycle(&n[0]), 0);
    n[5].next = &n[2];
    check_int("has_cycle(... 5 -> back to 2)", has_cycle(&n[0]), 1);
    n[5].next = &n[0];
    check_int("has_cycle(... 5 -> back to the head)", has_cycle(&n[0]), 1);
    n[5].next = NULL;
    Node self = {7, NULL};
    self.next = &self;
    check_int("has_cycle(a node pointing to itself)", has_cycle(&self), 1);
    check_int("has_cycle(NULL)", has_cycle(NULL), 0);
    Node one = {1, NULL};
    check_int("has_cycle(single node)", has_cycle(&one), 0);
    return summary();
}
