---
title: "Practice: Struct Layout, Unions and Scope"
track: c
module: Data Modeling
moduleOrder: 3
order: 3
level: advanced
summary: "Four exercises on the data-modeling module: predicting struct padding and reordering members to shrink a struct, reading a float's bytes through a union and using bit-fields, and telling storage duration, scope and linkage apart with static, extern and const, with verified answers."
prerequisites:
  - "Structs, Alignment, Padding, Unions, Bit-fields and Endianness"
  - "Qualifiers, Storage Classes, Scope and Linkage"
sources:
  - label: "ISO/IEC 9899:2018 §6.2.2 (Linkages of identifiers), §6.2.4 (Storage durations), §6.7.2.1 (Structure and union specifiers), §6.7.3 (Type qualifiers)"
  - label: "System V AMD64 psABI §3.1.2 — Data Representation (alignment of aggregates)"
    url: https://gitlab.com/x86-psABIs/x86-64-ABI
  - label: "Raymond — The Lost Art of Structure Packing"
    url: http://www.catb.org/esr/structure-packing/
---

Layout and linkage rules are mechanical, so you can predict them exactly. Work out each answer on paper first, then run the programs (compile with `gcc -std=c17 -Wall -Wextra -g`).

## Exercises

**1. Predict the layout.** On x86-64 Linux, `struct wasteful { char tag; double value; char flag; int count; short id; };` has what size and which offsets? Reorder the *same* members to make it as small as possible and give the new size. Why is the total always a multiple of the largest member's alignment?

**2. Packed or aligned?** What size does a `__attribute__((packed))` struct of `uint8_t`, `uint16_t` and `uint32_t` have, and what do you pay for it?

**3. Unions and bit-fields.** Store `1.0f` in a union with a `uint32_t` and a `char[4]`. What bytes and integer do you see, and what does that say about this machine? How many bytes does a struct of three bit-fields of 1, 3 and 4 bits occupy?

**4. Scope, duration and linkage.** For each, say where the name is visible, how long the object lives and whether other files can see it: (a) `static int file_counter` at file level, (b) `int shared_total` at file level, (c) `static int id` inside a function, (d) an `int x` in an inner block that hides an outer `x`, (e) `const int *p` versus `int *const p`.

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1, 2 and 3.**

```c
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct wasteful { char tag; double value; char flag; int count; short id; };      /* declared in a careless order */
struct compact  { double value; int count; short id; char tag; char flag; };      /* same members, largest first */
struct packed_hdr { uint8_t version; uint16_t length; uint32_t id; } __attribute__((packed));

#define SHOW(type, member) printf("  %-8s offset %2zu\n", #member, offsetof(type, member))

int main(void) {
    printf("struct wasteful: size %zu, alignment %zu\n", sizeof(struct wasteful), _Alignof(struct wasteful));
    SHOW(struct wasteful, tag); SHOW(struct wasteful, value); SHOW(struct wasteful, flag); SHOW(struct wasteful, count); SHOW(struct wasteful, id);
    printf("struct compact:  size %zu, alignment %zu\n", sizeof(struct compact), _Alignof(struct compact));
    SHOW(struct compact, value); SHOW(struct compact, count); SHOW(struct compact, id); SHOW(struct compact, tag); SHOW(struct compact, flag);
    printf("packed header:   size %zu (no padding, but unaligned fields are slower and can fault on some CPUs)\n", sizeof(struct packed_hdr));

    union { uint32_t as_int; float as_float; unsigned char bytes[4]; } u = {.as_float = 1.0f};
    printf("float 1.0 viewed as bytes: %02x %02x %02x %02x -> integer 0x%08x\n", u.bytes[0], u.bytes[1], u.bytes[2], u.bytes[3], u.as_int);

    struct { unsigned ready : 1; unsigned mode : 3; unsigned level : 4; } bits = {1, 5, 9};
    printf("bit-fields ready=%u mode=%u level=%u occupy %zu byte(s)\n", bits.ready, bits.mode, bits.level, sizeof bits);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o p3_layout p3_layout.c && ./p3_layout; rm -f p3_layout
struct wasteful: size 32, alignment 8
  tag      offset  0
  value    offset  8
  flag     offset 16
  count    offset 20
  id       offset 24
struct compact:  size 16, alignment 8
  value    offset  0
  count    offset  8
  id       offset 12
  tag      offset 14
  flag     offset 15
packed header:   size 7 (no padding, but unaligned fields are slower and can fault on some CPUs)
float 1.0 viewed as bytes: 00 00 80 3f -> integer 0x3f800000
bit-fields ready=1 mode=5 level=9 occupy 4 byte(s)
```

*Layout.* Each member must start at an offset that is a multiple of its own alignment (`double` 8, `int` 4, `short` 2, `char` 1). In the careless order `tag` sits at 0, then 7 bytes of **padding** let `value` start at 8; `flag` at 16, 3 padding bytes, `count` at 20, `id` at 24, and the struct is rounded up to 32 so that in an *array* every element's `double` stays 8-aligned. That is why a struct's size is a multiple of its largest alignment. Sorting members **largest alignment first** (`double`, `int`, `short`, then the two `char`s) packs everything with no inner padding: 16 bytes, half the size. The same data, half the memory, and twice as many elements per cache line.

*Packed.* The packed header is 7 bytes (1 + 2 + 4) with no padding. You pay with **unaligned access**: slower on some CPUs, a hardware fault on others, and taking the address of a packed member (`&hdr.id`) produces a misaligned pointer. Use packing only to match an external format, and read fields with `memcpy`. Never rely on struct layout for a network protocol across compilers; serialise explicitly (see *Practice: Bits, I/O and Byte Order*).

*Unions.* Reading the member you did not last write reinterprets the **same bytes**. `1.0f` is `0x3f800000` in IEEE 754; the byte order `00 00 80 3f` shows the **least significant byte first**, so this machine is little-endian. (Reading another union member is allowed in C, unlike in C++, but the result is implementation-specific; `memcpy` is the portable form.) The three bit-fields of 1 + 3 + 4 bits fit in a single 4-byte `unsigned` storage unit: `sizeof` is 4, not 1, because the allocation unit is the declared type. The order of bit-fields within the unit and across byte boundaries is implementation-defined, so bit-fields are fine for memory savings and unsuitable for wire formats or hardware registers.

**4.**

```c
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
```

```text
$ gcc -std=c17 -Wall -Wextra -o p3_scope p3_scope.c && ./p3_scope; rm -f p3_scope
ids: 1 2 3
inner x = 12
outer x after the block = 1
*p = 3 (writing through a pointer-to-const is a compile error)
file_counter = 100, shared_total = 7
```

- (a) `static int file_counter`: **file scope, static duration, internal linkage**; visible only in this file, lives for the whole run.
- (b) `int shared_total`: file scope, static duration, **external linkage**; another file can use it after declaring `extern int shared_total;`. Define it in exactly one file.
- (c) `static int id` inside `next_id`: **block scope** (only that function can name it) but **static duration**, so it keeps its value between calls: the three calls print 1, 2 and 3. A normal local would restart each time.
- (d) The inner `x` **shadows** the outer one only between its braces: the inner block prints 12 while the function still returns the outer `1`. Shadowing compiles silently, so enable `-Wshadow` to find accidents.
- (e) `const int *p` is a pointer to a constant int (you may repoint `p`, but cannot write `*p`); `int *const p` is a constant pointer to a modifiable int (you may write `*p`, but cannot repoint `p`). Read declarations **right to left**: "p is a pointer to const int" versus "p is a const pointer to int".

</details>

## Key takeaways

1. Members align to their own size; padding appears between them and at the end, and ordering members largest-first removes most of it.
2. Packed structs save bytes but cost aligned access; use them only for external formats and access fields with `memcpy`.
3. Union reads reinterpret bytes (and reveal endianness); bit-fields have implementation-defined layout, so never use them for wire formats.
4. Duration (how long), scope (where visible) and linkage (visible to other files) are three separate properties; `static` changes duration for locals and linkage for globals.
