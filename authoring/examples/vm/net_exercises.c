/* Answers to the Networking Exercises lab: IPv4 address arithmetic with shifts and masks. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef uint32_t u32;

static u32 ip(const char *s) { unsigned a, b, c, d; sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d); return a << 24 | b << 16 | c << 8 | d; }
static const char *dot(u32 x) {
    static char buf[8][16]; static int i;
    char *p = buf[i++ & 7];
    snprintf(p, 16, "%u.%u.%u.%u", (unsigned)(x >> 24), (unsigned)(x >> 16 & 255), (unsigned)(x >> 8 & 255), (unsigned)(x & 255));
    return p;
}
static u32 mask_of(int prefix) { return prefix ? 0xFFFFFFFFu << (32 - prefix) : 0; }          /* a shift by 32 would be undefined */
static u32 network(u32 addr, int prefix) { return addr & mask_of(prefix); }
static u32 broadcast(u32 addr, int prefix) { return addr | ~mask_of(prefix); }
static const char *cidr(u32 addr, int prefix) { static char buf[4][20]; static int i; char *p = buf[i++ & 3]; snprintf(p, 20, "%s/%d", dot(addr), prefix); return p; }
static int in_net(u32 addr, const char *net, int prefix) { return network(addr, prefix) == ip(net); }

/* merge neighbouring blocks of equal size into their parent until nothing changes (route aggregation) */
static int aggregate(u32 *net, int *plen, int n) {
    for (int changed = 1; changed;) {
        changed = 0;
        for (int i = 0; i < n && !changed; i++)
            for (int j = 0; j < n && !changed; j++)
                if (i != j && plen[i] == plen[j] && plen[i] > 0 && (net[i] ^ net[j]) == 1u << (32 - plen[i]) && network(net[i], plen[i] - 1) == network(net[j], plen[i] - 1)) {
                    net[i] = network(net[i], plen[i] - 1); plen[i]--;
                    net[j] = net[n - 1]; plen[j] = plen[n - 1]; n--; changed = 1;
                }
    }
    return n;
}

int main(void) {
    printf("Ex 1: 192.168.10.77 in binary: ");
    u32 a = ip("192.168.10.77");
    for (int s = 24; s >= 0; s -= 8) { for (int bit = 7; bit >= 0; bit--) putchar('0' + (int)(a >> (s + bit) & 1)); if (s) putchar('.'); }
    putchar('\n');

    u32 x = ip("172.20.5.9");
    printf("Ex 2: 172.20.5.9/20 -> %s | mask %s | usable hosts %u\n", cidr(network(x, 20), 20), dot(mask_of(20)), (unsigned)((1u << (32 - 20)) - 2));

    x = ip("10.20.77.130");
    printf("Ex 3: 10.20.77.130/21 -> network %s broadcast %s range %s - %s\n", dot(network(x, 21)), dot(broadcast(x, 21)), dot(network(x, 21) + 1), dot(broadcast(x, 21) - 1));

    const char *pairs[][2] = {{"192.168.1.10", "192.168.1.200"}, {"10.0.0.5", "10.0.0.130"}, {"172.16.4.1", "172.16.7.250"}};
    const int plen[] = {24, 25, 22};
    for (int i = 0; i < 3; i++) {
        u32 na = network(ip(pairs[i][0]), plen[i]), nb = network(ip(pairs[i][1]), plen[i]);
        printf("Ex 4: %s/%d and %s/%d: same network? %s (%s vs %s)\n", pairs[i][0], plen[i], pairs[i][1], plen[i], na == nb ? "yes" : "no", cidr(na, plen[i]), cidr(nb, plen[i]));
    }

    const int hosts[] = {25, 100, 500};                    /* smallest block with room for the hosts plus the network and broadcast addresses */
    for (int i = 0; i < 3; i++) {
        int bits = 0; while ((1u << bits) < (unsigned)hosts[i] + 2) bits++;
        printf("Ex 5: %d hosts need a /%d (%u usable addresses)\n", hosts[i], 32 - bits, (unsigned)((1u << bits) - 2));
    }

    printf("Ex 6: split 192.168.5.0/24 into four equal subnets:");
    for (int i = 0; i < 4; i++) printf("%s%s", i ? ", " : " ", cidr(ip("192.168.5.0") + ((u32)i << 6), 26));
    putchar('\n');

    const char *probe[] = {"10.1.2.3", "172.32.0.1", "192.168.255.1", "169.254.7.7", "100.64.0.9", "8.8.8.8"};
    for (int i = 0; i < 6; i++) {
        u32 v = ip(probe[i]);
        const char *kind = in_net(v, "10.0.0.0", 8) || in_net(v, "172.16.0.0", 12) || in_net(v, "192.168.0.0", 16) ? "private (RFC 1918)"
                         : in_net(v, "169.254.0.0", 16) ? "link-local (DHCP failed?)" : in_net(v, "100.64.0.0", 10) ? "shared/CGNAT space" : "public";
        printf("Ex 7: %-15s %s\n", probe[i], kind);
    }

    u32 nets[4]; int lens[4];
    for (int i = 0; i < 4; i++) { nets[i] = ip("10.1.0.0") + ((u32)i << 8); lens[i] = 24; }
    int n = aggregate(nets, lens, 4);
    printf("Ex 8: aggregate 10.1.0.0/24 + 10.1.1.0/24 + 10.1.2.0/24 + 10.1.3.0/24 ->");
    for (int i = 0; i < n; i++) printf(" %s", cidr(nets[i], lens[i]));
    putchar('\n');
    return 0;
}
