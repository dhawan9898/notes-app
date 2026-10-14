#include <stdint.h>
#include <stdio.h>

static int popcount32(uint32_t x) { int n = 0; while (x) { x &= x - 1; n++; } return n; }       /* clears the lowest set bit */
static int is_pow2(uint32_t x) { return x != 0 && (x & (x - 1)) == 0; }
static uint32_t rotl32(uint32_t x, unsigned r) { r &= 31; return r ? (x << r) | (x >> (32 - r)) : x; }   /* r == 0 would shift by 32: undefined */
static uint32_t set_bit(uint32_t x, unsigned i)    { return x | (1u << i); }
static uint32_t clear_bit(uint32_t x, unsigned i)  { return x & ~(1u << i); }
static uint32_t toggle_bit(uint32_t x, unsigned i) { return x ^ (1u << i); }
static uint32_t byte_swap(uint32_t x) { return (x >> 24) | ((x >> 8) & 0xff00u) | ((x << 8) & 0xff0000u) | (x << 24); }

int main(void) {
    printf("popcount(255) = %d, popcount(0xF0F0F0F0) = %d, popcount(0) = %d\n", popcount32(255), popcount32(0xF0F0F0F0u), popcount32(0));
    printf("is_pow2: 64 -> %d, 96 -> %d, 0 -> %d, 1 -> %d\n", is_pow2(64), is_pow2(96), is_pow2(0), is_pow2(1));
    printf("rotl32(0x80000001, 1) = 0x%08x, rotl32(x, 0) = 0x%08x\n", rotl32(0x80000001u, 1), rotl32(0x80000001u, 0));
    printf("set bit 4 of 0x01: 0x%02x, clear bit 0 of 0x03: 0x%02x, toggle bit 7 of 0x0f: 0x%02x\n", set_bit(1, 4), clear_bit(3, 0), toggle_bit(0x0f, 7));
    printf("byte_swap(0x11223344) = 0x%08x\n", byte_swap(0x11223344u));
    uint32_t probe = 1;
    printf("this machine is %s-endian (first byte of 1 is %d)\n", *(unsigned char *)&probe ? "little" : "big", *(unsigned char *)&probe);
    return 0;
}
