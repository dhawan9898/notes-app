#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- 1. function pointer syntax and typedef ---- */
typedef int (*binop)(int, int);
static int add(int a, int b) { return a + b; }
static int mul(int a, int b) { return a * b; }

static int apply(binop f, int a, int b) { return f(a, b); }

/* ---- 2. qsort with comparison callbacks (and the classic comparator bug) ---- */
struct person { const char *name; int age; };

static int by_age(const void *pa, const void *pb) {
    const struct person *a = pa, *b = pb;
    return (a->age > b->age) - (a->age < b->age);        /* never return a - b: that can overflow */
}
static int by_name(const void *pa, const void *pb) {
    const struct person *a = pa, *b = pb;
    return strcmp(a->name, b->name);
}
static int bad_cmp(const void *pa, const void *pb) {
    return *(const int *)pa - *(const int *)pb;           /* BUG: overflows for INT_MIN / INT_MAX */
}

/* ---- 3. callbacks with a context pointer (closure emulation) ---- */
typedef void (*visit_fn)(int value, void *ctx);

static void for_each(const int *a, size_t n, visit_fn fn, void *ctx) {
    for (size_t i = 0; i < n; i++) fn(a[i], ctx);
}
struct sum_ctx { long sum; int max; };
static void accumulate(int v, void *ctx) {
    struct sum_ctx *c = ctx;
    c->sum += v;
    if (v > c->max) c->max = v;
}

/* ---- 4. a vtable: polymorphism in C ---- */
struct shape;
struct shape_ops {
    double (*area)(const struct shape *);
    const char *(*name)(const struct shape *);
};
struct shape { const struct shape_ops *ops; };

struct circle { struct shape base; double r; };
struct rect   { struct shape base; double w, h; };

static double circle_area(const struct shape *s) { const struct circle *c = (const struct circle *)s; return 3.14159265 * c->r * c->r; }
static double rect_area(const struct shape *s)   { const struct rect *r = (const struct rect *)s; return r->w * r->h; }
static const char *circle_name(const struct shape *s) { (void)s; return "circle"; }
static const char *rect_name(const struct shape *s)   { (void)s; return "rect"; }

static const struct shape_ops circle_ops = { circle_area, circle_name };
static const struct shape_ops rect_ops   = { rect_area, rect_name };

int main(void) {
    binop table[] = { add, mul };                      /* dispatch table */
    printf("apply(add,6,7)=%d apply(mul,6,7)=%d table[1](3,4)=%d\n", apply(add, 6, 7), apply(mul, 6, 7), table[1](3, 4));

    struct person ps[] = { {"Carol", 41}, {"alice", 29}, {"Bob", 35} };
    qsort(ps, 3, sizeof ps[0], by_age);
    printf("by age:  "); for (int i = 0; i < 3; i++) printf("%s(%d) ", ps[i].name, ps[i].age); puts("");
    qsort(ps, 3, sizeof ps[0], by_name);
    printf("by name: "); for (int i = 0; i < 3; i++) printf("%s ", ps[i].name); puts("(strcmp is case-sensitive: 'alice' sorts after 'Carol')");

    int vals[] = { 2147483647, -5, 7 };
    qsort(vals, 3, sizeof vals[0], bad_cmp);
    printf("bad_cmp result: %d %d %d  <- not sorted ascending: subtraction overflowed\n", vals[0], vals[1], vals[2]);

    int data[] = { 4, 8, 15, 16, 23, 42 };
    struct sum_ctx ctx = { 0, data[0] };
    for_each(data, 6, accumulate, &ctx);
    printf("for_each: sum=%ld max=%d\n", ctx.sum, ctx.max);

    struct circle c = { { &circle_ops }, 2.0 };
    struct rect r = { { &rect_ops }, 3.0, 4.5 };
    const struct shape *shapes[] = { &c.base, &r.base };
    for (int i = 0; i < 2; i++) printf("%s area=%.2f\n", shapes[i]->ops->name(shapes[i]), shapes[i]->ops->area(shapes[i]));
    return 0;
}
