// Exercise 2: two correct replacements for a function that returned the address of a local array.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Fix 1: the caller supplies the buffer and its size; write "Hello, <name>" into it (truncate if it does not fit). */
void make_greeting_buf(const char *name, char *out, size_t cap) {
    (void)name; (void)out; (void)cap;
}

/* Fix 2: return a newly allocated "Hello, <name>" that the caller frees, or NULL if memory runs out. */
char *make_greeting_heap(const char *name) {
    (void)name;
    return NULL;
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
    char mine[32] = "";
    make_greeting_buf("Ada", mine, sizeof mine);
    check_str("make_greeting_buf(\"Ada\", 32-byte buffer)", mine, "Hello, Ada");
    char small[8] = "";
    make_greeting_buf("Grace", small, sizeof small);
    check_str("make_greeting_buf(\"Grace\", 8-byte buffer) truncates", small, "Hello, ");
    char *h = make_greeting_heap("Linus");
    check_str("make_greeting_heap(\"Linus\")", h, "Hello, Linus");
    free(h);
    char *long_name = make_greeting_heap("Margaret Hamilton, Apollo flight software");
    check_str("make_greeting_heap(long name) is not truncated", long_name, "Hello, Margaret Hamilton, Apollo flight software");
    free(long_name);
    return summary();
}
