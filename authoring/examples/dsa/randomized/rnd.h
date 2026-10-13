#include <stdint.h>
static uint64_t rng_state = 0x9E3779B97F4A7C15ULL;
static inline uint64_t rnd64(void) { rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17; return rng_state; }
static inline uint64_t rnd_below(uint64_t n) { uint64_t limit = UINT64_MAX - UINT64_MAX % n, x; do x = rnd64(); while (x >= limit); return x % n; }   // unbiased: reject the uneven tail
static inline double rnd01(void) { return (rnd64() >> 11) * (1.0 / 9007199254740992.0); }
