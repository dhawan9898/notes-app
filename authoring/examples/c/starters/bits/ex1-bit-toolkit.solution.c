int popcount32(uint32_t x) { int n = 0; while (x) { x &= x - 1; n++; } return n; }    /* clears the lowest set bit */
int is_pow2(uint32_t x) { return x != 0 && (x & (x - 1)) == 0; }
uint32_t rotl32(uint32_t x, unsigned r) { r &= 31; return r ? (x << r) | (x >> (32 - r)) : x; }
uint32_t set_bit(uint32_t x, unsigned i) { return x | (1u << i); }
uint32_t clear_bit(uint32_t x, unsigned i) { return x & ~(1u << i); }
uint32_t toggle_bit(uint32_t x, unsigned i) { return x ^ (1u << i); }
uint32_t byte_swap(uint32_t x) { return (x >> 24) | ((x >> 8) & 0xff00u) | ((x << 8) & 0xff0000u) | (x << 24); }
