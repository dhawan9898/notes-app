#include <stdio.h>

#define N 8
enum { EMPTY = -1, TOMBSTONE = -2 };

static int slot[N];

static void init(void) { for (int i = 0; i < N; i++) slot[i] = EMPTY; }
static int home(int key) { return key % N; }

static void insert(int key) {
    for (int i = 0, p = home(key); i < N; i++, p = (p + 1) % N)
        if (slot[p] == EMPTY || slot[p] == TOMBSTONE) { slot[p] = key; return; }     /* linear probing */
}
static int find(int key) {
    for (int i = 0, p = home(key); i < N; i++, p = (p + 1) % N) {
        if (slot[p] == EMPTY) return -1;                       /* an empty slot ends the search: the key cannot be further on */
        if (slot[p] == key) return p;
    }
    return -1;
}
static void remove_naive(int key) { int p = find(key); if (p >= 0) slot[p] = EMPTY; }
static void remove_tombstone(int key) { int p = find(key); if (p >= 0) slot[p] = TOMBSTONE; }

static void show(const char *label) {
    printf("%-26s", label);
    for (int i = 0; i < N; i++) { if (slot[i] == EMPTY) printf("  . "); else if (slot[i] == TOMBSTONE) printf("  X "); else printf("%3d ", slot[i]); }
    printf("\n");
}

int main(void) {
    init(); insert(8); insert(16); insert(24);               /* 8, 16 and 24 all hash to slot 0, so they probe into 1 and 2 */
    show("after inserting 8,16,24:");
    remove_naive(16);
    show("naive delete of 16:");
    printf("find(24) after naive delete -> %d   (24 is still stored, but the search stopped at the hole)\n", find(24));

    init(); insert(8); insert(16); insert(24);
    remove_tombstone(16);
    show("tombstone delete of 16:");
    printf("find(24) after tombstone    -> %d\n", find(24));
    insert(32);
    show("insert 32 reuses the X:");
    return 0;
}
