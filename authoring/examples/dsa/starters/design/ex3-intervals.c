// Exercise 3: the largest set of non-overlapping jobs (a job may start when another ends).
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int start, end; const char *name; } Job;

/* ===== Your code: edit below this line ===== */
/* Return the maximum number of jobs that can run one after another. You may reorder jobs. */
int max_jobs(Job *jobs, int n) {
    (void)jobs;
    return n > 0;
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
    Job jobs[] = {{0, 9, "A"}, {1, 3, "B"}, {2, 5, "C"}, {4, 6, "D"}, {6, 8, "E"}, {5, 7, "F"}, {8, 10, "G"}};
    check_int("max_jobs(A..G)", max_jobs(jobs, 7), 4);
    Job chain[] = {{2, 3, "y"}, {0, 1, "w"}, {1, 2, "x"}};
    check_int("max_jobs(back-to-back jobs)", max_jobs(chain, 3), 3);
    Job none[1];
    check_int("max_jobs(no jobs)", max_jobs(none, 0), 0);
    return summary();
}
