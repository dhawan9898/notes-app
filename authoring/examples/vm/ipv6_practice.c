/* IPv6 practice: compression and expansion (RFC 5952), address types, EUI-64, solicited-node multicast, prefix arithmetic. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef struct { uint8_t b[16]; } V6;

/* parse "2001:db8::1" (no embedded IPv4); returns 0 on success */
static int parse6(const char *s, V6 *out) {
    uint16_t head[8], tail[8]; int nh = 0, nt = 0, gap = 0;
    const char *p = s;
    if (p[0] == ':' && p[1] == ':') { gap = 1; p += 2; }
    while (*p) {
        char *end; unsigned long v = strtoul(p, &end, 16);
        if (end == p || end - p > 4 || v > 0xFFFF || nh + nt >= 8) return -1;
        if (gap) tail[nt++] = (uint16_t)v; else head[nh++] = (uint16_t)v;
        p = end;
        if (*p == ':') { if (p[1] == ':') { if (gap) return -1; gap = 1; p += 2; } else p++; }
    }
    if (!gap && nh != 8) return -1;
    memset(out, 0, sizeof *out);
    for (int i = 0; i < nh; i++) { out->b[2 * i] = (uint8_t)(head[i] >> 8); out->b[2 * i + 1] = (uint8_t)head[i]; }
    for (int i = 0; i < nt; i++) { int g = 8 - nt + i; out->b[2 * g] = (uint8_t)(tail[i] >> 8); out->b[2 * g + 1] = (uint8_t)tail[i]; }
    return 0;
}
static unsigned group(const V6 *a, int i) { return (unsigned)a->b[2 * i] << 8 | a->b[2 * i + 1]; }

static const char *exploded(const V6 *a) {
    static char buf[4][40]; static int k; char *s = buf[k++ & 3];
    for (int i = 0; i < 8; i++) sprintf(s + 5 * i, "%04x%s", group(a, i), i < 7 ? ":" : "");
    return s;
}
/* RFC 5952: lower case, no leading zeros, the longest run of two or more zero groups becomes "::" (the first one if tied) */
static const char *compressed(const V6 *a) {
    static char buf[4][40]; static int k; char *s = buf[k++ & 3];
    int best = -1, best_len = 0;
    for (int i = 0; i < 8;) {
        if (group(a, i)) { i++; continue; }
        int j = i; while (j < 8 && !group(a, j)) j++;
        if (j - i > best_len) { best = i; best_len = j - i; }
        i = j;
    }
    if (best_len < 2) best = -1;
    char *w = s; *w = 0;
    for (int i = 0; i < 8; i++) {
        if (i == best) { w += sprintf(w, "::"); i += best_len - 1; continue; }
        if (w != s && w[-1] != ':') *w++ = ':';
        w += sprintf(w, "%x", group(a, i));
    }
    return s;
}
static int in_prefix(const V6 *a, const char *prefix, int bits) {
    V6 p; parse6(prefix, &p);
    int whole = bits / 8, rest = bits % 8;
    if (memcmp(a->b, p.b, (size_t)whole)) return 0;
    return !rest || ((a->b[whole] ^ p.b[whole]) & (0xFF << (8 - rest)) & 0xFF) == 0;
}

int main(void) {
    /* 1. Compression and expansion */
    const char *forms[] = {"2001:0db8:0000:0000:0000:ff00:0042:8329", "2001:db8:0:0:1:0:0:1", "0000:0000:0000:0000:0000:0000:0000:0001"};
    for (int i = 0; i < 3; i++) { V6 a; parse6(forms[i], &a); printf("1. %s\n     compressed: %s\n     exploded:   %s\n", forms[i], compressed(&a), exploded(&a)); }

    /* 2. Address types */
    printf("\n");
    const char *types[] = {"::1", "fe80::1", "fd12:3456:789a::1", "2001:db8::1", "ff02::1", "2606:4700:4700::1111"};
    for (int i = 0; i < 6; i++) {
        V6 a; parse6(types[i], &a);
        const char *kind = in_prefix(&a, "::1", 128) ? "loopback" : in_prefix(&a, "fe80::", 10) ? "link-local (fe80::/10)" : in_prefix(&a, "fc00::", 7) ? "unique local (fc00::/7)"
                         : in_prefix(&a, "ff00::", 8) ? "multicast (ff00::/8)" : in_prefix(&a, "2001:db8::", 32) ? "documentation (2001:db8::/32)" : "global unicast";
        printf("2. %-22s %s\n", types[i], kind);
    }

    /* 3. EUI-64: interface ID from a MAC address (flip the universal/local bit, insert ff:fe) */
    uint8_t mac[6] = {0x00, 0x1a, 0x2b, 0x3c, 0x4d, 0x5e}, eui[8];
    memcpy(eui, mac, 3); eui[0] ^= 0x02; eui[3] = 0xff; eui[4] = 0xfe; memcpy(eui + 5, mac + 3, 3);
    V6 addr; parse6("2001:db8:1:2::", &addr); memcpy(addr.b + 8, eui, 8);
    printf("\n3. MAC 00:1a:2b:3c:4d:5e -> EUI-64 interface ID %02x%02x:%02x%02x:%02x%02x:%02x%02x -> address %s\n", eui[0], eui[1], eui[2], eui[3], eui[4], eui[5], eui[6], eui[7], compressed(&addr));

    /* 4. Solicited-node multicast: ff02::1:ff00:0/104 plus the last 24 bits of the address */
    V6 target, sn; parse6("2001:db8::1a:2b3c:4d5e", &target); parse6("ff02::1:ff00:0", &sn); memcpy(sn.b + 13, target.b + 13, 3);
    printf("4. neighbour solicitation for %s goes to %s\n", compressed(&target), compressed(&sn));

    /* 5. Prefix arithmetic */
    V6 site; parse6("2001:db8:abcd::", &site);
    unsigned subnets = 1u << (64 - 48);
    printf("\n5. a /48 holds %u,%03u /64 subnets; the first three are:\n", subnets / 1000, subnets % 1000);
    for (int i = 0; i < 3; i++) { V6 s = site; s.b[6] = (uint8_t)(i >> 8); s.b[7] = (uint8_t)i; printf("      %s/64\n", compressed(&s)); }
    char dec[32] = "1";                                                       /* 2^64 by repeated doubling of a decimal string */
    for (int step = 0; step < 64; step++) { int carry = 0; for (int i = (int)strlen(dec) - 1; i >= 0; i--) { int d = (dec[i] - '0') * 2 + carry; dec[i] = (char)('0' + d % 10); carry = d / 10; } if (carry) { memmove(dec + 1, dec, strlen(dec) + 1); dec[0] = (char)('0' + carry); } }
    printf("   addresses in one /64: 2^64 = ");
    for (size_t i = 0, n = strlen(dec); i < n; i++) printf("%s%c", i && (n - i) % 3 == 0 ? "," : "", dec[i]);
    V6 one; parse6("2001:db8::1", &one);
    printf("\n   reverse DNS name of 2001:db8::1: ");
    for (int i = 15; i >= 0; i--) printf("%x.%x.", one.b[i] & 15, one.b[i] >> 4);
    printf("ip6.arpa\n");
    return 0;
}
