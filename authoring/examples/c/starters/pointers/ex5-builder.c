// Exercise 5: a growable string that doubles its capacity with realloc.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The tests can make the next realloc fail, to check that a failed append keeps the old string. */
static int fail_next_realloc;
void *test_realloc(void *p, size_t n) {
    if (fail_next_realloc) { fail_next_realloc = 0; return NULL; }
    return realloc(p, n);
}
#define realloc(p, n) test_realloc(p, n)

typedef struct { char *data; size_t len, cap; } Builder;

/* ===== Your code: edit below this line ===== */
/* Start with a small buffer holding "" (len 0). Return 1 on success, 0 if memory runs out. */
int sb_init(Builder *b) {
    b->data = NULL;
    b->len = b->cap = 0;
    return 0;
}

/* Append s, doubling cap until it fits. On failure return 0 and leave b exactly as it was. */
int sb_append(Builder *b, const char *s) {
    (void)b; (void)s;
    return 0;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

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
    Builder b;
    check_int("sb_init succeeds", sb_init(&b), 1);
    if (b.data) {
        check_str("empty builder", b.data, "");
        int ok = 1;
        for (int i = 1; i <= 5; i++) {
            char piece[16];
            snprintf(piece, sizeof piece, "item-%d;", i);
            ok &= sb_append(&b, piece);
        }
        check_int("five appends succeed", ok, 1);
        check_str("contents", b.data, "item-1;item-2;item-3;item-4;item-5;");
        check_int("len", (long long)b.len, 35);
        check_int("cap leaves room for the terminating NUL", b.cap > b.len, 1);
        fail_next_realloc = 1;
        check_int("append when realloc fails returns", sb_append(&b, "this one needs to grow the buffer a lot further than it is now"), 0);
        check_str("  and keeps the old string", b.data, "item-1;item-2;item-3;item-4;item-5;");
        check_int("  and the old len", (long long)b.len, 35);
        fail_next_realloc = 0;
        free(b.data);
    }
    return summary();
}
