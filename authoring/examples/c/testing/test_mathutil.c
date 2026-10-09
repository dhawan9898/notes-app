#include <limits.h>
#include <stdio.h>
#include "mathutil.h"

/* A minimal xUnit-style harness: no dependencies, one process, clear output. */
static int failures, checks;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { long long _a = (a), _b = (b); checks++; if (_a != _b) { failures++; printf("FAIL %s:%d: %s == %s (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)

static void test_clamp(void) {
    CHECK_EQ(clamp(5, 0, 10), 5);
    CHECK_EQ(clamp(-3, 0, 10), 0);
    CHECK_EQ(clamp(99, 0, 10), 10);
    CHECK_EQ(clamp(0, 0, 0), 0);
}

static void test_safe_add(void) {
    int r = 0;
    CHECK_EQ(safe_add(2, 3, &r), 0);  CHECK_EQ(r, 5);
    CHECK_EQ(safe_add(INT_MAX, 1, &r), -1);
    CHECK_EQ(safe_add(INT_MIN, -1, &r), -1);
    CHECK_EQ(safe_add(INT_MAX, 0, &r), 0);
}

static void test_isqrt(void) {
    CHECK_EQ(isqrt(0), 0);
    CHECK_EQ(isqrt(15), 3);
    CHECK_EQ(isqrt(16), 4);
    /* no test for negative input or INT_MAX: coverage will show the gap */
}

int main(void) {
    test_clamp(); test_safe_add(); test_isqrt();
    printf("%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
