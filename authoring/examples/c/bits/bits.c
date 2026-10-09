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
