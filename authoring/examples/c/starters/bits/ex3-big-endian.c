// Exercise 3: store and load integers in big-endian (network) order using only shifts.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
void put_u16(unsigned char *p, uint16_t v) { (void)p; (void)v; }
void put_u32(unsigned char *p, uint32_t v) { (void)p; (void)v; }
uint32_t get_u32(const unsigned char *p)   { (void)p; return 0; }
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
    unsigned char buf[6] = {0};
    put_u16(buf, 0xABCD);
    check_int("put_u16(0xABCD): byte 0", buf[0], 0xAB);
    check_int("put_u16(0xABCD): byte 1", buf[1], 0xCD);
    put_u32(buf + 2, 0x12345678u);
    check_int("put_u32(0x12345678): byte 0", buf[2], 0x12);
    check_int("put_u32(0x12345678): byte 3", buf[5], 0x78);
    const unsigned char wire[4] = {0xC0, 0xA8, 0x01, 0x02};     /* 192.168.1.2 as it travels on the wire */
    check_int("get_u32({C0 A8 01 02})", get_u32(wire), 0xC0A80102u);
    unsigned char round[4];
    put_u32(round, 0xDEADBEEFu);
    check_int("get_u32(put_u32(0xDEADBEEF))", get_u32(round), 0xDEADBEEFu);
    return summary();
}
