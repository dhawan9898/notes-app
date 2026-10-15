// Macro techniques from C11 and C23: _Generic dispatch, __VA_OPT__, typeof temporaries, static_assert.
#include <stdio.h>

// One PRINT macro for several types, chosen at compile time by the type of the argument.
#define FMT(x) _Generic((x), int: "%d", long: "%ld", double: "%g", char *: "%s", const char *: "%s")
#define PRINT(x) (printf(#x " = "), printf(FMT(x), (x)), putchar('\n'))

// __VA_OPT__(,) emits the comma only when arguments follow the format (C23; replaces GNU ", ##__VA_ARGS__").
#define LOG(fmt, ...) printf("[log] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)

// Single evaluation with typeof temporaries; comparing the addresses makes mixed types a diagnostic.
#define MIN(a, b) ({ typeof(a) a_ = (a); typeof(b) b_ = (b); (void)(&a_ == &b_); a_ < b_ ? a_ : b_; })

// A compile-time check that fails the build, not the program.
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
static int table[] = {4, 8, 15, 16, 23, 42};
static_assert(ARRAY_LEN(table) == 6, "table must have six entries");

static int counter;
static int next(void) { return ++counter; }

int main(void) {
    PRINT(42);
    PRINT(42L);
    PRINT(2.5);
    PRINT("text");
    LOG("no arguments");
    LOG("%zu entries in table, last = %d", ARRAY_LEN(table), table[ARRAY_LEN(table) - 1]);
    int m = MIN(next(), 10);
    printf("MIN(next(), 10) = %d, next() was called %d time(s)\n", m, counter);
#ifdef SHOW_TYPE_CHECK
    long big = 5;
    (void)MIN(big, 3);          /* int vs long: comparison of distinct pointer types */
#endif
    return 0;
}
