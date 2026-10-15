// Exercise 3: counting sort of ages 0 to 10, without comparisons, and stable.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int age; char name; } Person;
#define MAXV 10

/* ===== Your code: edit below this line ===== */
/* Sort in[0..n) by age into out[0..n). People with equal ages must keep their input order. */
void counting_sort(const Person *in, Person *out, int n) {
    for (int i = 0; i < n; i++) out[i] = in[i];
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
    Person in[] = {{7, 'a'}, {3, 'b'}, {10, 'c'}, {3, 'd'}, {0, 'e'}, {7, 'f'}, {7, 'g'}, {1, 'h'}, {10, 'i'}, {3, 'j'}};
    Person out[10];
    counting_sort(in, out, 10);
    char got[64] = "";
    int k = 0;
    for (int i = 0; i < 10; i++) k += snprintf(got + k, sizeof got - (size_t)k, i ? " %d%c" : "%d%c", out[i].age, out[i].name);
    check_str("counting_sort (age and name)", got, "0e 1h 3b 3d 3j 7a 7f 7g 10c 10i");
    return summary();
}
