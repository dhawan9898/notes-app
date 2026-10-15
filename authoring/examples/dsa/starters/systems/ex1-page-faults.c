// Exercise 1: count the misses of FIFO, LRU and OPT replacement for a reference string.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* refs[0..n) are page numbers 0..99; the cache holds "frames" pages (at most 16) and starts empty. */
int misses_fifo(const int *refs, int n, int frames) { (void)refs; (void)frames; return n; }
int misses_lru(const int *refs, int n, int frames)  { (void)refs; (void)frames; return n; }
int misses_opt(const int *refs, int n, int frames)  { (void)refs; (void)frames; return n; }
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
    const int refs[] = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    const int fifo[] = {9, 10, 5}, lru[] = {10, 8, 5}, opt[] = {7, 6, 5};
    for (int frames = 3; frames <= 5; frames++) {
        char what[40];
        snprintf(what, sizeof what, "FIFO misses with %d frames", frames);
        check_int(what, misses_fifo(refs, 12, frames), fifo[frames - 3]);
        snprintf(what, sizeof what, "LRU misses with %d frames", frames);
        check_int(what, misses_lru(refs, 12, frames), lru[frames - 3]);
        snprintf(what, sizeof what, "OPT misses with %d frames", frames);
        check_int(what, misses_opt(refs, 12, frames), opt[frames - 3]);
    }
    return summary();
}
