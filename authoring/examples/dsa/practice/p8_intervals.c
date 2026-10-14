#include <stdio.h>
#include <stdlib.h>

typedef struct { int start, end; const char *name; } Job;

static int by_end(const void *x, const void *y) { return ((const Job *)x)->end - ((const Job *)y)->end; }
static int by_start(const void *x, const void *y) { return ((const Job *)x)->start - ((const Job *)y)->start; }

static int pick(Job *jobs, int n, const char *label) {
    int count = 0, last_end = -1;
    printf("%-22s", label);
    for (int i = 0; i < n; i++)
        if (jobs[i].start >= last_end) { printf(" %s", jobs[i].name); last_end = jobs[i].end; count++; }
    printf("  -> %d jobs\n", count);
    return count;
}

int main(void) {
    Job jobs[] = {{0, 9, "A"}, {1, 3, "B"}, {2, 5, "C"}, {4, 6, "D"}, {6, 8, "E"}, {5, 7, "F"}, {8, 10, "G"}};
    int n = (int)(sizeof jobs / sizeof jobs[0]);
    qsort(jobs, (size_t)n, sizeof jobs[0], by_end);
    pick(jobs, n, "earliest finish first:");
    qsort(jobs, (size_t)n, sizeof jobs[0], by_start);
    pick(jobs, n, "earliest start first:");
    return 0;
}
