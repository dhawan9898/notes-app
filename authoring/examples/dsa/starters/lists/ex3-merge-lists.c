// Exercise 3: merge two sorted lists by relinking nodes (no allocation), keeping equal keys stable.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node { int key; char tag; struct Node *next; } Node;

/* ===== Your code: edit below this line ===== */
/* Return the head of the merged list. Equal keys: nodes from a come before nodes from b. */
Node *merge(Node *a, Node *b) {
    (void)b;
    return a;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static void check_str(const char *what, const char *got, const char *want) {
    checks++;
    if (got && strcmp(got, want) == 0) printf("PASS %s = \"%s\"\n", what, got);
    else { failed++; printf("FAIL %s: expected \"%s\", got %s%s%s\n", what, want, got ? "\"" : "", got ? got : "NULL", got ? "\"" : ""); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    Node a[3] = {{1, 'a', &a[1]}, {5, 'a', &a[2]}, {9, 'a', NULL}};
    Node b[3] = {{2, 'b', &b[1]}, {5, 'b', &b[2]}, {8, 'b', NULL}};
    char got[32] = "";
    int k = 0, foreign = 0;
    for (Node *p = merge(a, b); p && k < 30; p = p->next) {
        if (!((p >= a && p < a + 3) || (p >= b && p < b + 3))) foreign++;
        k += snprintf(got + k, sizeof got - (size_t)k, k ? " %d%c" : "%d%c", p->key, p->tag);
    }
    check_str("merge(1 5 9, 2 5 8)", got, "1a 2b 5a 5b 8b 9a");
    check_int("nodes that were not in the input lists", foreign, 0);
    Node c = {4, 'c', NULL};
    Node *m = merge(NULL, &c);
    check_int("merge(empty, 4) head key", m ? m->key : -1, 4);
    check_int("merge(empty, empty) is NULL", merge(NULL, NULL) == NULL, 1);
    return summary();
}
