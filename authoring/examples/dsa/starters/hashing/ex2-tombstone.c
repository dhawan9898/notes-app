// Exercise 2: deleting from a linear-probing table without breaking later searches.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 8
enum { EMPTY = -1, TOMBSTONE = -2 };
static int slot[N];
static void init(void) { for (int i = 0; i < N; i++) slot[i] = EMPTY; }
static int home(int key) { return key % N; }

/* insert: the first EMPTY or TOMBSTONE slot along the probe sequence */
static void insert(int key) {
    for (int i = 0, p = home(key); i < N; i++, p = (p + 1) % N)
        if (slot[p] == EMPTY || slot[p] == TOMBSTONE) { slot[p] = key; return; }
}

/* find: the slot holding key, or -1. An EMPTY slot ends the search; a TOMBSTONE does not. */
static int find(int key) {
    for (int i = 0, p = home(key); i < N; i++, p = (p + 1) % N) {
        if (slot[p] == EMPTY) return -1;
        if (slot[p] == key) return p;
    }
    return -1;
}

/* ===== Your code: edit below this line ===== */
void remove_key(int key) {
    int p = find(key);
    if (p >= 0) slot[p] = EMPTY;        /* breaks the probe chain of keys stored after p */
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
    init();
    insert(8); insert(16); insert(24);          /* all hash to slot 0, so they sit in slots 0, 1 and 2 */
    remove_key(16);
    check_int("find(16) after removing it", find(16), -1);
    check_int("find(24) after removing 16", find(24), 2);
    check_int("find(8) after removing 16", find(8), 0);
    insert(32);
    check_int("insert(32) reuses the freed slot", find(32), 1);
    remove_key(99);
    check_int("removing a missing key changes nothing", find(24), 2);
    return summary();
}
