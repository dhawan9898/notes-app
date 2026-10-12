---
title: Bit Manipulation
track: c
module: Systems-Level C
moduleOrder: 7
order: 1
level: intermediate
summary: The bitwise toolbox — masks and fields, classic identities, compiler builtins and CPU instructions, bitsets, SWAR techniques, shift pitfalls — with tested code and a measured comparison of five popcount implementations.
prerequisites:
  - Binary, Hexadecimal and Bits
sources:
  - label: "Warren — Hacker's Delight, 2nd ed. (Addison-Wesley, 2012)"
  - label: "Anderson — Bit Twiddling Hacks"
    url: https://graphics.stanford.edu/~seander/bithacks.html
  - label: "GCC manual — Other Built-in Functions (__builtin_popcount, clz, ctz, ffs, bswap, parity); C23 <stdbit.h>"
    url: https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
  - label: "Linux kernel — include/linux/bitops.h, bitfield.h, bits.h"
    url: https://docs.kernel.org/core-api/kernel-api.html#bit-operations
  - label: "Intel SDM — POPCNT, LZCNT, TZCNT, BMI1/BMI2 (PDEP/PEXT)"
---

Bit-level operations underlie flags, hardware registers, protocols, compression, hashing, bitmaps, allocators and cryptography. C gives you six operators — `& | ^ ~ << >>` — and a few traps; compilers and CPUs add fast primitives.

## Rules before tricks

1. **Use unsigned types** for bit manipulation: shifts and masks on signed values hit undefined or implementation-defined behavior (`1 << 31` overflows `int`; right-shifting a negative value is implementation-defined; left-shifting a negative is undefined).
2. **Integer promotions**: `uint8_t`/`uint16_t` operands become `int` first (so `~b` sets high bits; cast back or mask).
3. **Shift counts** must be `0 ≤ n < width of the promoted left operand`; `1u << 32` is undefined. Use `1ULL` for 64-bit masks (`BIT(40)` with plain `1` is a bug).
4. **Precedence**: comparison binds tighter than bitwise (`x & MASK == 0` parses as `x & (MASK == 0)`); always parenthesize: `(x & MASK) == 0`.
5. `~0u`, `-1u` are all-ones in the type; `UINT32_MAX` is clearer.

## The core operations

| Goal | Expression |
|---|---|
| Set bit *n* | `x \| (1ULL << n)` |
| Clear bit *n* | `x & ~(1ULL << n)` |
| Toggle bit *n* | `x ^ (1ULL << n)` |
| Test bit *n* | `(x >> n) & 1` |
| Low *w*-bit mask | `(1ULL << w) - 1` (for *w* < 64) |
| Extract field [shift, shift+w) | `(x >> shift) & mask(w)` |
| Insert field | `(x & ~(mask << shift)) \| ((v & mask) << shift)` |
| Is power of two | `x && !(x & (x - 1))` |
| Isolate lowest set bit | `x & -x` |
| Clear lowest set bit | `x & (x - 1)` |
| Round up to power of two | `x--; x \|= x>>1; ... x \|= x>>16; x++` |
| Align up to *a* (power of 2) | `(x + a - 1) & ~(a - 1)` |
| Swap without temp | `a ^= b; b ^= a; a ^= b` (slower than a temp on modern CPUs; breaks if `a` and `b` alias) |
| Sign-extend *n*-bit field | `(v ^ m) - m` with `m = 1 << (n-1)` |
| Rotate left | `(x << r) \| (x >> (32 - r))` with `r &= 31` (compilers emit `ROL`) |
| Gray code | `x ^ (x >> 1)` |
| Absolute difference / min / max branchless | `y ^ ((x ^ y) & -(x < y))` |

Everything in one runnable file:

```c
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- single-bit operations ---- */
#define BIT(n)            (1ULL << (n))
#define SET(x, n)         ((x) |  BIT(n))
#define CLEAR(x, n)       ((x) & ~BIT(n))
#define TOGGLE(x, n)      ((x) ^  BIT(n))
#define TEST(x, n)        (((x) >> (n)) & 1ULL)

/* ---- fields ---- */
#define MASK(width)                ((1ULL << (width)) - 1)                    /* width < 64 */
#define GET_FIELD(x, shift, width) (((x) >> (shift)) & MASK(width))
#define SET_FIELD(x, shift, width, v) (((x) & ~(MASK(width) << (shift))) | (((uint64_t)(v) & MASK(width)) << (shift)))

/* ---- classic tricks ---- */
static inline bool     is_pow2(uint64_t x)      { return x && !(x & (x - 1)); }
static inline uint64_t lowest_set(uint64_t x)   { return x & -x; }              /* isolate the lowest 1 bit */
static inline uint64_t clear_lowest(uint64_t x) { return x & (x - 1); }         /* Kernighan: clears the lowest 1 bit */
static inline unsigned popcount_kernighan(uint64_t x) { unsigned c = 0; while (x) { x &= x - 1; c++; } return c; }
static inline uint32_t round_up_pow2(uint32_t x) { x--; x |= x >> 1; x |= x >> 2; x |= x >> 4; x |= x >> 8; x |= x >> 16; return x + 1; }
static inline uint32_t rotl32(uint32_t x, unsigned r) { return (x << (r & 31)) | (x >> ((32 - r) & 31)); }   /* compiles to one ROL */
static inline int32_t  sign_extend(uint32_t v, unsigned bits) { uint32_t m = 1u << (bits - 1); return (int32_t)((v ^ m) - m); }
static inline uint32_t reverse_bits8(uint32_t b) { b = (b & 0xF0) >> 4 | (b & 0x0F) << 4; b = (b & 0xCC) >> 2 | (b & 0x33) << 2; b = (b & 0xAA) >> 1 | (b & 0x55) << 1; return b; }
static inline unsigned gray(unsigned x) { return x ^ (x >> 1); }
/* SWAR: does any byte of a 32-bit word equal zero? (strlen/memchr use this idea) */
static inline bool has_zero_byte(uint32_t v) { return ((v - 0x01010101u) & ~v & 0x80808080u) != 0; }

/* ---- a bitset ---- */
typedef struct { uint64_t *w; size_t nbits; } Bitset;
static Bitset bs_new(size_t nbits) { return (Bitset){ calloc((nbits + 63) / 64, sizeof(uint64_t)), nbits }; }
static void bs_set(Bitset *b, size_t i)    { b->w[i >> 6] |= 1ULL << (i & 63); }
static bool bs_test(const Bitset *b, size_t i) { return b->w[i >> 6] >> (i & 63) & 1; }
static size_t bs_count(const Bitset *b) { size_t c = 0; for (size_t i = 0; i < (b->nbits + 63) / 64; i++) c += (size_t)__builtin_popcountll(b->w[i]); return c; }

int main(void) {
    uint64_t x = 0;
    x = SET(x, 3); x = SET(x, 5); x = TOGGLE(x, 0); x = CLEAR(x, 3);
    printf("after set/toggle/clear: 0x%llx, bit5=%llu bit3=%llu\n", (unsigned long long)x, (unsigned long long)TEST(x, 5), (unsigned long long)TEST(x, 3));

    uint64_t reg = 0;
    reg = SET_FIELD(reg, 4, 12, 0xABC);  reg = SET_FIELD(reg, 0, 4, 0x9);
    printf("register = 0x%llx, field[4..15] = 0x%llx, field[0..3] = 0x%llx\n", (unsigned long long)reg, (unsigned long long)GET_FIELD(reg, 4, 12), (unsigned long long)GET_FIELD(reg, 0, 4));

    printf("is_pow2(64)=%d is_pow2(96)=%d is_pow2(0)=%d\n", is_pow2(64), is_pow2(96), is_pow2(0));
    printf("lowest_set(0b101000)=%llu clear_lowest(0b101000)=%llu\n", (unsigned long long)lowest_set(0x28), (unsigned long long)clear_lowest(0x28));
    printf("popcount(0xF0F0F0F0F0F0F0F0): kernighan=%u builtin=%d\n", popcount_kernighan(0xF0F0F0F0F0F0F0F0ULL), __builtin_popcountll(0xF0F0F0F0F0F0F0F0ULL));
    printf("round_up_pow2(1000)=%u round_up_pow2(1024)=%u\n", round_up_pow2(1000), round_up_pow2(1024));
    printf("rotl32(0x80000001, 1)=0x%x   sign_extend(0xFF, 8)=%d sign_extend(0x7F, 8)=%d\n", rotl32(0x80000001u, 1), sign_extend(0xFF, 8), sign_extend(0x7F, 8));
    printf("reverse_bits8(0x01)=0x%x reverse_bits8(0xB4)=0x%x   gray: ", reverse_bits8(1), reverse_bits8(0xB4));
    for (unsigned i = 0; i < 8; i++) printf("%u ", gray(i));
    printf("\nhas_zero_byte(0x41424300)=%d has_zero_byte(0x41424344)=%d\n", has_zero_byte(0x41424300u), has_zero_byte(0x41424344u));
    printf("ctz(0x28)=%d clz32(0x28)=%d ffs(0x28)=%d  (builtins map to TZCNT/LZCNT/BSF)\n", __builtin_ctz(0x28), __builtin_clz(0x28), __builtin_ffs(0x28));

    Bitset b = bs_new(1000);
    for (size_t i = 0; i < 1000; i += 7) bs_set(&b, i);
    printf("bitset: %zu bits set, test(14)=%d test(15)=%d, memory=%zu bytes for 1000 flags\n", bs_count(&b), bs_test(&b, 14), bs_test(&b, 15), (b.nbits + 63) / 64 * 8);
    free(b.w);

    /* shifts: the traps */
    int sh = 33;
    printf("1u << 31 = %u; (1u << (sh %% 32)) with sh=33 = %u; 1ULL << 40 = %llu\n", 1u << 31, 1u << (sh % 32), 1ULL << 40);
    int8_t neg = -16;
    printf("neg>>2 = %d (arithmetic shift for signed: implementation-defined); (uint8_t)neg>>2 = %u\n", neg >> 2, (uint8_t)neg >> 2);
    return 0;
}
```

```text
$ gcc -std=gnu11 -Wall -Wextra -fsanitize=undefined bits.c -o bits && ./bits
after set/toggle/clear: 0x21, bit5=1 bit3=0
register = 0xabc9, field[4..15] = 0xabc, field[0..3] = 0x9
is_pow2(64)=1 is_pow2(96)=0 is_pow2(0)=0
lowest_set(0b101000)=8 clear_lowest(0b101000)=32
popcount(0xF0F0F0F0F0F0F0F0): kernighan=32 builtin=32
round_up_pow2(1000)=1024 round_up_pow2(1024)=1024
rotl32(0x80000001, 1)=0x3   sign_extend(0xFF, 8)=-1 sign_extend(0x7F, 8)=127
reverse_bits8(0x01)=0x80 reverse_bits8(0xB4)=0x2d   gray: 0 1 3 2 6 7 5 4 
has_zero_byte(0x41424300)=1 has_zero_byte(0x41424344)=0
ctz(0x28)=3 clz32(0x28)=26 ffs(0x28)=4  (builtins map to TZCNT/LZCNT/BSF)
bitset: 143 bits set, test(14)=1 test(15)=0, memory=128 bytes for 1000 flags
1u << 31 = 2147483648; (1u << (sh % 32)) with sh=33 = 2; 1ULL << 40 = 1099511627776
neg>>2 = -4 (arithmetic shift for signed: implementation-defined); (uint8_t)neg>>2 = 60
```

Walk through the output:

- `SET_FIELD`/`GET_FIELD` build and read a register image (`0xabc9`: a 12-bit field at bit 4 and a 4-bit field at bit 0). Define your register layouts with such macros (the kernel has `FIELD_GET/FIELD_PREP`).
- `lowest_set(0b101000)` = 8 and `clear_lowest` = 32 are the primitives behind iterating set bits: `while (x) { int i = ctz(x); ...; x &= x - 1; }`.
- `sign_extend(0xFF, 8)` = −1 converts a raw 8-bit two's-complement field to `int` (used when decoding ADC samples, instruction immediates, network fields).
- `has_zero_byte` is **SWAR** ("SIMD within a register"): test four bytes at once for a NUL, the heart of fast `strlen`/`memchr`.
- The bitset stores 1000 flags in 128 bytes (vs 1000+ bytes of `bool`), counting with popcount; bitmaps appear in allocators (free-page maps), filesystems (block/inode bitmaps), schedulers (CPU masks, `cpu_set_t`), sets of small integers, Bloom filters.
- Shift traps: `1u << (33 % 32)` is 2 (the mask avoids UB), and `neg >> 2` on a negative `int8_t` gives −4 (arithmetic shift on GCC, implementation-defined) while the same bits as `uint8_t` give 60.

## Compiler builtins and CPU instructions

| Builtin (GCC/Clang) | Meaning | x86 instruction |
|---|---|---|
| `__builtin_popcount(l/ll)(x)` | number of 1 bits | `POPCNT` (needs `-mpopcnt`/`-march=native`; otherwise a library call) |
| `__builtin_clz(x)` / `ctz(x)` | leading / trailing zero count (**undefined for 0!**) | `LZCNT`/`BSR`, `TZCNT`/`BSF` |
| `__builtin_ffs(x)` | 1-based index of lowest set bit, 0 if none | `BSF` + fixup |
| `__builtin_parity(x)` | parity of set bits | `PF` flag |
| `__builtin_bswap16/32/64(x)` | byte swap (endianness) | `BSWAP`/`MOVBE` |
| `__builtin_bitreverse*` (Clang) | reverse bits | ARM `RBIT` |
| `__builtin_add_overflow` etc. | checked arithmetic | `ADD`+`JO/JC` |
| `__builtin_expect(x, 1)` | branch hint | layout only |

C23 standardizes many in `<stdbit.h>` (`stdc_count_ones`, `stdc_leading_zeros`, `stdc_trailing_zeros`, `stdc_bit_width`, `stdc_bit_ceil`, `stdc_first_leading_one`, ...) available in glibc ≥ 2.39. MSVC has `_BitScanForward`, `__popcnt`. BMI2 `PDEP/PEXT` (parallel bit deposit/extract) accelerate bitboard chess engines and compression; AVX-512 has `VPOPCNT`.

### How much does it matter? Five popcounts

Same task (count the 1 bits of 20 million random 64-bit values), five implementations (`bitbench.c`):

```c
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static unsigned pc_loop(uint64_t x) { unsigned c = 0; while (x) { c += x & 1; x >>= 1; } return c; }
static unsigned pc_kern(uint64_t x) { unsigned c = 0; while (x) { x &= x - 1; c++; } return c; }
static unsigned pc_table(uint64_t x) { static unsigned char t[256]; static int init; if (!init) { for (int i = 0; i < 256; i++) t[i] = (unsigned char)((i & 1) + t[i / 2]); init = 1; } unsigned c = 0; for (int i = 0; i < 8; i++) c += t[(x >> (8 * i)) & 255]; return c; }
static unsigned pc_swar(uint64_t x) { x = x - ((x >> 1) & 0x5555555555555555ULL); x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL); x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL; return (unsigned)((x * 0x0101010101010101ULL) >> 56); }
static unsigned pc_builtin(uint64_t x) { return (unsigned)__builtin_popcountll(x); }
typedef unsigned (*fn)(uint64_t);
int main(void) {
    enum { N = 20000000 };
    uint64_t *v = malloc(N * sizeof *v); uint64_t s = 88172645463325252ULL;
    for (int i = 0; i < N; i++) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; v[i] = s; }
    struct { const char *name; fn f; } impls[] = { {"bit loop", pc_loop}, {"Kernighan", pc_kern}, {"table (bytes)", pc_table}, {"SWAR", pc_swar}, {"__builtin_popcountll", pc_builtin} };
    unsigned long ref = 0;
    for (size_t k = 0; k < 5; k++) {
        double t0 = now(); unsigned long total = 0;
        for (int i = 0; i < N; i++) total += impls[k].f(v[i]);
        double dt = now() - t0;
        if (k == 0) ref = total;
        printf("%-22s %7.1f ms  (%5.2f ns/value) %s\n", impls[k].name, dt * 1e3, dt * 1e9 / N, total == ref ? "" : "MISMATCH");
    }
    free(v); return 0;
}
```

```text
$ gcc -O2 bitbench.c -o bitbench && ./bitbench; echo '--- with -mpopcnt (hardware POPCNT):'; gcc -O2 -mpopcnt bitbench.c -o bitbench_popcnt && ./bitbench_popcnt | tail -n 1
bit loop                 518.1 ms  (25.90 ns/value) 
Kernighan                950.3 ms  (47.52 ns/value) 
table (bytes)            177.1 ms  ( 8.85 ns/value) 
SWAR                      74.3 ms  ( 3.72 ns/value) 
__builtin_popcountll     125.9 ms  ( 6.30 ns/value) 
--- with -mpopcnt (hardware POPCNT):
__builtin_popcountll      59.1 ms  ( 2.95 ns/value) 
```

- The simple loop and Kernighan's trick cost ~25 ns each (data-dependent: proportional to the bit width or the count, with branch mispredictions).
- A byte table and the branch-free **SWAR** version are 5–10× faster (SWAR needs no memory lookups).
- Without `-mpopcnt` GCC compiles `__builtin_popcountll` into a call to a libgcc helper (`__popcountdi2`), slower than inline SWAR here; **with `-mpopcnt` it becomes the single `POPCNT` instruction** and is the fastest (~1.8 ns, mostly loop and load overhead).
- The lesson: let the compiler/CPU do it, but *tell the compiler the target* (`-march=x86-64-v2` or later, `-mpopcnt`, `-march=native` for local builds) or use runtime dispatch (`__builtin_cpu_supports("popcnt")`, function multiversioning `__attribute__((target_clones(...)))`).

## Bit fields vs. masks, and endianness

Use masks/shifts (or bitfield macros) for external layouts; bit-fields are implementation-defined (see the structs note). When fields cross bytes, handle byte order explicitly: assemble from bytes with shifts rather than casting buffers. `__builtin_bswap32` and `htonl` convert.

## Bit-level data structures and algorithms

- **Bitsets / bit vectors / bitmaps**: set operations in O(n/64): union `|`, intersection `&`, difference `& ~`, symmetric difference `^`. Roaring bitmaps compress sparse sets.
- **Bloom filters** and hashing with bit mixing (`x ^= x >> 33; x *= 0xff51afd7ed558ccd`...).
- **Bit-parallel algorithms**: bitboards (chess), Shift-And / bitap string matching, bit-parallel LCS/edit distance, sieve of Eratosthenes with a bit array, subset enumeration with masks (`for (s = m; s; s = (s - 1) & m)`), bitmask DP (TSP states), Gosper's hack to iterate fixed-popcount values.
- **Allocators and schedulers**: find-first-set to locate free slots (`ffs`/`ctz` on a bitmap word), priority bitmaps (Linux real-time scheduler uses a bitmap of runnable priorities).
- **Compression and parsing**: variable-length integers (LEB128, UTF-8 continuation bits `10xxxxxx`), bit readers/writers for Huffman/DEFLATE, unary and Elias-gamma coding.
- **Networking and hardware**: flag words (TCP flags), IPv4 masks (`addr & mask`, prefix length via popcount of the mask), CRC/LFSR shifts, register maps.
- **Atomic bit operations**: `atomic_fetch_or`, `atomic_fetch_and`; the kernel's `test_and_set_bit()`.

## Safe-coding checklist for bit code

1. Use `uint32_t`/`uint64_t` and `unsigned` constants (`1u`, `1ULL`); never shift `int` into the sign bit.
2. Validate shift counts (`n < 64`); mask them when wrapping is intended (`n & 63`).
3. Parenthesize every bitwise expression and compare against explicit values.
4. Beware `clz(0)`/`ctz(0)` (undefined); check for zero first or use C23 `stdc_*` (defined for 0).
5. Test with edge values (0, 1, all ones, high bit, width−1) and property tests against a simple reference implementation (like `pc_loop`).
6. Encapsulate layouts in named macros/inline functions; `static_assert` field widths add up to the register size.
7. Remember portability: endianness, `long` width (LP64 vs LLP64), compiler builtins vs MSVC intrinsics.

## Key takeaways

- Work in unsigned types, parenthesize, mask shift counts, and use `1ULL` for wide masks; promotions turn `uint8_t` operations into `int` operations.
- The core idioms: set/clear/toggle/test, field extract/insert, `x & (x-1)`, `x & -x`, power-of-two checks, sign extension, rotation.
- Prefer builtins (`popcount`, `clz`, `ctz`, `bswap`) and give the compiler the target ISA flag; the same popcount ranged from 25 ns (loop) to under 2 ns (hardware instruction).
- Bit-level structures (bitsets, bitmaps, SWAR, bitmask DP) give compact storage and word-parallel speed.
