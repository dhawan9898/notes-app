/* Build an Ethernet + IPv4 + UDP frame by hand, parse it, then capture the kernel's own frame for the same datagram and compare.
   Needs root (an AF_PACKET socket) and an interface named lo that is up. */
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static uint16_t checksum(const uint8_t *d, size_t n) {                  /* ones' complement sum of 16-bit words (RFC 1071) */
    uint32_t s = 0;
    for (size_t i = 0; i + 1 < n; i += 2) s += (uint32_t)d[i] << 8 | d[i + 1];
    if (n & 1) s += (uint32_t)d[n - 1] << 8;
    while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
    return (uint16_t)~s;
}
static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static unsigned get16(const uint8_t *p) { return (unsigned)p[0] << 8 | p[1]; }

static size_t build_udp_frame(uint8_t *f, const uint8_t src_mac[6], const uint8_t dst_mac[6], const char *src_ip, const char *dst_ip,
                              unsigned sport, unsigned dport, const char *payload, unsigned ident) {
    size_t plen = strlen(payload), udp_len = 8 + plen;
    uint8_t sip[4], dip[4]; inet_pton(AF_INET, src_ip, sip); inet_pton(AF_INET, dst_ip, dip);
    memcpy(f, dst_mac, 6); memcpy(f + 6, src_mac, 6); put16(f + 12, 0x0800);          /* Ethernet II header */
    uint8_t *ip = f + 14;
    ip[0] = 0x45; ip[1] = 0; put16(ip + 2, (unsigned)(20 + udp_len)); put16(ip + 4, ident); put16(ip + 6, 0x4000);
    ip[8] = 64; ip[9] = 17; put16(ip + 10, 0); memcpy(ip + 12, sip, 4); memcpy(ip + 16, dip, 4);
    put16(ip + 10, checksum(ip, 20));
    uint8_t *udp = ip + 20;
    put16(udp, sport); put16(udp + 2, dport); put16(udp + 4, (unsigned)udp_len); put16(udp + 6, 0); memcpy(udp + 8, payload, plen);
    uint8_t pseudo[12 + 64]; memcpy(pseudo, sip, 4); memcpy(pseudo + 4, dip, 4); pseudo[8] = 0; pseudo[9] = 17; put16(pseudo + 10, (unsigned)udp_len);
    memcpy(pseudo + 12, udp, udp_len);
    unsigned c = checksum(pseudo, 12 + udp_len); put16(udp + 6, c ? c : 0xFFFF);        /* a computed 0 is sent as all ones */
    return 14 + 20 + udp_len;
}
static void show(const uint8_t *f, size_t len) {
    unsigned ihl = (f[14] & 0x0F) * 4;
    const uint8_t *udp = f + 14 + ihl;
    printf("   eth_dst        %02x:%02x:%02x:%02x:%02x:%02x\n", f[0], f[1], f[2], f[3], f[4], f[5]);
    printf("   eth_src        %02x:%02x:%02x:%02x:%02x:%02x\n", f[6], f[7], f[8], f[9], f[10], f[11]);
    printf("   ethertype      0x%04x\n   ip_total       %u\n   ttl            %u\n   proto          %u\n", get16(f + 12), get16(f + 16), f[22], f[23]);
    printf("   ip_checksum_ok %s\n", checksum(f + 14, ihl) == 0 ? "yes" : "no");
    printf("   src            %u.%u.%u.%u:%u\n   dst            %u.%u.%u.%u:%u\n", f[26], f[27], f[28], f[29], get16(udp), f[30], f[31], f[32], f[33], get16(udp + 2));
    unsigned ulen = get16(udp + 4);
    printf("   udp_len        %u\n   payload        \"%.*s\"\n", ulen, (int)(ulen - 8), (const char *)(udp + 8));
    (void)len;
}

int main(void) {
    const uint8_t zero[6] = {0};
    uint8_t mine[128], real[2048];
    size_t n = build_udp_frame(mine, zero, zero, "127.0.0.1", "127.0.0.1", 40000, 9999, "hello", 0x1234);
    printf("1. a frame built by hand: %zu bytes = 14 Ethernet + 20 IPv4 + 8 UDP + 5 payload\n   hex:", n);
    for (int i = 0; i < 20; i++) printf(" %02x", mine[i]);
    printf(" ...\n");
    show(mine, n);

    /* 2. send a real datagram over loopback, capture it with an AF_PACKET socket and compare with the hand-built frame */
    int cap = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (cap < 0) { perror("socket(AF_PACKET): run as root"); return 1; }
    struct sockaddr_ll ll = {.sll_family = AF_PACKET, .sll_protocol = htons(ETH_P_ALL), .sll_ifindex = (int)if_nametoindex("lo")};
    if (bind(cap, (struct sockaddr *)&ll, sizeof ll) < 0) { perror("bind lo"); return 1; }
    struct timeval tv = {2, 0}; setsockopt(cap, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    int tx = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in me = {.sin_family = AF_INET, .sin_port = htons(40000), .sin_addr.s_addr = htonl(0x7f000001)}, to = me;
    to.sin_port = htons(9999);
    bind(tx, (struct sockaddr *)&me, sizeof me); sendto(tx, "hello", 5, 0, (struct sockaddr *)&to, sizeof to);
    ssize_t got;
    do { got = recv(cap, real, sizeof real, 0); } while (got > 0 && !((size_t)got == n && real[23] == 17));
    if (got <= 0) { puts("\n2. no frame captured (is lo up?)"); return 1; }
    int diff[16], nd = 0;
    for (size_t i = 0; i < n && nd < 16; i++) if (mine[i] != real[i]) diff[nd++] = (int)i;
    printf("\n2. captured the kernel's own frame: %zd bytes; bytes that differ from mine: %d at offsets [", got, nd);
    for (int i = 0; i < nd; i++) printf("%s%d", i ? ", " : "", diff[i]);
    printf("]\n   IP identification: mine %02x%02x, kernel %02x%02x; IP checksum: mine %02x%02x, kernel %02x%02x\n", mine[18], mine[19], real[18], real[19], mine[24], mine[25], real[24], real[25]);
    printf("   UDP checksum: mine %02x%02x, kernel %02x%02x\n", mine[40], mine[41], real[40], real[41]);
    printf("   the kernel's frame parses to the same fields:\n");
    show(real, (size_t)got);
    return 0;
}
