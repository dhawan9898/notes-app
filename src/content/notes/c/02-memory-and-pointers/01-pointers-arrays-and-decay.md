---
title: Pointers, Arrays and Decay
track: c
module: Memory and Pointers
moduleOrder: 2
order: 1
level: intermediate
summary: What a pointer actually is in the C abstract machine, how arrays decay, what pointer arithmetic is allowed, and where undefined behavior hides.
prerequisites:
  - Pointers for Beginners
sources:
  - label: "ISO/IEC 9899:2011 (C11) §6.2.5 Types, §6.3.2.1 Lvalues/arrays/function designators, §6.5.2.1 Array subscripting, §6.5.6 Additive operators, §6.5.8 Relational operators"
  - label: "ISO/IEC 9899:2011 §6.5p7 (effective type and aliasing), §6.7.3.1 (restrict)"
  - label: "ISO/IEC 9899:2011 §6.3.2.3 (pointers, null pointer constants), §6.5.16.1 (simple assignment constraints and EXAMPLE 3), §6.2.4 (storage durations and lifetime), §7.22.3.5 (realloc)"
  - label: "CVE-2009-1897 — Linux tun driver: a null check removed by the compiler after a dereference"
    url: https://nvd.nist.gov/vuln/detail/CVE-2009-1897
  - label: "C FAQ (Steve Summit) — Q11.10: Why can't I pass a char ** to a function which expects a const char **?"
    url: https://c-faq.com/ansi/constmismatch.html
  - label: "WG14 N2676 — A Provenance-aware Memory Object Model for C (TS 6010 draft)"
    url: https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2676.pdf
---

## A pointer is not an integer

On common hardware a pointer is a machine address, and it is tempting to think of it as an integer. The C standard defines it more narrowly: a pointer is a value that **refers to an object (or function)** of a given type, or is null. That abstraction is what lets compilers optimize, and what turns "it worked on my machine" bugs into undefined behavior (UB).

Consequences you can rely on:

- A pointer has a **type**, and the type determines the size of one step (`p + 1`) and what `*p` means.
- A pointer is valid only while the object it points to is alive (storage duration), and only within that object's bounds, plus the single **one-past-the-end** address.
- Converting between pointer and integer is **implementation-defined**; `uintptr_t` (optional in the standard, universal in practice) round-trips a `void *`.
- Modern compilers track **provenance**: which object a pointer was derived from. Two pointers can have equal addresses yet not be interchangeable, e.g. a one-past-the-end pointer of `a` and the address of an adjacent `b`.

## Arrays are not pointers, but they decay

`int a[4]` is an **object of type `int[4]`**: contiguous storage for 4 ints. `sizeof a == 16` (typically). In most expressions the array *decays* into a pointer to its first element (§6.3.2.1p3). The only exceptions are:

1. operand of `sizeof`
2. operand of unary `&`
3. a string literal used to initialize a `char` array

```c
int a[4] = {1, 2, 3, 4};
int *p   = a;          // decay: &a[0], type int *
int (*q)[4] = &a;      // no decay: pointer to the whole array, type int (*)[4]

sizeof a;              // 16: whole array
sizeof p;              // 8 on LP64: just a pointer
sizeof *q;             // 16
```

`p` and `q` have the **same address** but different types, so `p + 1` advances 4 bytes while `q + 1` advances 16. This is the origin of 2D-array arithmetic.

### Function parameters never receive arrays

```c
void f(int a[10]);      // adjusted by the compiler to: void f(int *a);

void g(int a[10]) {
    sizeof a;           // sizeof(int *), NOT 40. GCC warns: -Wsizeof-array-argument
}
```

C cannot pass an array by value. To preserve the dimension, pass a pointer to the array or use the C99 `static` hint:

```c
void h(int (*a)[10]);       // caller passes &arr; sizeof *a is 40
void k(size_t n, int a[static n]);   // caller promises at least n valid elements
```

## Subscripting is pointer arithmetic

By definition (§6.5.2.1), `E1[E2]` is `(*((E1)+(E2)))`. Hence `a[i] == *(a + i) == *(i + a) == i[a]`. The last form is legal and useless.

Addition `p + n` is **scaled**: the result points `n * sizeof *p` bytes further. The result must stay inside the array or equal one-past-the-end; otherwise the *computation itself* is UB, even without a dereference.

```c
int a[4];
int *end = a + 4;   // OK: one past the end (may be compared, never dereferenced)
int *bad = a + 5;   // UB: already out of range
```

Rules for the other operations:

| Operation | Defined when |
|---|---|
| `p - q` (gives `ptrdiff_t`) | both point into the same array object (or one past it) |
| `p < q`, `p <= q`, ... | same: same array/struct object. Comparing unrelated pointers is UB |
| `p == q` | any two pointers of compatible type; the result is well-defined |
| `*p` | `p` points to a live object of the right type, properly aligned |

The one-past-the-end rule exists so that the idiom `for (p = a; p != a + n; ++p)` works for empty and full arrays alike, and the reason a *pointer to before the first element* (`a - 1`) is **not** allowed, so reverse loops must be written carefully:

```c
for (size_t i = n; i-- > 0; )   // visits n-1 ... 0 without forming a - 1
    use(a[i]);
```

## Declarations: read them inside-out

```c
int *a[3];       // array of 3 pointers to int
int (*b)[3];     // pointer to array of 3 ints
int *f(void);    // function returning int *
int (*g)(void);  // pointer to function returning int
int (*h[4])(int);// array of 4 pointers to function(int) returning int
```

Start at the identifier and alternate right (`[]`, `()`) then left (`*`), with `()` forcing priority. `const` binds to what is on its **left** (or right if nothing is on the left):

```c
const int *p;        // pointer to const int: *p is read-only, p can move
int *const p = &x;   // const pointer to int: p fixed, *p writable
const int *const p;  // both fixed
```

### `const` at two levels: why `char **` does not convert to `const char **`

Adding `const` to the pointee is always allowed (`char *` → `const char *`), so it is natural to expect `char **` → `const char **` to work too, for example to pass `argv` to a function that promises not to modify the strings. C forbids it, because that conversion would open a hole in the type system. §6.5.16.1 allows qualifiers to be added only to the type the pointer *directly* points to, and its EXAMPLE 3 shows the hole:

```c
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
```

```text
$ cc -std=c11 -Wall -Wextra -c constpp.c -o /dev/null 2>&1 | grep -E "warning|error"
constpp.c:16:15: warning: passing 'char *[2]' to parameter of type 'const char **' discards qualifiers in nested pointer types [-Wincompatible-pointer-types-discards-qualifiers]
constpp.c:21:18: warning: initializing 'const char **' with an expression of type 'char **' discards qualifiers in nested pointer types [-Wincompatible-pointer-types-discards-qualifiers]
2 warnings generated.
```

Through `cpp` the program stores the address of a `const char` into a plain `char *`, then writes through it: a const object modified with no cast anywhere. If `c` lives in read-only memory the write crashes; otherwise the optimizer may keep using the old value. The two diagnostics are the compiler refusing exactly this pattern. clang warns by default (the output above was captured with clang 21 on 64-bit ARM Linux, as were the other labs added in this revision; the remaining labs used GCC 14.2 on x86-64). GCC 14 and later treat incompatible pointer types as an error by default, and C++ rejects the conversion outright.

`const char *const *` is safe because nothing can be stored through it at any level, and C++ accepts that conversion implicitly. C does not: it still needs the explicit cast shown on line 17. A function that only reads an array of strings should take `const char *const *`, and C callers pass `(const char *const *)argv`.

## Pointers to pointers

A function receives copies of its arguments. To let it change the caller's *pointer* (not just the object the pointer refers to), pass the address of that pointer: a `T **`. Three places this shows up constantly:

- **Out-parameters for allocated results.** `int make_buffer(size_t n, char **out)` returns a status and stores the new buffer through `out`; `getline(char **lineptr, size_t *n, FILE *)`, `asprintf` and `strtol(…, char **endptr, …)` use the same shape.
- **Editing a linked structure without special cases.** A `struct node **pp` that points at "whichever pointer leads to the current node" (first the head variable, later some node's `next`) lets one loop insert or remove at the head, middle and tail alike.
- **`argv`**, which is `char **`: a pointer to the first element of an array of `argc + 1` pointers whose last element is a null pointer (§5.1.2.2.1), so it can be walked either by count or until `NULL`.

```c
// Pointers to pointers: out-parameters, a list edited through `struct node **`, and the shape of argv.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Wrong: `out` is a copy of the caller's pointer, so the caller never sees the new buffer (and it leaks).
static int make_buffer_wrong(size_t n, char *out) {
    out = malloc(n);
    if (!out) return -1;
    memset(out, 'x', n);
    return 0;
}

// Right: receive the address of the caller's pointer and store through it.
static int make_buffer(size_t n, char **out) {
    char *p = malloc(n);
    if (!p) return -1;
    memset(p, 'x', n - 1);
    p[n - 1] = '\0';
    *out = p;                          // the only write the caller observes
    return 0;
}

struct node { int value; struct node *next; };

static void push(struct node **head, int v) {
    struct node *n = malloc(sizeof *n);
    n->value = v;
    n->next = *head;
    *head = n;
}

// `pp` points at whichever pointer currently leads to the node: the head variable or a previous node's `next`.
// The head is therefore not a special case.
static int remove_value(struct node **pp, int v) {
    for (; *pp; pp = &(*pp)->next) {
        if ((*pp)->value == v) {
            struct node *dead = *pp;
            *pp = dead->next;
            free(dead);
            return 1;
        }
    }
    return 0;
}

static void print_list(const char *label, const struct node *n) {
    printf("%-22s", label);
    for (; n; n = n->next) printf(" %d", n->value);
    printf("\n");
}

int main(int argc, char **argv) {
    char *buf = NULL;
    make_buffer_wrong(8, buf);
    printf("after make_buffer_wrong: buf is %s\n", buf ? "set" : "still NULL (the callee changed its own copy)");
    if (make_buffer(8, &buf) == 0) printf("after make_buffer:       buf = \"%s\"\n", buf);
    free(buf);

    struct node *head = NULL;
    for (int v = 1; v <= 5; v++) push(&head, v);
    print_list("list:", head);
    remove_value(&head, 5);            // the head node
    print_list("removed 5 (the head):", head);
    remove_value(&head, 3);            // a middle node
    print_list("removed 3 (middle):", head);
    remove_value(&head, 1);            // the tail
    print_list("removed 1 (tail):", head);
    while (head) remove_value(&head, head->value);

    // argv is a pointer to the first element of an array of argc + 1 pointers; the last one is NULL (C11 5.1.2.2.1).
    printf("argc = %d, argv[argc] is %s\n", argc, argv[argc] == NULL ? "NULL" : "not NULL");
    for (char **p = argv + 1; *p; p++) printf("  arg %td: \"%s\" (%zu chars)\n", p - argv, *p, strlen(*p));
    return 0;
}
```

```text
$ cc -O2 -Wall -Wextra outparam.c -o outparam && ./outparam red green
after make_buffer_wrong: buf is still NULL (the callee changed its own copy)
after make_buffer:       buf = "xxxxxxx"
list:                  5 4 3 2 1
removed 5 (the head):  4 3 2 1
removed 3 (middle):    4 2 1
removed 1 (tail):      4 2
argc = 3, argv[argc] is NULL
  arg 1: "red" (3 chars)
  arg 2: "green" (5 chars)
```

- `make_buffer_wrong` assigns to its own parameter. The caller's `buf` is untouched (still `NULL`) and the allocation leaks; `-Wall` does not catch this, but LeakSanitizer and a review question ("who sees this assignment?") do.
- `remove_value` handles the head, a middle node and the tail with the same three lines, because it rewrites `*pp`, the pointer that led to the node, instead of tracking a separate `prev`.
- `argv[argc]` is guaranteed to be `NULL`, which is why `for (char **p = argv + 1; *p; p++)` is a correct loop.

## Null pointers

An integer constant expression with value 0, or such an expression cast to `void *`, is a **null pointer constant** (§6.3.2.3); converting it to a pointer type gives a **null pointer**, which compares unequal to a pointer to any object or function. `NULL` is a macro for a null pointer constant (`((void *)0)` in the `<stddef.h>` that GCC and clang ship for C). Three facts are easy to get wrong:

- **The representation is not promised to be all-bits-zero.** It is on every mainstream platform, but portable code writes `p = NULL` instead of relying on `memset` or `calloc` to produce null pointers inside structs.
- **Dereferencing a null pointer is undefined behavior, not "a guaranteed crash".** On Linux the first page is unmapped (`vm.mmap_min_addr`), so the typical result is `SIGSEGV`. But `p->field` with a large field offset, or `p[i]` with a large `i`, can land in mapped memory.
- **The optimizer uses the dereference as proof.** After `int v = p->x;` the compiler may assume `p != NULL` and delete a later `if (!p) return`. This is CVE-2009-1897: the Linux tun driver read `tun->sk` before checking `tun`, GCC removed the check, and a mapped page zero turned the bug into a privilege escalation. The kernel has built with `-fno-delete-null-pointer-checks` ever since. Check before you dereference, never after.

## Void pointers, alignment and casts

`void *` converts to and from any object-pointer type without a cast in C (unlike C++), and cannot be dereferenced or used in arithmetic. GCC allows `void *` arithmetic as an extension (treating the size as 1); don't rely on it.

Every type has an **alignment requirement** (`_Alignof`). Casting a `char *` that points into a buffer to `int *` and dereferencing it is UB if the address is not suitably aligned, and is also an aliasing violation. On x86 it may appear to work; on ARM, MIPS or with SIMD loads it can fault or silently misbehave. To read a field out of a byte buffer (a very common task when parsing packets), use `memcpy`:

```c
uint32_t read_u32(const unsigned char *buf) {
    uint32_t v;
    memcpy(&v, buf, sizeof v);   // compiles to a single load where legal
    return v;
}
```

## Strict aliasing

§6.5p7 allows an object to be accessed only through an lvalue of: a compatible type, a qualified/signed-unsigned variant, an aggregate containing such a member, or a **character type**. Accessing a `float` through an `int *` is UB, and optimizers assume it never happens.

```c
float f = 1.0f;
uint32_t bits = *(uint32_t *)&f;   // UB: violates strict aliasing
uint32_t ok; memcpy(&ok, &f, 4);   // defined
```

The rule is not academic. Here the same 4 bytes are reached through an `int *` and a `float *` (union members passed to separate pointer parameters are still two lvalues of different types, so this is undefined behavior). At `-O0` the function reloads `*ip` after the `float` store and returns the float's bit pattern; at `-O2` the compiler concludes the `float` store cannot have changed an `int` and returns the constant it stored a line earlier:

```c
// Strict aliasing in action: the optimizer assumes an int * and a float * never refer to the same object.
#include <stdio.h>
#include <string.h>

__attribute__((noinline)) int store_and_read(int *ip, float *fp) {
    *ip = 1;
    *fp = 2.0f;          // may this store change *ip? Under C11 6.5p7 it may not, so ...
    return *ip;          // ... the compiler is allowed to return the constant 1 without reloading
}

// Defined alternative: reinterpret bytes with memcpy (compilers turn it into a register move).
static unsigned bits_of(float f) {
    unsigned u;
    memcpy(&u, &f, sizeof u);
    return u;
}

int main(void) {
    union { int i; float f; } u;
    int r = store_and_read(&u.i, &u.f);    // both pointers refer to the same 4 bytes: undefined behavior
    printf("store_and_read returned %d (0x%08x)\n", r, (unsigned)r);
    printf("memory holds            0x%08x (bit pattern of 2.0f)\n", bits_of(u.f));
    return 0;
}
```

```text
$ for o in -O0 -O2 "-O2 -fno-strict-aliasing"; do echo "[$o]"; cc $o -Wall alias.c -o alias && ./alias; done
[-O0]
store_and_read returned 1073741824 (0x40000000)
memory holds            0x40000000 (bit pattern of 2.0f)
[-O2]
store_and_read returned 1 (0x00000001)
memory holds            0x40000000 (bit pattern of 2.0f)
[-O2 -fno-strict-aliasing]
store_and_read returned 1073741824 (0x40000000)
memory holds            0x40000000 (bit pattern of 2.0f)
```

Neither answer is "wrong" from the compiler's point of view: the program has no defined meaning, so the result depends on the optimization level. That is the signature of an aliasing bug: tests pass in debug builds and fail in release builds. GCC enables `-fstrict-aliasing` at `-O2` and above in the same way. `-fno-strict-aliasing` restores the "memory is just bytes" reading at some cost in optimization; the `memcpy` in `bits_of` is the defined way to reinterpret bytes, and optimizing compilers turn it into a plain register move.

Unions can also be used for type punning in C (but not C++), per the footnote in §6.5.2.3. Build with `-fno-strict-aliasing` only as a last resort; the Linux kernel does so deliberately, which is one reason kernel C is not "standard C".

### `restrict`

`restrict` is a **promise by the programmer** that, during the lifetime of the pointer, the object it accesses is accessed only through that pointer (or pointers derived from it). It lets the compiler vectorize and reorder loads and stores. Breaking the promise is UB; `memcpy` is declared with `restrict` while `memmove` is not, and that is the whole difference.

## Lifetime: when a valid pointer stops being valid

A pointer is valid only while the object it points to is alive (§6.2.4). Afterwards its value is *indeterminate*: even comparing or printing it is undefined, not just dereferencing it. The cases that cause real bugs:

| Situation | When the pointer dies |
|---|---|
| Address of a local variable | when the function returns (or the block ends) |
| Pointer from `malloc` | at `free`, including the free that `realloc` does internally when it moves the block |
| Pointer into a growable array (vector, string builder, hash table) | at the next operation that may reallocate |
| `getenv`, `strerror`, `localtime`, `readdir` results | at the next call to the same function (or as documented) |
| Pointer into a `FILE` buffer or `mmap` region | at `fclose` / `munmap` |

The third row is the one that surprises people, because the code that saved the pointer and the code that grew the array are often far apart. The program keeps both a pointer and an index to the first element of a vector, then appends 99 more elements:

```c
// A pointer into a growable array goes stale when realloc moves the block.
#include <stdio.h>
#include <stdlib.h>

struct vec { int *data; size_t len, cap; };

static int push(struct vec *v, int x) {
    if (v->len == v->cap) {
        size_t cap = v->cap ? 2 * v->cap : 4;
        int *d = realloc(v->data, cap * sizeof *d);   // may return a different address; the old block is freed
        if (!d) return -1;
        v->data = d;
        v->cap = cap;
    }
    v->data[v->len++] = x;
    return 0;
}

int main(int argc, char **argv) {
    struct vec v = {0};
    push(&v, 10);
    int *first = &v.data[0];      // pointer to an element: valid only until the next reallocation
    size_t index = 0;             // an index stays valid across reallocations
    for (int i = 1; i < 100; i++) push(&v, 10 + i);
    printf("by index:   v.data[%zu] = %d\n", index, v.data[index]);
    if (argc > 1) printf("by pointer: *first = %d\n", *first);   // use after free if the block moved
    free(v.data);
    return 0;
}
```

```text
$ cc -g -O0 -fsanitize=address stale.c -o stale && ./stale && ./stale x 2>&1 | grep -E "ERROR|READ of|freed by|previously allocated|in (main|push) " | sed -E "s/^==[0-9]+==//; s/ on address.*//; s/ *#[0-9]+ 0x[0-9a-f]+ in /    in /; s/ at 0x[0-9a-f]+//" | sed "s#$PWD/##"
by index:   v.data[0] = 10
ERROR: AddressSanitizer: heap-use-after-free
READ of size 4 thread T0
    in main stale.c:26:55
freed by thread T0 here:
    in push stale.c:10:18
    in main stale.c:24:35
previously allocated by thread T0 here:
    in push stale.c:10:18
    in main stale.c:21:5
```

The index survives because it is relative to whatever `data` currently is; the pointer refers to the old block, which `realloc` freed when it moved the data (the "freed by" stack points at the `realloc` in `push`). Without AddressSanitizer this program usually prints the right number, because the freed memory still holds the old value. That is why such bugs survive testing. The rules: store indices or handles, not element pointers, in anything that grows; re-derive pointers after any call that may reallocate; and document in an API which operations invalidate which pointers (C++ calls this *iterator invalidation*; C libraries must spell it out).

## Common bugs, and how to catch them

| Bug | Example | Tool |
|---|---|---|
| Dangling pointer | returning `&local` | `-Wreturn-local-addr`, ASan (`-fsanitize=address`) |
| Off-by-one past the end | `for (i = 0; i <= n; ++i) a[i]` | ASan, UBSan |
| `sizeof` on decayed array | `sizeof arr` in a function parameter | `-Wsizeof-array-argument` |
| Misaligned access | `*(int *)(buf + 1)` | `-fsanitize=alignment` |
| Strict aliasing | `*(int *)&f` | `-Wstrict-aliasing=2`, `-fsanitize=undefined` (partial) |
| Use after free | read after `free(p)` | ASan, Valgrind |
| Stale pointer after growth | element pointer kept across `realloc` | ASan (heap-use-after-free, "freed by … realloc") |
| Null check after dereference | `v = p->x; if (!p) …` | `-Wnull-dereference` (GCC, partial), static analysis (`-fanalyzer`, clang-tidy) |
| Lost out-parameter | assigning to a `T *` parameter instead of through a `T **` | LeakSanitizer, code review |
| Qualifier hole | `const char **cpp = &p` | the default warning; treat it as an error (`-Werror=incompatible-pointer-types`) |

A good default for development builds: `-std=c11 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g`.

### Troubleshooting a pointer bug

1. **Reproduce under the sanitizers first** (`-g -O0 -fsanitize=address,undefined`). ASan names the kind of access (heap-buffer-overflow, stack-use-after-return, heap-use-after-free) and gives three stacks: the bad access, where the memory was freed, and where it was allocated. That usually identifies the bug class without a debugger.
2. **If the bug only appears at `-O2`**, suspect undefined behavior the optimizer exploited: strict aliasing (try `-fno-strict-aliasing`; if the symptom disappears, find the punning), a null check after a dereference, signed overflow, or an out-of-range pointer computation. Build with `-fsanitize=undefined` at `-O2` as well.
3. **Ask "whose copy is this?"** for every assignment to a pointer parameter, and "who owns this, and until when?" for every stored pointer. Most lost updates and leaks are answered by the first question, most use-after-frees by the second.
4. **Print the types, not just the values.** A `sizeof` that is 8 where you expected 40 means the array decayed; `p + 1` moving 16 bytes instead of 4 means `p` is a pointer to an array.
5. **Watch the memory.** In `gdb`, `watch -l *ptr` stops at the instruction that changes a location, which finds stray writes through the wrong pointer.

## Lab: two-dimensional arrays and pointer-to-array types

A two-dimensional array is an array of arrays: `int m[3][4]` is three objects of type `int[4]`, laid out one after another (**row-major**). Pointer arithmetic follows the pointed-to type, so what `m` decays to (`int (*)[4]`, a pointer to an array of four ints) advances by a whole row, while `&m[0][0]` (a plain `int *`) advances by one element:

```c
// Pointer-to-array types, 2-D arrays, and how the same memory is seen through different pointer types.
#include <stdio.h>

int main(void) {
    int m[3][4];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 4; j++) m[i][j] = i * 10 + j;

    printf("sizeof(m) = %zu, sizeof(m[0]) = %zu, sizeof(m[0][0]) = %zu\n", sizeof m, sizeof m[0], sizeof m[0][0]);

    int (*row)[4] = m;                         // pointer to an array of 4 ints: what m decays to
    int *flat = &m[0][0];                      // pointer to the first int
    printf("row + 1 advances by %td bytes; flat + 1 advances by %td bytes\n", (char *)(row + 1) - (char *)row, (char *)(flat + 1) - (char *)flat);
    printf("(*(row + 2))[3] = %d   m[2][3] = %d   *(*(m + 2) + 3) = %d\n", (*(row + 2))[3], m[2][3], *(*(m + 2) + 3));

    // Walking all 12 ints through the flat pointer is how row-major layout is usually exploited. It is accepted practice but
    // formally goes outside the first row's bounds; strictly conforming code indexes m[i][j].
    printf("row-major order in memory: ");
    for (int k = 0; k < 12; k++) printf("%d ", flat[k]);
    printf("\n");

    // A function taking a 2-D array needs the inner dimension in the type; C99 lets the dimension be a parameter.
    int total = 0;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 4; j++) total += (*(m + i))[j];
    printf("sum via pointer-to-array arithmetic: %d\n", total);

    int *rows[3] = { m[0], m[1], m[2] };       // an array of pointers is a different type: no contiguous layout is implied
    printf("sizeof(rows) = %zu (three pointers), rows[1][2] = %d\n", sizeof rows, rows[1][2]);
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra arrays2d.c -o arrays2d && ./arrays2d
sizeof(m) = 48, sizeof(m[0]) = 16, sizeof(m[0][0]) = 4
row + 1 advances by 16 bytes; flat + 1 advances by 4 bytes
(*(row + 2))[3] = 23   m[2][3] = 23   *(*(m + 2) + 3) = 23
row-major order in memory: 0 1 2 3 10 11 12 13 20 21 22 23 
sum via pointer-to-array arithmetic: 138
sizeof(rows) = 24 (three pointers), rows[1][2] = 12
```

- The size of the whole array is 48 bytes, of a row 16, of an element 4: `sizeof` sees the real array type, before decay.
- `row + 1` moves 16 bytes and `flat + 1` moves 4: **the type of a pointer determines the scale of arithmetic**.
- `m[2][3]`, `(*(row + 2))[3]` and `*(*(m + 2) + 3)` are the same expression written three ways, because `a[i]` is defined as `*(a + i)`.
- Walking all twelve ints through `flat[k]` shows the contiguous row-major layout. This is widespread practice, but the standard formally treats indexing past the first row through a pointer derived from `m[0]` as out of bounds; code that must be strictly conforming indexes `m[i][j]`.
- `int *rows[3]` (an *array of pointers*) is a different type with a different layout: 24 bytes of pointers, and each row may live anywhere. A function parameter `int m[][4]` or `int (*m)[4]` accepts the first kind; `int **m` accepts only the second. Passing a true 2-D array where `int **` is expected compiles with a warning at best and crashes at run time.

## Lab: reading declarators

C declarations are read by starting at the identifier and working outward: look right (array `[]` and function `()` suffixes bind tighter than `*`), then left, and parentheses override. The program defines one of each hard case and prints `sizeof` and call results that confirm the reading:

```c
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
```

```text
$ gcc -O2 -Wall -Wextra decl.c -o decl && ./decl
sizeof a = 32 (4 pointers), sizeof b = 8 (one pointer), sizeof *b = 16 (an array of 4 ints)
c[1](6, 7) = 42, d(0)(6, 7) = 13, d(1)(6, 7) = 42
sizeof e = sizeof f = sizeof g = 8: const changes what the compiler lets you do, not the representation
```

| Declaration | Reading |
|---|---|
| `int *a[4]` | `a` is an array of 4 pointers to `int` (the `[]` binds first) |
| `int (*b)[4]` | `b` is a pointer to an array of 4 `int`: the parentheses force `*` first |
| `int (*c[3])(int, int)` | `c` is an array of 3 pointers to functions taking two ints and returning an int |
| `int (*(*d)(int))(int, int)` | `d` is a pointer to a function taking an int and returning a pointer to a function taking two ints and returning an int (what `pick` is) |
| `const int *e` | pointer to a constant int: the *pointee* cannot be changed through `e` |
| `int *const f` | constant pointer to int: `f` cannot be re-pointed |
| `const int *const g` | both |

The sizes show that `const` is a compile-time promise: the three pointers have identical representation. The tool `cdecl` (and `typedef` for function-pointer types, e.g. `typedef int (*binop)(int, int);`) makes complex declarations readable; use the typedef whenever a declarator needs more than one level.

## Lab: the edges of pointer arithmetic

Forming a pointer to **one past the last element** is valid and common (`for (p = a; p != a + n; p++)`), but dereferencing it, or forming a pointer more than one past the end, is undefined behavior, even if the pointer is never used. Pointer comparison and subtraction are defined only for pointers into the same array object. The program below does the legal thing, then four illegal things selected by a command-line argument and compiled with the sanitizers:

```c
// Pointer arithmetic limits: valid one-past-the-end pointers, and what the sanitizers say about the invalid cases.
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int a[4] = {1, 2, 3, 4}, b[4] = {5, 6, 7, 8};
    int *end = a + 4;                          // one past the end: valid to form and compare, invalid to dereference
    int sum = 0;
    for (int *p = a; p != end; p++) sum += *p;
    printf("loop to the one-past-the-end pointer: sum = %d\n", sum);
    printf("pointer difference (end - a) = %td elements, in units of the pointed-to type, not bytes\n", end - a);

    if (argc > 1) {
        int which = atoi(argv[1]);
        volatile int idx = 4;                  // volatile: keep the compiler from seeing the constant
        if (which == 1) printf("a[4] = %d\n", a[idx]);                                  // read one past the end of a: undefined behavior
        if (which == 2) { int *q = a + 100; printf("formed a + 100 (UB already), b is %s\n", q == b ? "there" : "not there"); }
        if (which == 3) { int *h = malloc(4 * sizeof *h); free(h); printf("use after free: %d\n", h[0]); }
        if (which == 4) { int *h = malloc(4 * sizeof *h); h[4] = 1; free(h); }          // heap overflow by one element
    }
    return 0;
}
```

```text
$ gcc -g -O0 -fsanitize=address,undefined provenance.c -o prov && ./prov && for n in 1 3 4; do echo "--- case $n"; ./prov $n 2>&1 | grep -E "ERROR|SUMMARY|runtime error" | sed "s#$PWD/##" | head -3; done
loop to the one-past-the-end pointer: sum = 10
pointer difference (end - a) = 4 elements, in units of the pointed-to type, not bytes
--- case 1
provenance.c:16:48: runtime error: index 4 out of bounds for type 'int [4]'
provenance.c:16:25: runtime error: load of address 0x7f850f400050 with insufficient space for an object of type 'int'
==3184421==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7f850f400050 at pc 0x561804fd47f3 bp 0x7ffcf379b360 sp 0x7ffcf379b358
--- case 3
==3184425==ERROR: AddressSanitizer: heap-use-after-free on address 0x502000000010 at pc 0x558dd391491c bp 0x7fffb430a170 sp 0x7fffb430a168
SUMMARY: AddressSanitizer: heap-use-after-free provenance.c:18 in main
--- case 4
==3184429==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x502000000020 at pc 0x563ae21c09e9 bp 0x7ffe65210750 sp 0x7ffe65210748
SUMMARY: AddressSanitizer: heap-buffer-overflow provenance.c:19 in main
```

- The legal loop ends at the one-past-the-end pointer and the difference `end - a` is 4 **elements**, because pointer subtraction is scaled by the element size.
- Case 1 (reading `a[4]`) is caught by UBSan as an out-of-bounds index; case 3 (use after free) and case 4 (one element past a heap block) are caught by AddressSanitizer with an exact classification and the source line. Case 2 (forming `a + 100`) is not shown because it is silent in practice, but it is still undefined, and the optimizer may use that fact (for example, to assume a loop bound).
- The sanitizers check actual accesses, so they only find what the test inputs exercise; combine with a fuzzer or broad test inputs (see *Static Analysis, Fuzzing and Testing C Code*).

## Key takeaways

- A pointer carries a type and (conceptually) provenance; it is not just an address.
- Arrays decay to a pointer to the first element, except under `sizeof`, `&` and string-literal initializers.
- Array parameters are really pointers.
- Pointer arithmetic is scaled and valid only within one array plus one-past-the-end; the same bounds apply to subtraction and ordering.
- Use `memcpy` for unaligned or type-punned reads; know `restrict` and strict aliasing before touching optimized code. Aliasing bugs are the ones that pass at `-O0` and fail at `-O2`.
- Pass a `T **` when the callee must change the caller's pointer; a `struct node **` removes the head-of-list special case.
- `char **` does not convert to `const char **` (that would let you modify a const object); C needs a cast even for the safe `const char *const *`.
- Null is checked *before* the dereference; after it, the compiler may delete the check.
- A pointer dies with its object, and `realloc` kills every pointer into the old block: keep indices into anything that grows.
