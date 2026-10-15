---
title: VLSM, Summarisation, Longest-Prefix Match and Fragmentation
track: networking
module: IPv4
moduleOrder: 3
order: 1
level: intermediate
minutes: 40
env: browser
terminal:
  - ./ipv4_practice
summary: "Four exercises on the IPv4 module: a VLSM addressing plan, summarising routes (and when it fails), choosing routes by longest-prefix match, and fragmenting a datagram by hand, with a C program that computes every answer and a check of the reasoning."
prerequisites:
  - "IPv4 Header, Fragmentation and MTU"
  - "IPv4 Addressing, Subnetting, VLSM and CIDR"
  - "Forwarding: Routing Table, Longest-Prefix Match, Policy Routing and ECMP"
sources:
  - label: "RFC 4632 — Classless Inter-domain Routing (CIDR): the Internet Address Assignment and Aggregation Plan"
    url: https://www.rfc-editor.org/rfc/rfc4632
  - label: "RFC 791 — Internet Protocol (fragmentation and reassembly)"
    url: https://www.rfc-editor.org/rfc/rfc791
  - label: "RFC 1812 — Requirements for IP Version 4 Routers (longest-prefix match)"
    url: https://www.rfc-editor.org/rfc/rfc1812
---

Subnetting skill is the foundation of every network design. Do each exercise on paper, then run the program at the end to check yourself. The simpler "same network?" and "network and broadcast address" drills are in *Practice: Networking Exercises*; these four are the ones that appear in real design work.

## Exercises

**1. A VLSM plan.** Divide `192.168.50.0/24` into five subnets for: Servers (100 hosts), Office (50), Guests (25), Management (10) and a router-to-router link (2). Allocate the largest first, aligned on block boundaries, and wasting as little as possible. What is the first address still free afterwards?

**2. Summarise.** The four routes `10.1.0.0/24`, `10.1.1.0/24`, `10.1.2.0/24` and `10.1.3.0/24` should be advertised as one. Give the summary. Then replace the last route by `10.1.4.0/24`: can they still be summarised into one route without including addresses you do not own?

**3. Longest-prefix match.** The routing table holds `0.0.0.0/0` (ISP), `10.0.0.0/8` (core), `10.1.0.0/16` (site A), `10.1.2.0/24` (lab) and `10.1.2.128/25` (lab servers). Which route does a router choose for `10.1.2.200`, `10.1.2.7`, `10.1.9.9`, `10.9.9.9` and `8.8.8.8`, and why is "the most specific route wins" the rule?

**4. Fragmentation.** A host sends 4000 bytes of IP payload over a link with MTU 1500 (header 20 bytes, no options). How many fragments are there, what is each fragment's total length, offset field and More-Fragments flag? Why must every fragment except the last carry a multiple of 8 bytes of data?

## Answers

Try before opening.

<details>
<summary>Show the program and its output</summary>

```c
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
```

```text
$ ./ipv4_practice
1. VLSM plan for 192.168.50.0/24
   Servers      needs 100 hosts -> 192.168.50.0/25    usable 126  range 192.168.50.1 - 192.168.50.126
   Office       needs  50 hosts -> 192.168.50.128/26  usable  62  range 192.168.50.129 - 192.168.50.190
   Guests       needs  25 hosts -> 192.168.50.192/27  usable  30  range 192.168.50.193 - 192.168.50.222
   Management   needs  10 hosts -> 192.168.50.224/28  usable  14  range 192.168.50.225 - 192.168.50.238
   Router link  needs   2 hosts -> 192.168.50.240/30  usable   2  range 192.168.50.241 - 192.168.50.242
   next free address: 192.168.50.244

2. summary of 10.1.0.0/24, 10.1.1.0/24, 10.1.2.0/24, 10.1.3.0/24 -> 10.1.0.0/22
   with 10.1.4.0/24 instead of 10.1.3.0/24 -> 10.1.0.0/23, 10.1.2.0/24, 10.1.4.0/24 (no single covering route without including 10.1.3.0)

3. routing table: 0.0.0.0/0 -> ISP, 10.0.0.0/8 -> core, 10.1.0.0/16 -> site-A, 10.1.2.0/24 -> lab, 10.1.2.128/25 -> lab-servers
   10.1.2.200  matches 0.0.0.0/0, 10.0.0.0/8, 10.1.0.0/16, 10.1.2.0/24, 10.1.2.128/25  -> best 10.1.2.128/25 -> lab-servers
   10.1.2.7    matches 0.0.0.0/0, 10.0.0.0/8, 10.1.0.0/16, 10.1.2.0/24  -> best 10.1.2.0/24 -> lab
   10.1.9.9    matches 0.0.0.0/0, 10.0.0.0/8, 10.1.0.0/16  -> best 10.1.0.0/16 -> site-A
   10.9.9.9    matches 0.0.0.0/0, 10.0.0.0/8  -> best 10.0.0.0/8 -> core
   8.8.8.8     matches 0.0.0.0/0  -> best 0.0.0.0/0 -> ISP

4. fragmenting 4000 bytes of payload, MTU 1500, header 20: data per fragment 1480
   fragment 1: total length 1500, offset field   0 (byte 0), MF=1
   fragment 2: total length 1500, offset field 185 (byte 1480), MF=1
   fragment 3: total length 1060, offset field 370 (byte 2960), MF=0
```

**1. VLSM.** For each requirement, find the smallest block with at least *hosts + 2* addresses (`2^bits − 2 ≥ hosts`): 100 hosts need `/25` (126 usable), 50 need `/26` (62), 25 need `/27` (30), 10 need `/28` (14) and 2 need `/30` (2). Allocate **largest first**: each block must start at a multiple of its own size, and placing big blocks first keeps everything aligned with no gaps, which is why the plan packs perfectly into the first 244 addresses of the /24. The next free address is `192.168.50.244`, leaving room for a later `/30`, `/29` or `/28`. Plan growth when you choose sizes: a `/27` for 25 guests leaves only 5 spare addresses. Giving each subnet its own VLAN and gateway address (the first usable address by convention) completes the design.

**2. Summarisation.** Four consecutive `/24`s starting on a multiple of four (`10.1.0.0` to `10.1.3.0`) collapse exactly into `10.1.0.0/22`: the first 22 bits are identical and the last two bits of the third octet enumerate the four networks. Summarisation shrinks routing tables and hides instability (a flapping `/24` inside the summary no longer causes updates elsewhere). With `10.1.4.0/24` in place of `10.1.3.0/24` the best collapse is three routes (`10.1.0.0/23`, `10.1.2.0/24`, `10.1.4.0/24`): the smallest single covering prefix would be `10.1.0.0/21`, which also covers `10.1.3.0/24`, `10.1.5.0/24` and so on that you may not own, and **advertising addresses you do not own attracts traffic you cannot deliver (a black hole)**. Good address plans allocate blocks that summarise cleanly.

**3. Longest-prefix match.** Every route whose prefix contains the destination *matches*; the router uses the **longest** (most specific) one:

- `10.1.2.200` matches five routes; the longest is `10.1.2.128/25`, so it goes to **lab-servers**.
- `10.1.2.7` is not in `10.1.2.128/25` (that covers .128 to .255), so the `/24` **lab** route wins.
- `10.1.9.9` leaves the lab, so `10.1.0.0/16` (**site A**) wins; `10.9.9.9` only matches the `/8` (**core**); `8.8.8.8` falls through to the default route (**ISP**).

Specific-beats-general lets you carve exceptions out of larger blocks and keeps the default route as a catch-all. The rule is why a *more specific* route announced by an attacker or by mistake pulls traffic away (route hijacks), and why `/32` host routes are powerful. Ties between equal prefixes are broken by administrative distance and metric, not by prefix length (*Forwarding*).

**4. Fragmentation.** A fragment can carry at most `MTU − header = 1480` bytes of data, and the **fragment offset field counts in units of 8 bytes** (it is only 13 bits wide), so every fragment except the last must carry a multiple of 8 bytes: 1480 is exactly 185 × 8. The 4000 bytes become three fragments: 1480 + 1480 + 1040 bytes of data, with total lengths 1500, 1500 and 1060, offset fields 0, 185 and 370, and More-Fragments set on the first two only. The receiver reassembles by (source, destination, protocol, identification) and the offsets, and discards everything if any fragment is lost, so one lost fragment wastes the whole datagram. This is why modern stacks avoid fragmentation with **Path MTU Discovery** (set the Don't Fragment bit, and shrink on ICMP "fragmentation needed") and why blocking ICMP breaks connections that cross smaller-MTU links (see *IPv4 Header, Fragmentation and MTU*).

</details>

## Key takeaways

1. VLSM: size each subnet for `hosts + 2`, allocate the largest first and align every block on its own size.
2. Contiguous, aligned blocks summarise exactly into one shorter prefix; summarising non-contiguous blocks advertises addresses you do not own.
3. Routers choose the longest matching prefix; the default route `0.0.0.0/0` matches everything and loses to anything more specific.
4. Fragment data is a multiple of 8 bytes (offset units); losing one fragment loses the whole datagram, so prefer Path MTU Discovery.
