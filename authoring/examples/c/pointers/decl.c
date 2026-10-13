// Reading declarations: the right-left rule. Each line is compiled, and sizeof/typeid-style output confirms the reading.
#include <stdio.h>

int add(int a, int b) { return a + b; }
int mul(int a, int b) { return a * b; }

int (*pick(int which))(int, int) {            // pick is a function (int) returning a pointer to a function (int, int) returning int
    return which ? mul : add;
}

int main(void) {
    int *a[4];                                // a: array of 4 pointers to int
    int (*b)[4];                              // b: pointer to an array of 4 ints
    int (*c[3])(int, int) = { add, mul, add };// c: array of 3 pointers to functions (int, int) returning int
    int (*(*d)(int))(int, int) = pick;        // d: pointer to a function (int) returning a pointer to a function (int,int) returning int
    const int *e = 0;                         // e: pointer to const int (the int cannot be modified through e)
    int *const f = 0;                         // f: const pointer to int (f cannot be re-pointed)
    const int *const g = 0;                   // g: const pointer to const int
    printf("sizeof a = %zu (4 pointers), sizeof b = %zu (one pointer), sizeof *b = %zu (an array of 4 ints)\n", sizeof a, sizeof b, sizeof *b);
    printf("c[1](6, 7) = %d, d(0)(6, 7) = %d, d(1)(6, 7) = %d\n", c[1](6, 7), d(0)(6, 7), d(1)(6, 7));
    printf("sizeof e = sizeof f = sizeof g = %zu: const changes what the compiler lets you do, not the representation\n", sizeof e);
    (void)a; (void)b; (void)f; (void)g;
    return 0;
}
