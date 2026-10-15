// Exercise 1: a bit toolkit for uint32_t.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
int popcount32(uint32_t x)                     { (void)x; return 0; }   /* try one loop iteration per set bit */
int is_pow2(uint32_t x)                        { (void)x; return 0; }   /* is_pow2(0) is false */
uint32_t rotl32(uint32_t x, unsigned r)        { (void)r; return x; }   /* must be defined for r == 0 */
uint32_t set_bit(uint32_t x, unsigned i)       { (void)i; return x; }
uint32_t clear_bit(uint32_t x, unsigned i)     { (void)i; return x; }
uint32_t toggle_bit(uint32_t x, unsigned i)    { (void)i; return x; }
uint32_t byte_swap(uint32_t x)                 { return x; }            /* 0x11223344 -> 0x44332211 */
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    check_int("popcount32(0)", popcount32(0), 0);
    check_int("popcount32(0xF0F0)", popcount32(0xF0F0u), 8);
    check_int("popcount32(0xFFFFFFFF)", popcount32(0xFFFFFFFFu), 32);
    check_int("is_pow2(0)", is_pow2(0), 0);
    check_int("is_pow2(1)", is_pow2(1), 1);
    check_int("is_pow2(64)", is_pow2(64), 1);
    check_int("is_pow2(96)", is_pow2(96), 0);
    check_int("rotl32(0x80000001, 1)", rotl32(0x80000001u, 1), 0x00000003);
    check_int("rotl32(0x12345678, 8)", rotl32(0x12345678u, 8), 0x34567812);
    check_int("rotl32(0x12345678, 0)", rotl32(0x12345678u, 0), 0x12345678);
    check_int("set_bit(0, 31)", set_bit(0, 31), 0x80000000u);
    check_int("clear_bit(0xFF, 0)", clear_bit(0xFFu, 0), 0xFE);
    check_int("toggle_bit(0x0F, 4)", toggle_bit(0x0Fu, 4), 0x1F);
    check_int("toggle_bit(0x1F, 4)", toggle_bit(0x1Fu, 4), 0x0F);
    check_int("byte_swap(0x11223344)", byte_swap(0x11223344u), 0x44332211);
    return summary();
}
