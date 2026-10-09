---
title: Structs, Alignment, Padding, Unions, Bit-fields and Endianness
track: c
module: Data Modeling
moduleOrder: 3
order: 1
level: intermediate
summary: How the compiler lays out aggregate types, why member order changes size, safe type punning, tagged unions, bit-field pitfalls, and portable serialization — with measured layouts and tested code.
sources:
  - label: ISO/IEC 9899:2018 (C17) §6.7.2.1 Structure and union specifiers; §6.2.8 Alignment of objects; §6.5.2.3
  - label: System V AMD64 psABI §3.1.2 Data Representation (alignment and aggregate layout)
    url: https://gitlab.com/x86-psABIs/x86-64-ABI
  - label: GCC manual — Common Type Attributes (packed, aligned), Structure-Layout Pragmas
    url: https://gcc.gnu.org/onlinedocs/gcc/Common-Type-Attributes.html
  - label: Drepper — What Every Programmer Should Know About Memory, §3 (cache lines, false sharing)
  - label: man 3 htonl, man 3 endian (be32toh); RFC 1700 network byte order convention
    url: https://man7.org/linux/man-pages/man3/endian.3.html
  - label: Eric Raymond — The Lost Art of Structure Packing
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

{{include c/structs/layout.c}}

{{run c/structs :: gcc -std=gnu11 -Wall -Wextra layout.c -o layout && ./layout}}

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

{{include c/structs/unions.c}}

{{run c/structs :: gcc -std=gnu11 -Wall -Wextra unions.c -o unions && ./unions}}

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

{{include c/structs/endian.c}}

{{run c/structs :: gcc -std=gnu11 -Wall -Wextra endian.c -o endian && ./endian}}

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
