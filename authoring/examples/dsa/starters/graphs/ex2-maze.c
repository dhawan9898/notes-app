// Exercise 2: the length of the shortest path from S to E in a grid maze (breadth-first search).
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Moves go up, down, left or right; '#' is a wall. Return the number of steps, or -1 if E cannot be reached. */
int shortest_path(const char *maze[], int rows, int cols) {
    (void)maze; (void)rows; (void)cols;
    return -1;
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
    const char *maze[] = {"S.#......", ".##.####.", "....#....", ".####.##.", "......#.E"};
    check_int("shortest_path(5x9 maze)", shortest_path(maze, 5, 9), 16);
    const char *next_to[] = {"SE"};
    check_int("shortest_path(\"SE\")", shortest_path(next_to, 1, 2), 1);
    const char *walled[] = {"S#E"};
    check_int("shortest_path(\"S#E\") (unreachable)", shortest_path(walled, 1, 3), -1);
    const char *open[] = {"S...", "....", "...E"};
    check_int("shortest_path(open 3x4 room)", shortest_path(open, 3, 4), 5);
    return summary();
}
