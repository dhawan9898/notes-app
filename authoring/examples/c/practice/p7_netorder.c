#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Serialise a header into bytes in network (big-endian) order without depending on the host's byte order. */
static void put_u16(unsigned char *p, uint16_t v) { p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v; }
static void put_u32(unsigned char *p, uint32_t v) { p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16); p[2] = (unsigned char)(v >> 8); p[3] = (unsigned char)v; }
static uint32_t get_u32(const unsigned char *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

int main(void) {
    unsigned char pkt[6];
    put_u16(pkt, 0xCAFE);
    put_u32(pkt + 2, 0x01020304);
    printf("wire bytes:");
    for (size_t i = 0; i < sizeof pkt; i++) printf(" %02x", pkt[i]);
    uint32_t raw;
    memcpy(&raw, pkt + 2, 4);                          /* reinterpret the 4 wire bytes as a host integer */
    printf("\nget_u32 -> 0x%08x, ntohl(raw) -> 0x%08x (same value, on any host)\n", get_u32(pkt + 2), ntohl(raw));
    return 0;
}
