// Exercise 9: network address of an IPv4 address and prefix length, using a mask and &.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IP(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))

/* ===== Your code: edit below this line ===== */
/* Return ip with the host bits (the last 32 - prefix bits) cleared. prefix is 0 to 32. */
uint32_t network_of(uint32_t ip, int prefix) {
    (void)prefix;
    return ip;      /* build the mask, then return ip & mask */
}
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
    check_int("10.20.77.130/21 -> 10.20.72.0", network_of(IP(10, 20, 77, 130), 21), IP(10, 20, 72, 0));
    check_int("192.168.1.77/24 -> 192.168.1.0", network_of(IP(192, 168, 1, 77), 24), IP(192, 168, 1, 0));
    check_int("172.16.5.4/12 -> 172.16.0.0", network_of(IP(172, 16, 5, 4), 12), IP(172, 16, 0, 0));
    check_int("10.1.2.3/32 -> 10.1.2.3", network_of(IP(10, 1, 2, 3), 32), IP(10, 1, 2, 3));
    check_int("10.1.2.3/0 -> 0.0.0.0", network_of(IP(10, 1, 2, 3), 0), 0);
    return summary();
}
