// Why `char **` does not convert to `const char **`: the conversion would let a program modify a const object
// without a cast (C11 6.5.16.1, EXAMPLE 3). Compiled only; running it would be undefined behavior.
#include <stdio.h>

static void print_all(const char **names, int n) {      // "I will not modify the strings"
    for (int i = 0; i < n; i++) puts(names[i]);
}

static void print_all_ok(const char *const *names, int n) {   // safe, but C (unlike C++) still requires a cast
    for (int i = 0; i < n; i++) puts(names[i]);
}

int main(void) {
    char a[] = "alpha", b[] = "beta";
    char *names[] = {a, b};
    print_all(names, 2);                // diagnostic: nested pointer types differ in qualifiers
    print_all_ok((const char *const *)names, 2);   // explicit cast: harmless here because every level is const

    const char c = 'A';
    char *p;
    const char **cpp = &p;              // the conversion the language forbids ...
    *cpp = &c;                          // ... because now p points to the const object c
    *p = 'B';                           // and this writes to it with no cast in sight
    return c;
}
