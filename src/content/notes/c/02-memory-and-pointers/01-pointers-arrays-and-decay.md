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

Unions can also be used for type punning in C (but not C++), per the footnote in §6.5.2.3. Build with `-fno-strict-aliasing` only as a last resort; the Linux kernel does so deliberately, which is one reason kernel C is not "standard C".

### `restrict`

`restrict` is a **promise by the programmer** that, during the lifetime of the pointer, the object it accesses is accessed only through that pointer (or pointers derived from it). It lets the compiler vectorize and reorder loads and stores. Breaking the promise is UB; `memcpy` is declared with `restrict` while `memmove` is not, and that is the whole difference.

## Common bugs, and how to catch them

| Bug | Example | Tool |
|---|---|---|
| Dangling pointer | returning `&local` | `-Wreturn-local-addr`, ASan (`-fsanitize=address`) |
| Off-by-one past the end | `for (i = 0; i <= n; ++i) a[i]` | ASan, UBSan |
| `sizeof` on decayed array | `sizeof arr` in a function parameter | `-Wsizeof-array-argument` |
| Misaligned access | `*(int *)(buf + 1)` | `-fsanitize=alignment` |
| Strict aliasing | `*(int *)&f` | `-Wstrict-aliasing=2`, `-fsanitize=undefined` (partial) |
| Use after free | read after `free(p)` | ASan, Valgrind |

A good default for development builds: `-std=c11 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g`.

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
- The sanitizers check actual accesses, so they only find what the test inputs exercise; combine with a fuzzer or broad test inputs (see the testing note).

## Key takeaways

- A pointer carries a type and (conceptually) provenance; it is not just an address.
- Arrays decay to a pointer to the first element, except under `sizeof`, `&` and string-literal initializers.
- Array parameters are really pointers.
- Pointer arithmetic is scaled and valid only within one array plus one-past-the-end; the same bounds apply to subtraction and ordering.
- Use `memcpy` for unaligned or type-punned reads; know `restrict` and strict aliasing before touching optimized code.
