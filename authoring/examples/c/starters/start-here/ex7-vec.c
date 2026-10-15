// Exercise 7: a growable array of ints that doubles its capacity when full.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int *data;
    size_t size;    /* elements in use */
    size_t cap;     /* elements allocated */
} IntVec;

/* ===== Your code: edit below this line ===== */
/* Append x; grow data with realloc (doubling cap) when size == cap. Return 1 on success, 0 if memory runs out. */
int vec_push(IntVec *v, int x) {
    (void)v; (void)x;
    return 0;
}

/* Release the memory and reset the vector to empty. */
void vec_free(IntVec *v) {
    (void)v;
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
    IntVec v = {0};
    int ok = 1;
    for (int i = 0; i < 10; i++) ok &= vec_push(&v, i * i);
    check_int("every vec_push returned 1", ok, 1);
    check_int("size after 10 pushes", (long long)v.size, 10);
    check_int("capacity is at least the size", v.cap >= v.size && v.data != NULL, 1);
    if (v.data && v.size == 10) {
        check_int("data[0]", v.data[0], 0);
        check_int("data[9]", v.data[9], 81);
    }
    vec_free(&v);
    check_int("data is NULL after vec_free", v.data == NULL, 1);
    check_int("size is 0 after vec_free", (long long)v.size, 0);
    return summary();
}
