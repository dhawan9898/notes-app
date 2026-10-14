#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *name; int (*op)(int, int); } Command;      /* a dispatch table */

static int add(int a, int b) { return a + b; }
static int mul(int a, int b) { return a * b; }
static int sub(int a, int b) { return a - b; }

typedef struct { const char *name; int age; } Person;

static int by_age(const void *x, const void *y) {
    const Person *a = x, *b = y;
    return (a->age > b->age) - (a->age < b->age);
}
static int by_name(const void *x, const void *y) {
    return strcmp(((const Person *)x)->name, ((const Person *)y)->name);
}

int main(void) {
    const Command table[] = {{"add", add}, {"sub", sub}, {"mul", mul}};
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++)
        printf("%s(6, 3) = %d\n", table[i].name, table[i].op(6, 3));

    Person people[] = {{"Linus", 55}, {"Ada", 36}, {"Grace", 85}, {"Dennis", 70}};
    size_t n = sizeof people / sizeof people[0];
    qsort(people, n, sizeof people[0], by_age);
    printf("by age:  "); for (size_t i = 0; i < n; i++) printf("%s(%d) ", people[i].name, people[i].age); printf("\n");
    qsort(people, n, sizeof people[0], by_name);
    printf("by name: "); for (size_t i = 0; i < n; i++) printf("%s ", people[i].name); printf("\n");
    return 0;
}
