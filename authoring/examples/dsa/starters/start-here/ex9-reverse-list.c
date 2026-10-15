// Exercise 9: reverse a singly linked list in place by changing the next pointers.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node { int v; struct Node *next; } Node;

/* ===== Your code: edit below this line ===== */
/* Return the new head. Do not allocate nodes. */
Node *reverse_list(Node *head) {
    return head;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static void print_arr(const int *v, int n) {
    printf("{");
    for (int i = 0; i < n; i++) printf(i ? ", %d" : "%d", v[i]);
    printf("}");
}

static void check_arr(const char *what, const int *got, int got_n, const int *want, int want_n) {
    int same = got_n == want_n;
    for (int i = 0; same && i < want_n; i++) same = got[i] == want[i];
    checks++;
    if (!same) failed++;
    printf("%s %s", same ? "PASS" : "FAIL", what);
    if (same) { printf(" = "); print_arr(got, got_n); }
    else { printf(": expected "); print_arr(want, want_n); printf(", got "); print_arr(got, got_n); }
    printf("\n");
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    Node n4 = {4, NULL}, n3 = {3, &n4}, n2 = {2, &n3}, n1 = {1, &n2};
    int got[8], k = 0;
    for (Node *p = reverse_list(&n1); p && k < 8; p = p->next) got[k++] = p->v;
    const int want[] = {4, 3, 2, 1};
    check_arr("reverse_list(1 -> 2 -> 3 -> 4)", got, k, want, 4);
    Node single = {9, NULL};
    Node *r = reverse_list(&single);
    check_int("reverse_list(single node) head value", r ? r->v : -1, 9);
    check_int("  and its next is NULL", r && r->next == NULL, 1);
    check_int("reverse_list(NULL) is NULL", reverse_list(NULL) == NULL, 1);
    return summary();
}
