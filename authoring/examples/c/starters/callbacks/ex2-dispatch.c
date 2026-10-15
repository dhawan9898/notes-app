// Exercise 2: a dispatch table of {name, function pointer} entries, called without a switch.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *name; int (*op)(int, int); } Command;

/* ===== Your code: edit below this line ===== */
int add(int a, int b) { (void)a; (void)b; return 0; }
int sub(int a, int b) { (void)a; (void)b; return 0; }
int mul(int a, int b) { (void)a; (void)b; return 0; }

/* Point each entry at its function. */
const Command table[] = {{"add", NULL}, {"sub", NULL}, {"mul", NULL}};
const size_t table_len = sizeof table / sizeof table[0];
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
    const char *names[] = {"add", "sub", "mul"};
    const int want[] = {9, 3, 18};
    for (int k = 0; k < 3; k++) {
        char what[48];
        snprintf(what, sizeof what, "%s(6, 3) through the table", names[k]);
        const Command *c = NULL;
        for (size_t i = 0; i < table_len; i++)              /* look the name up: no switch */
            if (strcmp(table[i].name, names[k]) == 0) c = &table[i];
        if (!c || !c->op) { checks++; failed++; printf("FAIL %s: no function pointer in the table\n", what); continue; }
        check_int(what, c->op(6, 3), want[k]);
    }
    return summary();
}
