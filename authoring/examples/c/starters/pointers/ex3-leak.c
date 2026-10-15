// Exercise 3: student_free must release everything student_new allocated.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The tests count allocations: malloc, calloc and free below go through these counters. */
static int live_blocks;
static void *counted_malloc(size_t n) { void *p = malloc(n); if (p) live_blocks++; return p; }
static void *counted_calloc(size_t k, size_t n) { void *p = calloc(k, n); if (p) live_blocks++; return p; }
static void counted_free(void *p) { if (p) live_blocks--; free(p); }
#define malloc(n) counted_malloc(n)
#define calloc(k, n) counted_calloc(k, n)
#define free(p) counted_free(p)

typedef struct { char *name; int *scores; int n; } Student;

static Student *student_new(const char *name, int n) {
    Student *s = malloc(sizeof *s);
    s->name = malloc(strlen(name) + 1);
    strcpy(s->name, name);
    s->scores = calloc((size_t)n, sizeof(int));
    s->n = n;
    return s;
}

/* ===== Your code: edit below this line ===== */
void student_free(Student *s) {
    free(s->name);
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
    Student *s = student_new("Ada", 3);
    check_int("blocks allocated by student_new", live_blocks, 3);
    student_free(s);
    check_int("blocks still allocated after student_free", live_blocks, 0);
    Student *t = student_new("Grace", 0);
    student_free(t);
    check_int("blocks still allocated after a second student", live_blocks, 0);
    return summary();
}
