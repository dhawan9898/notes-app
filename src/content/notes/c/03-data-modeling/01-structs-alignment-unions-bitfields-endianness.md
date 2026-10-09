---
title: Structs, Alignment, Padding, Unions, Bit-fields and Endianness
track: c
module: Data Modeling
moduleOrder: 3
order: 1
level: intermediate
summary: How the compiler lays out aggregate types, why member order changes size, safe type punning, tagged unions, bit-field pitfalls, and portable serialization — with measured layouts and tested code.
sources:
  - label: "ISO/IEC 9899:2018 (C17) §6.7.2.1 Structure and union specifiers; §6.2.8 Alignment of objects; §6.5.2.3"
  - label: "System V AMD64 psABI §3.1.2 Data Representation (alignment and aggregate layout)"
    url: https://gitlab.com/x86-psABIs/x86-64-ABI
  - label: "GCC manual — Common Type Attributes (packed, aligned), Structure-Layout Pragmas"
    url: https://gcc.gnu.org/onlinedocs/gcc/Common-Type-Attributes.html
  - label: "Drepper — What Every Programmer Should Know About Memory, §3 (cache lines, false sharing)"
  - label: "man 3 htonl, man 3 endian (be32toh); RFC 1700 network byte order convention"
    url: https://man7.org/linux/man-pages/man3/endian.3.html
  - label: "Eric Raymond — The Lost Art of Structure Packing"
    url: http://www.catb.org/esr/structure-packing/
---

A `struct` is not "its members concatenated". The compiler inserts **padding** so that each member sits at an address the CPU can access efficiently (or at all), and rounds the total size up so arrays of the struct stay aligned. Layout affects memory use, cache behavior, binary compatibility and the safety of reading data from files and networks.

## Alignment

Every object type has an **alignment requirement**: its address must be a multiple of a power of two (`alignof(T)`). On x86-64 System V, the alignment of a scalar equals its size up to 8 (16 for `long double` and `__int128`, SSE types). Misaligned access is **undefined behavior** in C; x86 tolerates it (slowly or with extra cache-line crossings), but ARM (older), MIPS, SPARC and atomic/SIMD instructions may trap or silently misbehave.

Rules for struct layout:

1. Members are placed **in declaration order** (C forbids reordering, unlike Rust).
2. Each member is placed at the next offset that is a multiple of its alignment (**padding** inserted before it).
3. The struct's alignment is the **largest** alignment among its members.
4. The struct's size is rounded up to a multiple of its alignment (**tail padding**), so that in `T arr[n]` every element is aligned.

Measured on x86-64 (`layout.c` prints `sizeof`, `alignof` and `offsetof` of each member):

```c
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>

struct bad  { char a; double b; char c; int d; short e; };          /* declaration order as written */
struct good { double b; int d; short e; char a; char c; };          /* members sorted by decreasing alignment */
struct __attribute__((packed)) packed { char a; double b; char c; int d; short e; };

struct hdr {                       /* a wire-format-like header: avoid relying on struct layout for that! */
    uint8_t  version;
    uint16_t length;
    uint32_t id;
};

struct flex { size_t n; int items[]; };                              /* flexible array member */

#define SHOW(T, ...) do { printf("%-14s size=%2zu align=%zu\n", #T, sizeof(struct T), alignof(struct T)); __VA_ARGS__ } while (0)
#define OFF(T, m) printf("    %-4s offset=%2zu size=%zu\n", #m, offsetof(struct T, m), sizeof(((struct T *)0)->m))

int main(void) {
    SHOW(bad,    OFF(bad, a); OFF(bad, b); OFF(bad, c); OFF(bad, d); OFF(bad, e););
    SHOW(good,   OFF(good, b); OFF(good, d); OFF(good, e); OFF(good, a); OFF(good, c););
    SHOW(packed, OFF(packed, a); OFF(packed, b); OFF(packed, c); OFF(packed, d); OFF(packed, e););
    SHOW(hdr,    OFF(hdr, version); OFF(hdr, length); OFF(hdr, id););
    printf("flex: sizeof(struct flex)=%zu (items[] adds 0), allocate n items with sizeof + n*sizeof(int)\n", sizeof(struct flex));
    return 0;
}
```

```text
$ gcc -std=gnu11 -Wall -Wextra layout.c -o layout && ./layout
bad            size=32 align=8
    a    offset= 0 size=1
    b    offset= 8 size=8
    c    offset=16 size=1
    d    offset=20 size=4
    e    offset=24 size=2
good           size=16 align=8
    b    offset= 0 size=8
    d    offset= 8 size=4
    e    offset=12 size=2
    a    offset=14 size=1
    c    offset=15 size=1
packed         size=16 align=1
    a    offset= 0 size=1
    b    offset= 1 size=8
    c    offset= 9 size=1
    d    offset=10 size=4
    e    offset=14 size=2
hdr            size= 8 align=4
    version offset= 0 size=1
    length offset= 2 size=2
    id   offset= 4 size=4
flex: sizeof(struct flex)=8 (items[] adds 0), allocate n items with sizeof + n*sizeof(int)
```

Reading it:

- `bad` has the same members as `good`, but declaring `char a; double b; char c; int d; short e;` leaves 7 bytes of padding after `a` (to align `b` at 8), 3 after `c` (to align `d` at 4), and 6 of tail padding: **32 bytes** instead of **16**. Sorting members by **decreasing alignment** removes almost all padding. In a table of 100 million records that is 1.6 GB of difference, and fewer cache misses.
- `packed` (`__attribute__((packed))`) removes padding (size 16, alignment 1) but now `b` sits at offset 1: reading it is a misaligned access. GCC generates safe (byte-wise or unaligned-load) code when accessing members directly, yet **taking the address of a packed member** (`&p.b`) produces a normally-typed pointer that may be misaligned: UB (`-Waddress-of-packed-member` warns). Use packed only for exact external layouts, and access fields through the struct or `memcpy`.
- `hdr` shows `uint8_t version; uint16_t length; uint32_t id;` is **8 bytes**, with a padding byte after `version` and the fields at 0, 2, 4. A network or file header with that exact meaning is *not* what the struct represents: don't `fread`/`write` structs for wire/disk formats (layout, padding and endianness vary by compiler/ABI/version); serialize each field explicitly (below).
- `flex` is a **flexible array member** (C99): `int items[]` as the last member has no size; allocate `sizeof(struct flex) + n * sizeof(int)` bytes with a single `malloc`. It replaces the old `items[1]` hack (UB).

Useful tools: `offsetof(type, member)` (`<stddef.h>`), `_Static_assert(sizeof(struct hdr) == 8, "ABI")` to freeze layout, `alignas(64)` (`<stdalign.h>`) for cache-line alignment, `pahole -C struct_name binary` (dwarves) shows holes from DWARF, and `-Wpadded` warns when padding is inserted. The Linux kernel documents hot/cold field ordering to keep frequently accessed fields in one cache line.

### Cache lines and false sharing

A cache line is typically **64 bytes**. Fields that are used together should be adjacent (hot fields first); fields written by *different threads* should be on *different* cache lines, otherwise cores invalidate each other's lines even though they touch different variables (**false sharing**), which can slow a counter array by 10× or more:

```c
struct counter { _Alignas(64) _Atomic long value; };    /* each counter owns a cache line */
struct counter counters[NUM_THREADS];
```

### Struct assignment, comparison and initialization

- `a = b;` copies the whole struct, including padding (may be bitwise copy). Passing/returning small structs by value is fine; large ones are copied through memory (see the calling-convention note).
- **Don't compare structs with `memcmp` or hash their bytes**: padding bytes have indeterminate values, so two logically equal structs can differ. Compare member by member.
- Initialize with designated initializers and zero everything else: `struct config c = { .port = 8080, .name = "x" };` (omitted members are zero) or `= {0}` (C23: `= {}`). `memset(&s, 0, sizeof s)` also clears padding, which matters before writing a struct to a file or copying it to another trust domain (kernel→user): *uninitialized padding leaks stack bytes*, a classic kernel information-disclosure bug.
- **Compound literals** (C99): `draw((struct point){ .x = 1, .y = 2 });`.

## Unions

A `union` overlays its members at the same address; its size is that of the largest member (plus alignment padding). Only the last-written member is meaningful.

- **Type punning through a union is defined in C99/C11 (footnote to §6.5.2.3), undefined in C++.** `memcpy` is valid in both and compiles to the same code. Both appear in the output:

```c
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef union { float f; uint32_t u; } FloatBits;

/* A tagged union (sum type): the tag says which member is valid. */
typedef struct {
    enum { T_INT, T_DOUBLE, T_STR } tag;
    union { long i; double d; const char *s; } as;
} Value;

static void print(const Value *v) {
    switch (v->tag) {
    case T_INT:    printf("int %ld\n", v->as.i); break;
    case T_DOUBLE: printf("double %g\n", v->as.d); break;
    case T_STR:    printf("string \"%s\"\n", v->as.s); break;
    }
}

struct flags {                       /* bit-fields: layout is implementation-defined */
    unsigned ready : 1;
    unsigned mode  : 3;
    unsigned count : 12;
};

int main(void) {
    FloatBits fb = { .f = 1.0f };
    printf("1.0f as bits via union: 0x%08x (type punning through a union is defined in C, not C++)\n", fb.u);
    uint32_t viaMemcpy; float one = 1.0f; memcpy(&viaMemcpy, &one, sizeof one);
    printf("same via memcpy:        0x%08x\n", viaMemcpy);

    Value vs[] = { { T_INT, { .i = 42 } }, { T_DOUBLE, { .d = 2.5 } }, { T_STR, { .s = "text" } } };
    for (size_t i = 0; i < 3; i++) print(&vs[i]);
    printf("sizeof(Value) = %zu (tag + largest member + padding)\n", sizeof(Value));

    struct flags f = { .ready = 1, .mode = 5, .count = 4000 };
    uint32_t raw = 0; memcpy(&raw, &f, sizeof f);
    printf("sizeof(struct flags)=%zu, raw bytes as integer = 0x%08x  (bit order is compiler/ABI specific)\n", sizeof f, raw);
    return 0;
}
```

```text
$ gcc -std=gnu11 -Wall -Wextra unions.c -o unions && ./unions
1.0f as bits via union: 0x3f800000 (type punning through a union is defined in C, not C++)
same via memcpy:        0x3f800000
int 42
double 2.5
string "text"
sizeof(Value) = 16 (tag + largest member + padding)
sizeof(struct flags)=4, raw bytes as integer = 0x0000fa0b  (bit order is compiler/ABI specific)
```

- **Tagged union (discriminated union / sum type)**: pair a union with an explicit tag, as `Value` does; this is how interpreters, ASTs and protocol messages are modelled in C. Always `switch` on the tag; consider `-Wswitch-enum` so adding a tag value flags unhandled cases. Reading a member other than the last written may produce a trap representation or an unrelated value.
- Anonymous structs/unions (C11) allow `v.i` instead of `v.as.i`.
- Unions are the basis of `sockaddr` families, `epoll_data_t`, `siginfo_t`.

## Bit-fields

`unsigned ready : 1; unsigned mode : 3; unsigned count : 12;` declares members with explicit bit widths, packed into storage units. They are convenient but **their layout is implementation-defined**: allocation unit size, whether bits fill from the least- or most-significant end, whether a field may straddle units, signedness of plain `int` fields (a 1-bit `int` field can only hold 0 and −1), and the order across endianness. In the output above the struct occupied 4 bytes and the integer view was `0x0000fa0b` on little-endian GCC; another compiler/ABI may differ. Further restrictions: you can't take their address, `atomic` is awkward, and adjacent bit-fields share one memory location, so **concurrent updates to neighbors from different threads race** even with distinct names.

Guideline: use bit-fields for **in-memory** packing of flags where you control both ends. For **hardware registers, file formats and network protocols**, define masks and shifts explicitly:

```c
#define FLAG_READY   (1u << 0)
#define MODE_SHIFT   1
#define MODE_MASK    (0x7u << MODE_SHIFT)
#define COUNT_SHIFT  4
#define COUNT_MASK   (0xFFFu << COUNT_SHIFT)
uint32_t reg = FLAG_READY | (5u << MODE_SHIFT) | (4000u << COUNT_SHIFT);
unsigned mode = (reg & MODE_MASK) >> MODE_SHIFT;
```

(`<linux/bitfield.h>`'s `FIELD_GET/FIELD_PREP` generalizes this in the kernel.) Use **unsigned** operands for shifts, and keep register definitions in one header.

## Endianness and serialization

A multi-byte integer can be stored **little-endian** (least significant byte at the lowest address: x86, ARM Linux, RISC-V) or **big-endian** (most significant first: network order, historic SPARC/POWER/s390x/MIPS-BE). Network protocols (IP, TCP, DNS) use **big-endian** ("network byte order"); USB, PNG headers, many CPU-native formats are little. Casting a byte buffer to `uint32_t *` and dereferencing assumes host order, alignment, and no aliasing violation: three separate bugs.

Portable, alignment-safe, endianness-explicit read/write (`endian.c`): assemble integers from bytes with shifts, which compiles to a single load or `bswap` on modern compilers:

```c
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Portable, alignment-safe, endianness-explicit serialization. */
static void put_be32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static uint32_t get_be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static void put_le16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static uint16_t get_le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

int main(void) {
    uint32_t x = 0x0A0B0C0D;
    uint8_t *b = (uint8_t *)&x;
    printf("host memory bytes of 0x0A0B0C0D: %02x %02x %02x %02x -> %s-endian\n", b[0], b[1], b[2], b[3], b[0] == 0x0D ? "little" : "big");
    uint32_t n = htonl(x); b = (uint8_t *)&n;
    printf("htonl bytes (network order):       %02x %02x %02x %02x\n", b[0], b[1], b[2], b[3]);

    uint8_t buf[16] = {0};
    put_be32(buf + 1, 0xDEADBEEF);              /* odd offset: an unaligned (uint32_t *) store would be UB */
    put_le16(buf + 6, 0x1234);
    printf("buffer: "); for (int i = 0; i < 8; i++) printf("%02x ", buf[i]); printf("\n");
    printf("read back: be32=0x%08x le16=0x%04x\n", get_be32(buf + 1), get_le16(buf + 6));
    return 0;
}
```

```text
$ gcc -std=gnu11 -Wall -Wextra endian.c -o endian && ./endian
host memory bytes of 0x0A0B0C0D: 0d 0c 0b 0a -> little-endian
htonl bytes (network order):       0a 0b 0c 0d
buffer: 00 de ad be ef 00 34 12 
read back: be32=0xdeadbeef le16=0x1234
```

- `htonl/htons/ntohl/ntohs` (`<arpa/inet.h>`) convert 32/16-bit between host and network order; glibc `<endian.h>` adds `htobe64`, `le32toh`, … and `__builtin_bswap32/64` swap bytes. For compile-time detection use `__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__`.
- The buffer test wrote at an **odd offset** (`buf + 1`): the byte-wise `put_be32` is safe anywhere, whereas `*(uint32_t *)(buf + 1) = x` would be misaligned UB. For a one-off native-order read use `memcpy(&v, p, sizeof v)`; compilers turn it into a single load where the target allows (the cast variant happens to print the same value on x86, but is still undefined).
- Floating-point and structs: serialize each field; treat `float` via its `uint32_t` bits (`memcpy`) and agree on IEEE 754 with the peer.
- Sizes of `long`, `wchar_t`, enums and `bool` differ between platforms; use fixed-width types in any persisted or transmitted structure, and include a **version/magic** field plus length so the format can evolve.
- Alternatives: Protocol Buffers/FlatBuffers/Cap'n Proto/CBOR/MessagePack generate (de)serializers and define the byte layout for you.

### Check assumptions at compile time

```c
_Static_assert(sizeof(struct hdr) == 8, "struct hdr layout changed");
_Static_assert(offsetof(struct hdr, id) == 4, "id offset");
_Static_assert(sizeof(int) == 4, "int must be 32-bit");
_Static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "code assumes little endian");
```

## Design checklist

1. Order members by decreasing alignment unless readability/ABI says otherwise; verify with `pahole`/`offsetof`; keep hot fields together.
2. Never write structs directly to files/sockets; serialize fields; document endianness and versions.
3. Use `memcpy` (not pointer casts) for unaligned/typed access; avoid `packed` except for deliberate layouts and access members carefully.
4. Compare and hash structs member-wise; zero-initialize before copying across trust boundaries.
5. Prefer masks/shifts over bit-fields for hardware and wire formats.
6. Keep public struct layouts stable (ABI), or hide them behind opaque pointers (next notes).

## Key takeaways

- The compiler inserts padding for alignment: member order alone changed a struct from 32 to 16 bytes; `sizeof` is a multiple of `alignof`.
- Misaligned access is UB; `packed` changes layout and makes addresses of members unsafe; `memcpy` is the safe way to read unaligned data.
- Unions allow type punning in C and implement tagged unions; bit-field layout is implementation-defined, so don't use it for external formats.
- Convert endianness explicitly (`htonl`, shifts, `be32toh`); never cast byte buffers to wider integer pointers; zero padding before exporting structs.
