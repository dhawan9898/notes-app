#include <stdio.h>

static int file_counter = 100;                    /* internal linkage: private to this file, lives for the whole run */
int shared_total = 7;                             /* external linkage: other files may use it with 'extern int shared_total;' */

static int next_id(void) {
    static int id = 0;                            /* local name, but static storage: keeps its value between calls */
    return ++id;
}

static int shadow_demo(void) {
    int x = 1;
    {
        int x = 2;                                /* a new x hides the outer one only inside these braces */
        x += 10;
        printf("inner x = %d\n", x);
    }
    return x;                                     /* the outer x is untouched */
}

int main(void) {
    int a = next_id(), b = next_id(), c = next_id();          /* separate statements: argument order is unspecified */
    printf("ids: %d %d %d\n", a, b, c);
    printf("outer x after the block = %d\n", shadow_demo());
    const int limit = 3;
    const int *p = &limit;
    printf("*p = %d (writing through a pointer-to-const is a compile error)\n", *p);
    printf("file_counter = %d, shared_total = %d\n", file_counter, shared_total);
    return 0;
}
