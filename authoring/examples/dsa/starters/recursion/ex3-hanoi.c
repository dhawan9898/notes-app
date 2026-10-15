// Exercise 3: Tower of Hanoi. Count the moves, then make them.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The tests provide move(from, to) and check that every move is legal. Pegs are 0, 1 and 2. */
void move(int from, int to);

/* ===== Your code: edit below this line ===== */
/* Number of moves for n disks, from the recurrence. */
long long hanoi_moves(int n) {
    (void)n;
    return 0;
}

/* Move n disks from peg "from" to peg "to", using "via", by calling move(). */
void hanoi(int n, int from, int to, int via) {
    (void)n; (void)from; (void)to; (void)via;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int peg[3][16], height[3], made, illegal;

void move(int from, int to) {
    made++;
    if (from < 0 || from > 2 || to < 0 || to > 2 || height[from] == 0) { illegal++; return; }
    int disk = peg[from][height[from] - 1];
    if (height[to] > 0 && peg[to][height[to] - 1] < disk) { illegal++; return; }     /* larger on smaller */
    height[from]--;
    peg[to][height[to]++] = disk;
}

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
    check_int("hanoi_moves(1)", hanoi_moves(1), 1);
    check_int("hanoi_moves(5)", hanoi_moves(5), 31);
    check_int("hanoi_moves(20)", hanoi_moves(20), 1048575);
    check_int("hanoi_moves(63)", hanoi_moves(63), 9223372036854775807LL);
    for (int d = 0; d < 6; d++) peg[0][d] = 6 - d;          /* six disks on peg 0, largest at the bottom */
    height[0] = 6;
    hanoi(6, 0, 2, 1);
    check_int("hanoi(6): illegal moves", illegal, 0);
    check_int("hanoi(6): moves made", made, 63);
    check_int("hanoi(6): disks on peg 2 at the end", height[2], 6);
    return summary();
}
