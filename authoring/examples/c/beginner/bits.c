#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void print_bin(const char *label, uint32_t v, int width) {         /* print the lowest `width` bits, most significant first */
    printf("%-24s", label);
    for (int i = width - 1; i >= 0; i--) { putchar((v >> i & 1) ? '1' : '0'); if (i % 4 == 0 && i) putchar(' '); }
    printf("   = %u = 0x%X\n", v, v);
}

int main(void) {
    printf("-- the same number in three bases --\n");
    print_bin("42", 42, 8);
    print_bin("255", 255, 8);
    print_bin("0xC0A8 (192.168)", 0xC0A8, 16);

    printf("\n-- bit operations on 0b1100 (12) and 0b1010 (10) --\n");
    print_bin("12", 12, 4); print_bin("10", 10, 4);
    print_bin("12 & 10  (AND)", 12 & 10, 4);
    print_bin("12 | 10  (OR)", 12 | 10, 4);
    print_bin("12 ^ 10  (XOR)", 12 ^ 10, 4);
    print_bin("1 << 3   (shift left)", 1u << 3, 4);
    print_bin("12 >> 2  (shift right)", 12u >> 2, 4);

    printf("\n-- flags in one number: permission bits like rwx --\n");
    enum { READ = 4, WRITE = 2, EXEC = 1 };
    unsigned perm = READ | WRITE;                                           /* 6 = rw- */
    printf("perm = %u; can write? %s; can exec? %s\n", perm, (perm & WRITE) ? "yes" : "no", (perm & EXEC) ? "yes" : "no");
    perm |= EXEC; printf("after adding exec: %u (rwx); clearing write: %u (r-x)\n", perm, perm & ~WRITE);

    printf("\n-- masks: an IPv4 netmask is just 24 one-bits followed by 8 zero-bits --\n");
    uint32_t mask24 = 0xFFFFFF00u, ip = (192u << 24) | (168u << 16) | (10u << 8) | 77u;
    printf("ip & mask  = %u.%u.%u.%u  (the network address of 192.168.10.77/24)\n", (ip & mask24) >> 24, (ip & mask24) >> 16 & 255, (ip & mask24) >> 8 & 255, (ip & mask24) & 255);
    printf("ip & ~mask = %u  (the host part)\n", ip & ~mask24);

    printf("\n-- negative numbers: two's complement --\n");
    int8_t m1 = -1, m5 = -5; print_bin("-1 as 8 bits", (uint8_t)m1, 8); print_bin("-5 as 8 bits", (uint8_t)m5, 8);
    printf("(to negate: flip all bits, add 1.  ~5 + 1 = %d)\n", (int8_t)(~5 + 1));

    printf("\n-- limits: a uint8_t wraps around --\n");
    uint8_t b = 250; for (int i = 0; i < 8; i++) b++;
    printf("250 + 8 stored in 8 bits = %u (wrapped past 255)\n", b);

    printf("\n-- byte order: how 0x12345678 sits in memory on this machine --\n");
    uint32_t v = 0x12345678; unsigned char bytes[4]; memcpy(bytes, &v, 4);
    printf("bytes in memory order: %02X %02X %02X %02X  -> %s-endian\n", bytes[0], bytes[1], bytes[2], bytes[3], bytes[0] == 0x78 ? "little" : "big");
    return 0;
}
