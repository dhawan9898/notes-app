---
title: Function Pointers, Callbacks, Opaque Types and Intrusive Containers
track: c
module: Advanced Language
moduleOrder: 4
order: 2
level: advanced
summary: Polymorphism and abstraction in C — function pointer syntax, dispatch tables and vtables, qsort-style callbacks with context pointers, opaque handles for encapsulation, and the kernel-style intrusive list with container_of, all compiled and run.
prerequisites:
  - Pointers, Arrays and Decay
  - Structs, Alignment, Padding, Unions, Bit-fields and Endianness
sources:
  - label: "ISO/IEC 9899:2018 (C17) §6.7.6.3 Function declarators; §6.3.2.3 Pointers; §7.22.5 Searching and sorting utilities"
  - label: "Linux kernel docs — Linked List API (include/linux/list.h) and container_of"
    url: https://docs.kernel.org/core-api/list.html
  - label: "Hanson — C Interfaces and Implementations (Addison-Wesley, 1996)"
  - label: "SEI CERT C — EXP37-C (call functions with the correct number and type of arguments), API rules"
    url: https://wiki.sei.cmu.edu/confluence/display/c/EXP37-C.+Call+functions+with+the+correct+number+and+type+of+arguments
  - label: "Drepper — How To Write Shared Libraries (ABI and encapsulation)"
    url: https://www.akkadia.org/drepper/dsohowto.pdf
---

C has no classes, interfaces or generics, but it has the primitives to build all three: **pointers to functions**, **structs**, and **opaque types**. Mature C code (the Linux kernel, SQLite, OpenSSL, GLib) relies on these patterns daily.

## Function pointers

A function name decays to a pointer to the function. The declaration syntax is inside-out, so always use a `typedef`:

```c
int (*fp)(int, int);                   /* fp: pointer to function taking (int,int) returning int */
typedef int (*binop)(int, int);        /* readable */
binop f = add;                         /* or &add */
int r = f(2, 3);                       /* or (*f)(2, 3) */
binop table[] = { add, mul };          /* dispatch table */
int (*(*getter)(void))(int);           /* pointer to function returning pointer to function... use typedefs! */
```

Rules and hazards:

- Calling through a pointer whose type doesn't match the function's real signature is **undefined behavior** (e.g. casting `int (*)(int)` to `void (*)(void)` and calling it). Casting between function pointer types and back is allowed; calling is the problem. `-Wcast-function-type` warns.
- Function pointers and data pointers are different kinds in the standard; `void *` ↔ function pointer is not portable (POSIX `dlsym` requires it and GCC allows it with `-pedantic` warnings).
- A `NULL` check before calling an optional callback is your job.
- Comparing function pointers for equality is fine.
- Indirect calls cannot be inlined (usually) and are harder for the CPU to predict; a hot loop calling through a pointer per element is slower than a direct call; JIT-style devirtualization, `switch`, or templates/macros recover speed.
- Control-flow-integrity mitigations (CET, CFI) validate indirect-call targets; function pointers in writable memory are attractive exploit targets (put dispatch tables in `const` data so they land in `.rodata`).

### Callbacks and context pointers

A callback is a function you pass so that library code calls you back. C has no closures, so the idiom pairs the function with a **context pointer** (`void *userdata`) that the library carries opaquely and hands back:

```c
typedef void (*visit_fn)(int value, void *ctx);
void for_each(const int *a, size_t n, visit_fn fn, void *ctx);
```

The standard library's `qsort(base, n, size, cmp)` and `bsearch` use `const void *` arguments because C has no templates: the callback casts them back. (`qsort_r`/`qsort_s` add a context parameter; glibc has `qsort_r`, C11 Annex K `qsort_s`, but argument order differs between platforms.)

A complete tour (`callbacks.c`): dispatch table, `qsort` with two comparators and one buggy comparator, a context-carrying `for_each`, and a vtable:

```c
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
```

```text
$ gcc -std=gnu11 -Wall -Wextra callbacks.c -o callbacks && ./callbacks
apply(add,6,7)=13 apply(mul,6,7)=42 table[1](3,4)=12
by age:  alice(29) Bob(35) Carol(41) 
by name: Bob Carol alice (strcmp is case-sensitive: 'alice' sorts after 'Carol')
bad_cmp result: 2147483647 -5 7  <- not sorted ascending: subtraction overflowed
for_each: sum=108 max=42
circle area=12.57
rect area=13.50
```

Observations:

- A comparator must return negative/zero/positive **without overflowing**: `(a > b) - (a < b)` is the safe idiom. The buggy `*(int*)a - *(int*)b` produced an unsorted result for `INT_MAX`, `-5`, `7`, because `INT_MAX - (-5)` overflows (UB). An inconsistent comparator can also make `qsort` crash or loop (it must define a strict weak ordering).
- `strcmp` sorts by byte value, so uppercase precedes lowercase ("Carol" before "alice"); use `strcasecmp` or `strcoll` for user-facing order.
- `qsort` is not stable; for stable ordering add the original index to the sort key or use a merge sort.

### Vtables: polymorphism by hand

The `shape` example is the C implementation of virtual methods: each object starts with a pointer to a table of function pointers (the **vtable**), shared by all objects of the same "class". A `struct circle` begins with `struct shape base`, so a pointer to the circle can be treated as a pointer to its first member (guaranteed by the standard: a pointer to a struct, suitably converted, points to its first member). Callers use `s->ops->area(s)` without knowing the concrete type. This is how the Linux VFS (`struct file_operations`, `inode_operations`), device drivers, `FILE` in some libcs, SQLite VFS and virtual tables, and OpenSSL `BIO` methods work. Design tips: make the ops table `const`, check required hooks for `NULL` at registration, version the struct (size field) for ABI growth, and give each hook a clear ownership/error contract.

## Opaque types and encapsulation

An **opaque type** exposes only a name in the header and keeps the layout private, achieving information hiding without classes:

```c
#ifndef STACK_H
#define STACK_H
#include <stddef.h>
typedef struct Stack Stack;                    /* opaque: callers cannot see or depend on the layout */
Stack *stack_new(size_t initial_capacity);     /* returns NULL on failure */
int    stack_push(Stack *s, int value);        /* 0 on success, -1 on allocation failure */
int    stack_pop(Stack *s, int *out);          /* 0 on success, -1 if empty */
void   stack_free(Stack *s);                   /* accepts NULL */
#endif
```

```c
#include "stack.h"
#include <stdlib.h>
struct Stack { int *data; size_t len, cap; };
Stack *stack_new(size_t cap) {
    Stack *s = malloc(sizeof *s);
    if (!s) return NULL;
    s->data = cap ? malloc(cap * sizeof *s->data) : NULL;
    if (cap && !s->data) { free(s); return NULL; }
    s->len = 0; s->cap = cap;
    return s;
}
int stack_push(Stack *s, int v) {
    if (s->len == s->cap) {
        size_t ncap = s->cap ? s->cap * 2 : 4;
        int *p = realloc(s->data, ncap * sizeof *p);
        if (!p) return -1;
        s->data = p; s->cap = ncap;
    }
    s->data[s->len++] = v;
    return 0;
}
int stack_pop(Stack *s, int *out) { if (!s->len) return -1; *out = s->data[--s->len]; return 0; }
void stack_free(Stack *s) { if (s) { free(s->data); free(s); } }
```

```text
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined opaque_main.c stack.c -o opaque && ./opaque
popped: 25 16 9 4 1
```

Callers hold only `Stack *`; they cannot use `sizeof(Stack)`, embed one by value, access fields, or break invariants. The header documents contracts (return values, `NULL` handling); the implementation can change its fields without recompiling clients (**ABI stability**: crucial for shared libraries: the cost is a heap allocation per object, which can be avoided with a caller-supplied storage variant (`stack_init(Stack *, void *buf, size_t)` plus a `STACK_STORAGE_SIZE`). Conventions that make C libraries pleasant:

- `T *t_new(...)` / `void t_free(T *)` pairs (free accepts `NULL`), or `t_init`/`t_destroy` for caller-allocated storage.
- Return `0`/`-1` (or an error enum) and pass results through out-parameters; or return `NULL` for constructors.
- Accept `const T *` for read-only operations.
- Prefix all public symbols (`stack_`), keep helpers `static`.
- Write a header comment per function describing ownership, thread-safety, and errors.

## Intrusive containers and `container_of`

Textbook containers allocate a node that points at your data; **intrusive** containers put the links *inside your struct*. Benefits: no separate allocation, an object can be on several lists at once, O(1) removal given the object, and no pointer indirection. The kernel's `list_head` is the canonical example. Given a pointer to the embedded link, `container_of` subtracts the member's offset to recover the enclosing struct:

```c
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
```

```c
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* The Linux kernel's trick: embed the link in your struct; recover the struct from the link. */
struct list_head { struct list_head *next, *prev; };

#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#define LIST_HEAD_INIT(name) { &(name), &(name) }

static void list_add_tail(struct list_head *item, struct list_head *head) {
    item->prev = head->prev; item->next = head;
    head->prev->next = item; head->prev = item;
}
static void list_del(struct list_head *item) {
    item->prev->next = item->next; item->next->prev = item->prev;
    item->next = item->prev = item;
}
#define list_for_each(pos, head) for (pos = (head)->next; pos != (head); pos = pos->next)

struct task {
    int id;
    const char *name;
    struct list_head run_list;      /* the same task can also sit on other lists via other members */
    struct list_head wait_list;
};

int main(void) {
    struct list_head runnable = LIST_HEAD_INIT(runnable), waiting = LIST_HEAD_INIT(waiting);
    struct task t[4] = { {.id = 1, .name = "init"}, {.id = 2, .name = "sshd"}, {.id = 3, .name = "cron"}, {.id = 4, .name = "nginx"} };
    for (int i = 0; i < 4; i++) list_add_tail(&t[i].run_list, &runnable);
    list_add_tail(&t[2].wait_list, &waiting);          /* cron is on two lists at once, with no extra allocation */

    struct list_head *p;
    printf("runnable:"); list_for_each(p, &runnable) printf(" %s", container_of(p, struct task, run_list)->name);
    printf("\nwaiting: "); list_for_each(p, &waiting) printf(" %s", container_of(p, struct task, wait_list)->name);
    list_del(&t[1].run_list);                           /* O(1) removal given only the node */
    printf("\nafter list_del(sshd):"); list_for_each(p, &runnable) printf(" %s", container_of(p, struct task, run_list)->name);
    puts("");
    printf("offsetof(run_list)=%zu offsetof(wait_list)=%zu\n", offsetof(struct task, run_list), offsetof(struct task, wait_list));
    return 0;
}
```

```text
$ gcc -std=gnu11 -Wall -Wextra -fsanitize=address,undefined intrusive.c -o intrusive && ./intrusive
runnable: init sshd cron nginx
waiting:  cron
after list_del(sshd): init cron nginx
offsetof(run_list)=16 offsetof(wait_list)=32
```

`cron` is linked into both the `runnable` and `waiting` lists using two different embedded members, with no additional allocation; removing `sshd` required only its own node (`list_del(&t[1].run_list)`). The circular doubly linked list with a sentinel head removes all the `NULL` special cases for insertion and deletion. Cautions: the link must really be a member of the enclosing type (a wrong `type`/`member` pair compiles but corrupts memory; the kernel's version adds type checks), object lifetime must outlive its list membership (removing before `free`), and concurrent modification needs locks or RCU. The same technique underlies intrusive hash tables (`hlist`), red-black trees (`rb_node`), and wait queues; `offsetof` and the requirement that `ptr` truly points into a `type` object are what keep it well-defined.

## Generic programming without generics

| Technique | Pros | Cons |
|---|---|---|
| `void *` + element size (like `qsort`) | one implementation, simple | no type safety, indirect compare calls, extra copying via `memcpy` |
| Macros generating type-specific code (`DEFINE_VECTOR(int)`) | type-safe, fast, inlinable | code bloat, hard to debug |
| Header-only "templates" included multiple times with `#define T int` | same as above, more readable | namespace management |
| `_Generic` dispatch to typed functions | type-safe call sites | manual |
| Intrusive containers | zero allocation, flexible | requires modifying element types |
| Code generation (X-macros, scripts) | explicit | build complexity |

For performance-critical sorts, a specialized inline comparison (macro/template style) can beat `qsort` several-fold because the comparator can be inlined.

## State machines and dispatch tables

Function-pointer tables elegantly encode protocols and state machines:

```c
typedef enum { ST_IDLE, ST_CONNECTING, ST_READY, ST_CLOSED, ST_COUNT } state_t;
typedef state_t (*handler)(struct conn *, int event);
static const handler transitions[ST_COUNT] = { on_idle, on_connecting, on_ready, on_closed };
c->state = transitions[c->state](c, ev);
```

Validate indices before indexing (`state < ST_COUNT`), keep tables `const`, and generate parallel arrays with X-macros so they cannot diverge.

## Pitfalls checklist

1. Matching signatures at every call (compile with `-Wall -Wextra -Wcast-function-type`).
2. Don't call function pointers loaded from untrusted memory; keep tables `const`.
3. Comparator overflow and inconsistent ordering.
4. Callback lifetime: the context pointer must outlive the registration; document who frees it and whether the callback can run concurrently/reenter the library.
5. Don't `longjmp` out of a callback through library frames.
6. Opaque types hide `sizeof`: callers can't allocate them; provide constructors or size queries.

## Key takeaways

- Use a `typedef` for function pointer types; calling through a mismatched signature is UB.
- Callbacks need a context pointer for state; `qsort` comparators must be overflow-free and consistent.
- Vtables (a const struct of function pointers embedded by pointer in each object) implement polymorphism; the kernel's `file_operations` is the model.
- Opaque types (`typedef struct X X;` in the header) give encapsulation and ABI stability; `new/free` pairs and documented ownership make C APIs safe.
- Intrusive lists with `container_of` give allocation-free, multi-list membership and O(1) removal.
