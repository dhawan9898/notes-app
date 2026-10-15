---
title: Networking Exercises
track: networking
module: Networking Essentials
moduleOrder: 0.5
order: 1
level: beginner
minutes: 35
env: browser
terminal:
  - ./net_exercises
summary: "Eight graded exercises: binary addresses, network and broadcast addresses, same-network tests, sizing subnets for host counts, splitting a block, classifying special addresses, summarising routes, and a hands-on lab, with answers checked by a C program that runs in the in-browser Linux."
prerequisites:
  - "What Is a Network?"
  - Binary, Hexadecimal and Bits
  - "IP Addresses, Subnets and Ports in Plain Language"
sources:
  - label: "RFC 4632 — Classless Inter-domain Routing (CIDR)"
    url: https://www.rfc-editor.org/rfc/rfc4632
  - label: "RFC 1918 — Address Allocation for Private Internets"
    url: https://www.rfc-editor.org/rfc/rfc1918
---

Subnetting is a skill: it becomes fast only through practice. Work each exercise on paper first (use the "block size = 256 − mask octet" shortcut from *IPv4 Addressing, Subnetting, VLSM and CIDR*), then check yourself against the answers at the end.

## Exercises

**1. Binary.** Write `192.168.10.77` as four 8-bit binary groups.

**2. Network, mask and size.** For `172.20.5.9/20`: the network address, the subnet mask in dotted form, and the number of usable host addresses.

**3. Range.** For `10.20.77.130/21`: network address, broadcast address, and the first and last usable host.

**4. Same network?** Are these pairs on the same network (so they could talk without a router)?
- `192.168.1.10/24` and `192.168.1.200/24`
- `10.0.0.5/25` and `10.0.0.130/25`
- `172.16.4.1/22` and `172.16.7.250/22`

**5. Sizing.** What is the smallest prefix (largest subnet mask) that gives room for 25 hosts? For 100 hosts? For 500? Remember the network and broadcast addresses.

**6. Splitting.** Split `192.168.5.0/24` into four equal subnets. Write each.

**7. Classify.** Say whether each address is private, link-local, shared (carrier-grade NAT) or public: `10.1.2.3`, `172.32.0.1`, `192.168.255.1`, `169.254.7.7`, `100.64.0.9`, `8.8.8.8`.

**8. Summarise.** Can `10.1.0.0/24`, `10.1.1.0/24`, `10.1.2.0/24` and `10.1.3.0/24` be advertised as one route? Which?

**Lab (hands-on).** Extend the virtual LAN from *What Is a Network?*: add a third namespace `pc3` with address `192.168.1.30/24`, connected to `pc2` by a *second* veth pair (so `pc3` is on a different cable from `pc1`). Predict whether `pc1` can ping `192.168.1.30`, then try it and explain the result. Then build the fix: connect all three hosts to one Linux bridge (a virtual switch; see *Linux Network Labs*) and ping again. (This last part needs namespaces and a bridge, which the in-browser Linux does not have: do it on your own Linux machine or virtual machine.)

## Answers

<details>
<summary>Show answers and the checking program</summary>

1. `192.168.10.77` = `11000000.10101000.00001010.01001101`.
2. `/20` mask is `255.255.240.0`; the third octet block size is 16, and 5 rounds down to 0 → network **172.20.0.0/20**; hosts = 2^12 − 2 = **4094**.
3. `/21`: block size 8 in the third octet; 77 → 72, so network **10.20.72.0**, broadcast **10.20.79.255**, hosts **10.20.72.1 – 10.20.79.254**.
4. Yes (both in `192.168.1.0/24`); **No** (`10.0.0.0/25` and `10.0.0.128/25` are different networks: they need a router); Yes (both in `172.16.4.0/22`, which covers 4.0 to 7.255).
5. 25 hosts + 2 = 27 addresses → the next power of two is 32 = 2^5 → **/27** (30 usable); 100 hosts → 128 → **/25** (126 usable); 500 hosts → 512 → **/23** (510 usable).
6. Four equal parts of a /24 are /26 blocks of 64 addresses: `192.168.5.0/26`, `.64/26`, `.128/26`, `.192/26`.
7. `10.1.2.3` private; `172.32.0.1` **public** (the private range is 172.16.0.0 to 172.31.255.255 only); `192.168.255.1` private; `169.254.7.7` link-local (a host that failed to get an address from DHCP); `100.64.0.9` shared address space (CGNAT); `8.8.8.8` public.
8. Yes: four consecutive /24s starting on a multiple of 4 (`10.1.0.0`) are exactly **10.1.0.0/22**.
Lab: `pc1` believes `192.168.1.30` is on its own LAN (same /24), so it sends an ARP request out of its only cable. `pc2` is not a switch and does not forward frames between its two cables, so nobody answers: the ping fails (100 % packet loss, typically reported as *Destination Host Unreachable*). A bridge connecting all three interfaces forwards frames between them, so all three hosts become one LAN and the ARP request reaches `pc3`.

The C program that produced and checked these answers. Run it with `./net_exercises` in the Linux terminal (the program is already installed there), or compile it yourself with `cc -o net_exercises net_exercises.c`:

```c
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
```

```text
$ ./net_exercises
Ex 1: 192.168.10.77 in binary: 11000000.10101000.00001010.01001101
Ex 2: 172.20.5.9/20 -> 172.20.0.0/20 | mask 255.255.240.0 | usable hosts 4094
Ex 3: 10.20.77.130/21 -> network 10.20.72.0 broadcast 10.20.79.255 range 10.20.72.1 - 10.20.79.254
Ex 4: 192.168.1.10/24 and 192.168.1.200/24: same network? yes (192.168.1.0/24 vs 192.168.1.0/24)
Ex 4: 10.0.0.5/25 and 10.0.0.130/25: same network? no (10.0.0.0/25 vs 10.0.0.128/25)
Ex 4: 172.16.4.1/22 and 172.16.7.250/22: same network? yes (172.16.4.0/22 vs 172.16.4.0/22)
Ex 5: 25 hosts need a /27 (30 usable addresses)
Ex 5: 100 hosts need a /25 (126 usable addresses)
Ex 5: 500 hosts need a /23 (510 usable addresses)
Ex 6: split 192.168.5.0/24 into four equal subnets: 192.168.5.0/26, 192.168.5.64/26, 192.168.5.128/26, 192.168.5.192/26
Ex 7: 10.1.2.3        private (RFC 1918)
Ex 7: 172.32.0.1      public
Ex 7: 192.168.255.1   private (RFC 1918)
Ex 7: 169.254.7.7     link-local (DHCP failed?)
Ex 7: 100.64.0.9      shared/CGNAT space
Ex 7: 8.8.8.8         public
Ex 8: aggregate 10.1.0.0/24 + 10.1.1.0/24 + 10.1.2.0/24 + 10.1.3.0/24 -> 10.1.0.0/22
```

</details>

## Key takeaways

1. Subnetting is binary arithmetic with a mask; the block-size shortcut gives the answer in seconds.
2. Same network means same prefix bits; different networks need a router.
3. Size a subnet for hosts + 2, rounded up to a power of two.
4. Know the special ranges so you can recognise them in a capture or log.

**Next:** *ARP and IPv4 Address Conflict Detection*.
