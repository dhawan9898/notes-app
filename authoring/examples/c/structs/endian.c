#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Portable, alignment-safe, endianness-explicit serialization. */
static void put_be32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static uint32_t get_be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static void put_le16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static uint16_t get_le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

int main(void) {
    uint32_t x = 0x0A0B0C0D;
    uint8_t *b = (uint8_t *)&x;
    printf("host memory bytes of 0x0A0B0C0D: %02x %02x %02x %02x -> %s-endian\n", b[0], b[1], b[2], b[3], b[0] == 0x0D ? "little" : "big");
    uint32_t n = htonl(x); b = (uint8_t *)&n;
    printf("htonl bytes (network order):       %02x %02x %02x %02x\n", b[0], b[1], b[2], b[3]);

    uint8_t buf[16] = {0};
    put_be32(buf + 1, 0xDEADBEEF);              /* odd offset: an unaligned (uint32_t *) store would be UB */
    put_le16(buf + 6, 0x1234);
    printf("buffer: "); for (int i = 0; i < 8; i++) printf("%02x ", buf[i]); printf("\n");
    printf("read back: be32=0x%08x le16=0x%04x\n", get_be32(buf + 1), get_le16(buf + 6));
    return 0;
}
