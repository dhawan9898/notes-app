---
title: IPv6 Header, Extension Headers and Addressing
track: networking
module: IPv6
moduleOrder: 4
order: 1
level: intermediate
summary: The simplified fixed header, how extension headers chain, every address type and scope, interface identifiers, multicast formation and sensible prefix planning.
prerequisites:
  - IPv4 Header, Fragmentation and MTU
  - IPv4 Addressing, Subnetting, VLSM and CIDR
sources:
  - label: "RFC 8200 — Internet Protocol, Version 6 (IPv6) Specification (STD 86)"
    url: https://www.rfc-editor.org/rfc/rfc8200
  - label: "RFC 4291 — IP Version 6 Addressing Architecture"
    url: https://www.rfc-editor.org/rfc/rfc4291
  - label: "RFC 5952 — A Recommendation for IPv6 Address Text Representation"
    url: https://www.rfc-editor.org/rfc/rfc5952
  - label: "RFC 4193 — Unique Local IPv6 Unicast Addresses"
    url: https://www.rfc-editor.org/rfc/rfc4193
  - label: "RFC 7217 / RFC 8981 — Stable opaque interface identifiers / Temporary address extensions"
    url: https://www.rfc-editor.org/rfc/rfc8981
  - label: "RFC 6724 — Default Address Selection for IPv6"
    url: https://www.rfc-editor.org/rfc/rfc6724
  - label: "RFC 6177 / RFC 7421 — IPv6 address assignment to end sites; the /64 boundary"
    url: https://www.rfc-editor.org/rfc/rfc7421
  - label: "RFC 9288 — Filtering of IPv6 packets containing extension headers at transit routers"
    url: https://www.rfc-editor.org/rfc/rfc9288
---

## The IPv6 header

IPv6 simplifies the base header to a **fixed 40 bytes**, optimizing for fast hardware forwarding.

```text
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-------+---------------+-----------------------------------------+
|Version| Traffic Class |              Flow Label (20)            |
+-------+---------------+---------------+-------------+-----------+
|         Payload Length (16)           |  Next Header | Hop Limit |
+---------------------------------------+--------------+-----------+
|                                                                  |
+                       Source Address (128)                       +
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
+                     Destination Address (128)                    +
|                                                                  |
+------------------------------------------------------------------+
```

Compared with IPv4:

| IPv4 | IPv6 |
|---|---|
| IHL, options | removed: fixed 40 B; options moved to **extension headers** |
| Identification, Flags, Fragment Offset | moved to the optional **Fragment header** |
| Header checksum | **removed** (link CRC and transport checksums suffice) |
| Total Length | **Payload Length** (excludes the 40 B header; 0 with a jumbogram option, RFC 2675) |
| TTL | **Hop Limit** |
| Protocol | **Next Header** |
| ToS | **Traffic Class** (DSCP 6 + ECN 2) |
| (none) | **Flow Label** (20 bits): identifies a flow; use for ECMP/ LAG hashing even when the payload is encrypted (RFC 6437) |

Fragmentation is performed only by the **source host**, never by routers; routers that cannot forward return ICMPv6 **Packet Too Big** (type 2). The minimum link MTU is **1280 bytes**. IPv6 also changes upper layers: UDP checksum is mandatory, and the pseudo-header for TCP/UDP/ICMPv6 includes the 128-bit addresses.

## Extension headers

Optional information is carried in headers chained by the Next Header field: base header → ext. header → ext. header → upper layer (TCP/UDP/ICMPv6).

| Next Header | Header | Notes |
|---|---|---|
| 0 | **Hop-by-Hop Options** | examined by *every* router on the path; must be first. Used by Router Alert (MLD, RSVP), jumbograms. Often slow-pathed, therefore frequently dropped |
| 43 | **Routing** | type 2 (mobile IPv6), type 3 RPL, type 4 **Segment Routing (SRv6)**; type 0 is deprecated (RFC 5095, amplification attack) |
| 44 | **Fragment** | 8 bytes: offset (13 bits, 8-byte units), M flag, 32-bit Identification |
| 60 | **Destination Options** | processed only by the destination (and by listed intermediates if before a routing header) |
| 51 / 50 | AH / ESP | IPsec |
| 135 | Mobility | |
| 59 | **No Next Header** | nothing follows |
| 6 / 17 / 58 | TCP / UDP / ICMPv6 | upper-layer protocols |

Recommended order (RFC 8200): Hop-by-Hop, Destination Options (for routing-header intermediates), Routing, Fragment, AH, ESP, Destination Options, upper layer. Nodes process headers in the order they appear, and only the Hop-by-Hop header is processed by intermediate nodes (with exceptions). Each option has a type with two high bits telling what to do if unrecognized (skip, discard, discard + ICMP).

Operational consequences:

- A firewall or ACL must **walk the chain** to find the upper-layer protocol; hardware with limited parsing depth may fail or drop packets with long chains. RFC 7112 requires that the entire chain be in the first fragment.
- Many networks drop extension-header traffic (esp. HBH, and fragments): measurements (RFC 7872) found that a large fraction of Internet paths drop packets carrying HBH options or fragment headers. RFC 9288 gives guidance for transit filtering.
- **Fragments** have the L4 header only in the first, as in IPv4, so ECMP and ACLs behave the same. Prefer PMTUD; avoid fragmentation in DNS (EDNS buffer 1232).

## Address notation

128 bits as eight 16-bit hex groups: `2001:0db8:0000:0000:0000:ff00:0042:8329`. Rules (RFC 5952 canonical form): drop leading zeros in each group; compress the **longest** run of zero groups (≥ 2 groups; leftmost if tie) with `::` (only once); lowercase hex → `2001:db8::ff00:42:8329`. Prefix notation `2001:db8:1234::/48`. In URLs use brackets: `http://[2001:db8::1]:8080/`. Link-local needs a **zone identifier**: `fe80::1%eth0`.

## Address types and scopes

| Type | Prefix | Scope / use |
|---|---|---|
| Unspecified | `::/128` | "no address yet" (DAD source); never assigned |
| Loopback | `::1/128` | host-local |
| **Link-local unicast** | `fe80::/10` (in practice `fe80::/64`) | on-link only; **required on every interface**; used by NDP, routing protocol adjacencies and next hops |
| **Unique local (ULA)** | `fc00::/7` (`fd00::/8` in use) | private, site-scope: `fd` + random 40-bit global ID + 16-bit subnet (RFC 4193); not routed on the Internet; no NAT needed |
| **Global unicast** | `2000::/3` | Internet-routable; today allocated from `2000::/3` blocks (e.g. `2001::/16`, `2400::/12`...) |
| Multicast | `ff00::/8` | see below; replaces broadcast |
| Documentation | `2001:db8::/32` | RFC 3849; use in examples |
| IPv4-mapped | `::ffff:0:0/96` | `::ffff:192.0.2.1`: a socket API representation of IPv4 (dual-stack sockets), never on the wire |
| NAT64 | `64:ff9b::/96` | well-known prefix (RFC 6052) |
| 6to4 / Teredo | `2002::/16`, `2001::/32` | legacy transition, deprecated |
| Discard-only | `100::/64` | remote-triggered black hole (RFC 6666) |

There is **no broadcast** in IPv6. **Anycast** addresses are taken from the unicast space and are configured explicitly (e.g. subnet-router anycast: the prefix with zero host bits; used also for DNS/CDN anycast).

### Interface identifiers (the lower 64 bits)

Standard unicast addresses (except those starting `000`) use a **64-bit interface identifier (IID)**; a subnet prefix is therefore /64 for SLAAC (RFC 7421 explains why /64 is architecturally embedded). Ways to build an IID:

| Method | Description | Privacy |
|---|---|---|
| **Modified EUI-64** | MAC `00:1a:2b:3c:4d:5e` → insert `ff:fe` in the middle, flip the U/L bit → `021a:2bff:fe3c:4d5e` | exposes the MAC, trackable across networks |
| **Stable opaque (RFC 7217)** | hash(prefix, interface, secret key, DAD counter) | stable per prefix, different on every network |
| **Temporary (RFC 8981)** | random, regenerated (default valid 2 days, preferred 1 day) | used for outgoing connections |
| Manual / DHCPv6 | `::1`, `::53` etc. | for servers |

Linux defaults: `addr_gen_mode=stable-privacy` (RFC 7217) when configured, and `use_tempaddr=2` for temporary addresses on desktops.

### Multicast addresses

```text
 8        4     4          112
+--------+-----+-----+----------------+
|11111111|flags|scope|    group ID    |
+--------+-----+-----+----------------+
```

Flags (`0RPT`): T = 1 transient (not well-known), P = 1 derived from a unicast prefix (RFC 3306), R = 1 embedded rendezvous point (RFC 3956). Scope: `1` interface-local, **`2` link-local**, `4` admin-local, `5` site-local, `8` organization-local, **`e` global**.

| Address | Group |
|---|---|
| `ff02::1` | all nodes (link) |
| `ff02::2` | all routers |
| `ff02::5` / `ff02::6` | OSPFv3 routers / DRs |
| `ff02::9` | RIPng |
| `ff02::d` | PIM routers |
| `ff02::16` | MLDv2-capable routers |
| `ff02::1:2` | DHCPv6 relays and servers |
| `ff02::fb` | mDNS |
| `ff02::1:ffXX:XXXX` | **solicited-node**: `ff02::1:ff` + last 24 bits of a unicast address; the target of Neighbor Solicitation |

Ethernet multicast MAC = `33:33` + the last 32 bits of the IPv6 group, so a solicited-node address maps to `33:33:ff:XX:XX:XX`: only hosts whose last 24 bits match (almost always just one) receive and interrupt on the NS, which is why NDP is far more efficient than ARP's broadcast.

## Planning prefixes

- An ISP typically delegates a **/48** (RFC 6177 recommendation) or /56 or /60 (residential) to a site. A /48 gives 16 bits of subnet ID = 65,536 /64 networks.
- **Always /64 for every LAN/VLAN**, even for tiny segments: SLAAC, NDP, privacy extensions and many features assume it. Don't try to conserve addresses.
- Point-to-point links: **/127** (RFC 6164; avoids ping-pong on /126) or use only link-local addresses with a loopback for management.
- Loopbacks /128. Allocate on **nibble boundaries** (/48, /52, /56, /60, /64) so that hex digits map to hierarchy, which also eases summarization and ACLs: e.g. `2001:db8:SSSS:VVVV::/64` with SSSS = site, VVVV = VLAN.
- Use **ULA** for stable internal-only addressing alongside GUA (not as a replacement for firewalling), and avoid IPv6 NAT (NPTv6, RFC 6296, is a stateless prefix translation if you must).

## Source address selection (RFC 6724)

A host with multiple addresses (link-local, ULA, GUA, temporary) picks source and destination by rules: prefer the same scope, avoid deprecated addresses, prefer matching label, prefer **temporary** for outbound (when enabled), prefer longest common prefix. Browsers also use **Happy Eyeballs** (RFC 8305), racing IPv6 and IPv4 with a small head start for IPv6. Policy tables can be adjusted (`/etc/gai.conf`).

```bash
ip -6 addr show dev eth0           # flags: tentative, dadfailed, deprecated, temporary, mngtmpaddr
ip -6 route show
ping -6 -I eth0 fe80::1            # or ping fe80::1%eth0
sysctl net.ipv6.conf.eth0.{accept_ra,use_tempaddr,addr_gen_mode}
```

## Key takeaways

- Fixed 40-byte header, no checksum, no router fragmentation, minimum MTU 1280; options live in a chain of extension headers.
- Every interface has a link-local address; multicast replaces broadcast; solicited-node multicast replaces ARP broadcasts.
- Use /64 for LANs, /127 for p2p, /128 for loopbacks, /48 per site, and plan on nibble boundaries.
- IIDs: prefer stable-opaque and temporary addresses over EUI-64 for hosts.
- Extension headers (especially HBH and fragments) are often filtered: design for the base header plus ICMPv6 PTB.
