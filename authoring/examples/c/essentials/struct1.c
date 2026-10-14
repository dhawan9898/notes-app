#include <stdio.h>

struct point {            /* a new type: two ints that travel together */
    int x;
    int y;
};

static void move_wrong(struct point p) { p.x += 10; }          /* gets a COPY */
static void move_right(struct point *p) { p->x += 10; }        /* gets the address */

int main(void) {
    struct point a = {3, 4};                      /* members in declaration order */
    struct point b = {.y = 9, .x = 1};            /* designated initializers: by name */
    struct point c = a;                           /* assignment copies every member */

    c.x = 100;                                    /* '.' reaches a member of a struct variable */
    printf("a = (%d, %d)  b = (%d, %d)  c = (%d, %d)\n", a.x, a.y, b.x, b.y, c.x, c.y);

    move_wrong(a);
    printf("after move_wrong: a.x = %d\n", a.x);
    move_right(&a);                               /* '->' reaches a member through a pointer */
    printf("after move_right: a.x = %d\n", a.x);

    struct point path[3] = {{0, 0}, {1, 2}, {3, 4}};   /* arrays of structs work like any array */
    int total = 0;
    for (int i = 0; i < 3; i++) total += path[i].x + path[i].y;
    printf("sum of all coordinates = %d, sizeof(struct point) = %zu\n", total, sizeof(struct point));
    return 0;
}
