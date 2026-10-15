/* IPv4 practice: VLSM allocation, route summarisation, longest-prefix match and fragmentation arithmetic. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef uint32_t u32;
static u32 ip(const char *s) { unsigned a, b, c, d; sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d); return a << 24 | b << 16 | c << 8 | d; }
static const char *dot(u32 x) {
    static char buf[8][16]; static int i;
    char *p = buf[i++ & 7];
    snprintf(p, 16, "%u.%u.%u.%u", (unsigned)(x >> 24), (unsigned)(x >> 16 & 255), (unsigned)(x >> 8 & 255), (unsigned)(x & 255));
    return p;
}
static const char *cidr(u32 a, int p) { static char buf[8][20]; static int i; char *s = buf[i++ & 7]; snprintf(s, 20, "%s/%d", dot(a), p); return s; }
static u32 mask_of(int p) { return p ? 0xFFFFFFFFu << (32 - p) : 0; }

typedef struct { const char *name; int hosts; } Need;
static int by_hosts_desc(const void *x, const void *y) { return ((const Need *)y)->hosts - ((const Need *)x)->hosts; }

/* merge neighbouring blocks of equal size into their parent until nothing changes */
static int collapse(u32 *net, int *len, int n) {
    for (int changed = 1; changed;) {
        changed = 0;
        for (int i = 0; i < n && !changed; i++)
            for (int j = 0; j < n && !changed; j++)
                if (i != j && len[i] == len[j] && len[i] > 0 && (net[i] ^ net[j]) == 1u << (32 - len[i])) {
                    net[i] &= mask_of(len[i] - 1); len[i]--; net[j] = net[n - 1]; len[j] = len[n - 1]; n--; changed = 1;
                }
    }
    for (int i = 1; i < n; i++)                                            /* sort by address for a stable listing */
        for (int j = i; j > 0 && net[j] < net[j - 1]; j--) { u32 t = net[j]; net[j] = net[j - 1]; net[j - 1] = t; int l = len[j]; len[j] = len[j - 1]; len[j - 1] = l; }
    return n;
}
static void print_collapsed(const char *const *routes, int count) {
    u32 net[8]; int len[8];
    for (int i = 0; i < count; i++) { char buf[32]; int p; sscanf(routes[i], "%31[^/]/%d", buf, &p); net[i] = ip(buf); len[i] = p; }
    int n = collapse(net, len, count);
    for (int i = 0; i < n; i++) printf("%s%s", i ? ", " : "", cidr(net[i], len[i]));
}

int main(void) {
    /* 1. VLSM: allocate the largest requirement first, each from the next free aligned block */
    u32 base = ip("192.168.50.0"), next_free = base;
    Need needs[] = {{"Servers", 100}, {"Office", 50}, {"Guests", 25}, {"Management", 10}, {"Router link", 2}};
    qsort(needs, 5, sizeof needs[0], by_hosts_desc);
    printf("1. VLSM plan for %s\n", cidr(base, 24));
    for (int i = 0; i < 5; i++) {
        int bits = 0; while ((1u << bits) < (unsigned)needs[i].hosts + 2) bits++;     /* host bits so that 2^bits - 2 >= hosts */
        u32 size = 1u << bits;
        next_free = (next_free + size - 1) / size * size;                              /* align to the block size */
        printf("   %-12s needs %3d hosts -> %-18s usable %3u  range %s - %s\n", needs[i].name, needs[i].hosts, cidr(next_free, 32 - bits), (unsigned)(size - 2), dot(next_free + 1), dot(next_free + size - 2));
        next_free += size;
    }
    printf("   next free address: %s\n", dot(next_free));

    /* 2. Route summarisation */
    const char *good[] = {"10.1.0.0/24", "10.1.1.0/24", "10.1.2.0/24", "10.1.3.0/24"};
    const char *bad[] = {"10.1.0.0/24", "10.1.1.0/24", "10.1.2.0/24", "10.1.4.0/24"};
    printf("\n2. summary of 10.1.0.0/24, 10.1.1.0/24, 10.1.2.0/24, 10.1.3.0/24 -> "); print_collapsed(good, 4); putchar('\n');
    printf("   with 10.1.4.0/24 instead of 10.1.3.0/24 -> "); print_collapsed(bad, 4); printf(" (no single covering route without including 10.1.3.0)\n");

    /* 3. Longest-prefix match */
    struct { const char *prefix; int len; const char *via; } table[] = {{"0.0.0.0", 0, "ISP"}, {"10.0.0.0", 8, "core"}, {"10.1.0.0", 16, "site-A"}, {"10.1.2.0", 24, "lab"}, {"10.1.2.128", 25, "lab-servers"}};
    printf("\n3. routing table:");
    for (int i = 0; i < 5; i++) printf(" %s -> %s%s", cidr(ip(table[i].prefix), table[i].len), table[i].via, i < 4 ? "," : "\n");
    const char *dests[] = {"10.1.2.200", "10.1.2.7", "10.1.9.9", "10.9.9.9", "8.8.8.8"};
    for (int d = 0; d < 5; d++) {
        u32 a = ip(dests[d]); int best = -1;
        printf("   %-11s matches ", dests[d]);
        for (int i = 0; i < 5; i++) if ((a & mask_of(table[i].len)) == ip(table[i].prefix)) { printf("%s%s", best >= 0 ? ", " : "", cidr(ip(table[i].prefix), table[i].len)); if (best < 0 || table[i].len > table[best].len) best = i; }
        printf("  -> best %s -> %s\n", cidr(ip(table[best].prefix), table[best].len), table[best].via);
    }

    /* 4. Fragmentation: a 4000-byte IP payload over MTU 1500 (20-byte header) */
    int mtu = 1500, hdr = 20, payload = 4000, per = (mtu - hdr) / 8 * 8;                /* data per fragment must be a multiple of 8 bytes */
    printf("\n4. fragmenting %d bytes of payload, MTU %d, header %d: data per fragment %d\n", payload, mtu, hdr, per);
    for (int off = 0, n = 1; off < payload; off += per, n++) {
        int size = payload - off < per ? payload - off : per;
        printf("   fragment %d: total length %d, offset field %3d (byte %d), MF=%d\n", n, size + hdr, off / 8, off, off + size < payload);
    }
    return 0;
}
