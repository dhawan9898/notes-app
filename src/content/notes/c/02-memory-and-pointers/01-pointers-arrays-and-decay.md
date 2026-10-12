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

## Key takeaways

- A pointer carries a type and (conceptually) provenance; it is not just an address.
- Arrays decay to a pointer to the first element, except under `sizeof`, `&` and string-literal initializers.
- Array parameters are really pointers.
- Pointer arithmetic is scaled and valid only within one array plus one-past-the-end; the same bounds apply to subtraction and ordering.
- Use `memcpy` for unaligned or type-punned reads; know `restrict` and strict aliasing before touching optimized code.
