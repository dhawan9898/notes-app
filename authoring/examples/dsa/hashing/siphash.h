// SipHash-2-4 (Aumasson & Bernstein): a keyed 64-bit hash for untrusted keys.
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))
#define SIPROUND do { \
    v0 += v1; v1 = ROTL(v1, 13); v1 ^= v0; v0 = ROTL(v0, 32); \
    v2 += v3; v3 = ROTL(v3, 16); v3 ^= v2; \
    v0 += v3; v3 = ROTL(v3, 21); v3 ^= v0; \
    v2 += v1; v1 = ROTL(v1, 17); v1 ^= v2; v2 = ROTL(v2, 32); } while (0)

static uint64_t siphash24(const void *data, size_t len, const uint8_t key[16]) {
    uint64_t k0, k1; memcpy(&k0, key, 8); memcpy(&k1, key + 8, 8);        // little-endian host assumed
    uint64_t v0 = 0x736f6d6570736575ULL ^ k0, v1 = 0x646f72616e646f6dULL ^ k1;
    uint64_t v2 = 0x6c7967656e657261ULL ^ k0, v3 = 0x7465646279746573ULL ^ k1;
    const uint8_t *p = data, *end = p + (len & ~(size_t)7);
    for (; p != end; p += 8) { uint64_t m; memcpy(&m, p, 8); v3 ^= m; SIPROUND; SIPROUND; v0 ^= m; }
    uint64_t b = (uint64_t)len << 56;
    for (size_t i = 0; i < (len & 7); i++) b |= (uint64_t)p[i] << (8 * i);
    v3 ^= b; SIPROUND; SIPROUND; v0 ^= b;
    v2 ^= 0xff; SIPROUND; SIPROUND; SIPROUND; SIPROUND;
    return v0 ^ v1 ^ v2 ^ v3;
}
