// Exercise 3: qsort comparators, by age and by name.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *name; int age; } Person;

/* ===== Your code: edit below this line ===== */
/* Return a negative number, 0 or a positive number, like strcmp. Never return a->age - b->age: it can overflow. */
int by_age(const void *x, const void *y) {
    (void)x; (void)y;
    return 0;
}

int by_name(const void *x, const void *y) {
    (void)x; (void)y;
    return 0;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

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
    Person people[] = {{"Linus", 55}, {"Ada", 36}, {"Grace", 85}, {"Dennis", 70}, {"Big", 2000000000}, {"Small", -2000000000}};
    size_t n = sizeof people / sizeof people[0];
    char order[96];
    qsort(people, n, sizeof people[0], by_age);
    order[0] = '\0';
    for (size_t i = 0; i < n; i++) { strcat(order, people[i].name); if (i + 1 < n) strcat(order, " "); }
    check_str("sorted by age", order, "Small Ada Linus Dennis Grace Big");
    qsort(people, n, sizeof people[0], by_name);
    order[0] = '\0';
    for (size_t i = 0; i < n; i++) { strcat(order, people[i].name); if (i + 1 < n) strcat(order, " "); }
    check_str("sorted by name", order, "Ada Big Dennis Grace Linus Small");
    Person extremes[] = {{"Big", 2000000000}, {"Small", -2000000000}};      /* 2000000000 - (-2000000000) overflows an int: a subtracting comparator gets the sign wrong */
    qsort(extremes, 2, sizeof extremes[0], by_age);
    check_str("two extreme ages sorted by age: first", extremes[0].name, "Small");
    return summary();
}
