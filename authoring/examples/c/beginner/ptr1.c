#include <stdio.h>

static void add_one_wrong(int n) { n = n + 1; }          /* receives a COPY: the caller's variable is untouched */
static void add_one(int *n)      { *n = *n + 1; }        /* receives the ADDRESS: can change the caller's variable */
static void swap(int *a, int *b) { int t = *a; *a = *b; *b = t; }

int main(void) {
    int age = 30;
    int *p = &age;                    /* p holds the address of age ("p points to age") */

    printf("age = %d\n", age);
    printf("*p  = %d   (follow the pointer to read what it points at)\n", *p);
    printf("p == &age ? %s\n", p == &age ? "yes" : "no");

    *p = 31;                          /* writing through the pointer changes age itself */
    printf("after *p = 31: age = %d\n", age);

    add_one_wrong(age); printf("add_one_wrong(age): age = %d  (unchanged)\n", age);
    add_one(&age);      printf("add_one(&age):      age = %d\n", age);

    int x = 1, y = 2; swap(&x, &y);
    printf("swap: x = %d, y = %d\n", x, y);

    printf("sizeof(int) = %zu bytes, sizeof(int *) = %zu bytes (an address is a 64-bit number here)\n", sizeof(int), sizeof(int *));
    return 0;
}
