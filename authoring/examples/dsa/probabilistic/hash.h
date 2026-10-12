#ifndef HASH_H
#define HASH_H
#include <stdint.h>
#include <string.h>

/* SplitMix64 / Murmur3 finalizer style mixing: every input bit affects every output bit with probability ~1/2 (avalanche). */
static inline uint64_t mix64(uint64_t x) {
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}
/* Seeded 64-bit hash of a 64-bit key. */
static inline uint64_t hash64(uint64_t key, uint64_t seed) { return mix64(key + 0x9e3779b97f4a7c15ULL * (seed + 1)); }

/* FNV-1a over bytes: simple, but weak avalanche on short inputs. */
static inline uint64_t fnv1a(const void *p, size_t n) {
    const unsigned char *s = p; uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++) { h ^= s[i]; h *= 1099511628211ULL; }
    return h;
}
#endif
