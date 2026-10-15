---
title: Building and Parsing a Packet by Hand
track: networking
module: Fundamentals
moduleOrder: 1
order: 1
level: intermediate
minutes: 45
env: browser
terminal:
  - ip link set lo up
  - ./packet
summary: "A hands-on encapsulation exercise: build an Ethernet frame carrying IPv4 and UDP in C with correct lengths and checksums, parse it back, capture the real frame the kernel sends for the same datagram, and compare the two byte by byte, with the full program and its output."
prerequisites:
  - "Network Models, Encapsulation and Device Roles"
  - "A Packet's Journey: Host to Gateway to Host"
  - "Reading RFCs and IEEE Standards Efficiently"
sources:
  - label: "RFC 791 — Internet Protocol (header format)"
    url: https://www.rfc-editor.org/rfc/rfc791
  - label: "RFC 768 — User Datagram Protocol (header and pseudo-header)"
    url: https://www.rfc-editor.org/rfc/rfc768
  - label: "RFC 1071 — Computing the Internet Checksum"
    url: https://www.rfc-editor.org/rfc/rfc1071
  - label: "IEEE 802.3 — Ethernet frame format (overview)"
    url: https://standards.ieee.org/ieee/802.3/7071/
---

"Encapsulation" is easy to say and easy to forget. Building a frame from the RFC diagrams, byte by byte, and then comparing it with what the kernel actually sends is the most reliable way to make the layers concrete. The C program below needs only the standard socket API. Part 1 builds the frame and parses it; part 2 captures the kernel's real frame for the same datagram on the loopback interface with a raw `AF_PACKET` socket, which needs root (or `CAP_NET_RAW`). In the in-browser Linux you are root, so just run `ip link set lo up` and then `./packet`; on your own machine compile it with `cc -o packet packet.c` and run it with `sudo ./packet` (loopback is already up there).

```c
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
```

```text
$ ip link set lo up
$ ./packet
1. a frame built by hand: 47 bytes = 14 Ethernet + 20 IPv4 + 8 UDP + 5 payload
   hex: 00 00 00 00 00 00 00 00 00 00 00 00 08 00 45 00 00 21 12 34 ...
   eth_dst        00:00:00:00:00:00
   eth_src        00:00:00:00:00:00
   ethertype      0x0800
   ip_total       33
   ttl            64
   proto          17
   ip_checksum_ok yes
   src            127.0.0.1:40000
   dst            127.0.0.1:9999
   udp_len        13
   payload        "hello"

2. captured the kernel's own frame: 47 bytes; bytes that differ from mine: 6 at offsets [18, 19, 24, 25, 40, 41]
   IP identification: mine 1234, kernel 5ce8; IP checksum: mine 2a96, kernel dfe1
   UDP checksum: mine faaf, kernel fe20
   the kernel's frame parses to the same fields:
   eth_dst        00:00:00:00:00:00
   eth_src        00:00:00:00:00:00
   ethertype      0x0800
   ip_total       33
   ttl            64
   proto          17
   ip_checksum_ok yes
   src            127.0.0.1:40000
   dst            127.0.0.1:9999
   udp_len        13
   payload        "hello"
```

## Exercises

**1. Count the layers.** The frame is 47 bytes. Break it into headers and payload, and say which layer each belongs to and what addressing each uses.

**2. Read the fields.** In the hand-built hex, locate the EtherType, the IP version/length byte, the TTL, the protocol number and the two ports. What do the values `0800`, `45`, `40`, `11` mean?

**3. The checksum.** How is the Internet checksum computed, and how does a receiver verify it? Why does the UDP checksum include a *pseudo-header* with the IP addresses?

**4. Compare with the kernel.** Six bytes differed between the hand-built frame and the captured one. Which fields, and why? What does that tell you about which parts of a packet are fixed by the standard and which are a sender's choice?

## Answers

Try before opening.

<details>
<summary>Show the explanations</summary>

**1.** 47 = **14** (Ethernet header) + **20** (IPv4 header, no options) + **8** (UDP header) + **5** (payload `hello`).

| Bytes | Layer | Contains | Addressing |
|---|---|---|---|
| 0-13 | Layer 2, Ethernet | destination MAC (6), source MAC (6), EtherType (2) | MAC addresses: the next hop on this link |
| 14-33 | Layer 3, IPv4 | version/header length, TOS, total length, identification, flags/fragment, TTL, protocol, checksum, source and destination address | IP addresses: end to end |
| 34-41 | Layer 4, UDP | source port, destination port, length, checksum | ports: which program |
| 42-46 | application | `hello` | none |

(An Ethernet frame also has a 4-byte frame check sequence at the end that the network card adds and strips; software never sees it, so it is not in the capture.) Each layer treats everything inside as opaque payload: Ethernet does not look at IP, and IP does not look at UDP; that separation is the whole point of the model in *Network Models, Encapsulation and Device Roles*. Here both MACs are all zeros because the loopback device has no hardware address.

**2.** In the hex the Ethernet header ends with **`08 00`**: the **EtherType** `0x0800` means "the payload is IPv4" (`0x86dd` would be IPv6, `0x0806` ARP). The next byte **`45`** packs two 4-bit fields: version **4** and header length **5** words of 4 bytes = 20 bytes. After the TOS byte, the total length (`00 21` = 33 = 20 + 8 + 5) and the identification (`12 34`), the flags/fragment field `40 00` (the **Don't Fragment** bit set), then the TTL **`40`** = 64 hops, and the protocol **`11`** = 17 = **UDP** (6 would be TCP, 1 ICMP). The UDP header carries the ports 40000 (`9c 40`) and 9999 (`27 0f`) and its own length 13 = 8 + 5. The parser prints the same values, and `ip_checksum_ok yes` shows the header verifies.

**3.** The **Internet checksum** (RFC 1071) is the **ones' complement of the ones' complement sum of all 16-bit words**: add the words, fold any carry out of bit 16 back into the low bits, then invert. The sender computes it with the checksum field set to zero and stores the result. A receiver **sums everything including the checksum field**; a correct, undamaged header gives all ones, whose complement is `0`, which is what `checksum(header) == 0` tests. It catches most random bit errors but is weak (it cannot detect reordered words, for instance), so higher layers and link-layer CRCs add stronger checks, and TLS provides real integrity. The IPv4 checksum covers **only the header** (which is why routers must recompute it after decrementing the TTL). UDP's checksum covers the **payload plus a pseudo-header** made of the source and destination IP addresses, the protocol number and the UDP length: those fields are not in the UDP header itself, but a datagram delivered to the *wrong host* (because of a corrupted IP address) or in the wrong protocol must be detected, so they are included in the sum. In IPv4 the UDP checksum is optional (a zero value means "not computed"); in IPv6 it is mandatory, since IPv6 has no header checksum of its own. Our program turns a computed `0` into `0xFFFF` to avoid ambiguity, as RFC 768 requires.

**4.** The two frames differ in six bytes, in three fields: the **IP identification** (the kernel picks its own value, different on every run, against my chosen `1234`; the standard only says it should help reassembly of fragments, so each sender picks its own values), the **IP header checksum** (mine is `2a96`, the kernel's differs: it changes whenever any header byte changes, so it differs *because* the identification differs, and both versions verify correctly) and the **UDP checksum** (`fe20` against my `faaf`): on a modern system the kernel leaves checksum calculation to the network card (**checksum offload**) and stores only the pseudo-header sum (`fe20`: the sum of the address, protocol and length words) for the hardware to complete; on the loopback interface no card ever does, and the receiving stack knows to accept it. Everything else is identical: addresses, ports, TTL 64, the DF bit and the length fields. So most of a packet is **dictated by the protocol** (field positions, lengths, valid values, how the checksum is computed), while a few fields are the **sender's choice** (identification, TTL, TOS/DSCP, and options), and the checksum is **derived** from the others. Whenever you read an RFC for a new protocol, this is the split to look for: *what is fixed, what is chosen, and what is computed*. That matches the advice in *Reading RFCs and IEEE Standards Efficiently*, and it is also how packet generators and firewalls' parsers are written.

</details>

## Key takeaways

1. A frame is nested headers: Ethernet (MAC addresses, EtherType), IPv4 (addresses, TTL, protocol number, header checksum), UDP (ports, length, checksum), then the payload; each layer treats the next as opaque data.
2. The Internet checksum is the ones' complement of the ones' complement sum of 16-bit words; verifying means summing everything and getting zero.
3. The UDP checksum includes a pseudo-header with the IP addresses so a misdelivered datagram is detected; IPv4 header checksums are recomputed at every router because the TTL changes.
4. In any protocol some fields are fixed by the standard, some chosen by the sender, and some computed from the rest; building a packet by hand and comparing it with a capture is the fastest way to learn the format.
