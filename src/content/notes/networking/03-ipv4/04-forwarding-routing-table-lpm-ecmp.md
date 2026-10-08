---
title: "Forwarding: Routing Table, Longest-Prefix Match, Policy Routing and ECMP"
track: networking
module: IPv4
moduleOrder: 3
order: 4
level: advanced
summary: How a router turns routes into forwarding decisions — RIB vs FIB, route selection, recursive resolution, Linux routing tables and rules, ECMP hashing and failure behavior.
sources:
  - label: "RFC 1812 — Requirements for IP Version 4 Routers (§5.2 Forwarding)"
    url: https://www.rfc-editor.org/rfc/rfc1812
  - label: "RFC 2992 — Analysis of an Equal-Cost Multi-Path Algorithm"
    url: https://www.rfc-editor.org/rfc/rfc2992
  - label: "RFC 3704 — Ingress Filtering for Multihomed Networks (uRPF)"
    url: https://www.rfc-editor.org/rfc/rfc3704
  - label: "Linux — ip-route(8), ip-rule(8), Documentation/networking/ip-sysctl.rst (fib_multipath_*)"
    url: https://docs.kernel.org/networking/ip-sysctl.html
  - label: "Nilsson & Karlsson — IP-address lookup using LC-tries (IEEE JSAC 1999)"
---

## Control plane vs. data plane tables

A router separates *learning routes* from *forwarding packets*:

| Structure | Role |
|---|---|
| **Per-protocol tables** | each routing protocol's own database (OSPF LSDB, BGP Adj-RIB-In) |
| **RIB** (Routing Information Base) | the best route per prefix, chosen across protocols |
| **FIB** (Forwarding Information Base) | the optimized, **fully resolved** copy used for lookups: prefix → next-hop(s) + egress interface + L2 rewrite info |
| **Adjacency table** | next-hop IP → MAC + interface (from ARP/NDP) |

Hardware routers program the FIB into ASIC memory (TCAM or algorithmic LPM). Linux uses a single table in the kernel (an LC-trie), which *is* the FIB, managed with `ip route`.

## Route selection: three tie-breaks, in order

For a packet destination, the forwarding decision is:

1. **Longest prefix match (LPM)**: of all FIB entries whose prefix contains the destination, take the one with the **longest mask**. `10.1.2.0/24` beats `10.1.0.0/16` beats `0.0.0.0/0`. This is *always* first and independent of protocols and metrics.
2. If several **sources** offer the *same* prefix, the RIB picks by **administrative distance** (a trust ranking): connected 0, static 1, eBGP 20, EIGRP 90, OSPF 110, IS-IS 115, RIP 120, iBGP 200 (Cisco defaults; Juniper "preference" differs, Linux "metric" and protocol-specific proto IDs).
3. Among the same protocol, lowest **metric**; if equal, install all of them (ECMP).

Consequences: a more-specific prefix from a *less trusted* source still wins over a less-specific one (e.g. a /32 injected by a rogue router beats the legitimate /24: the foundation of route-hijack attacks); a **floating static route** is a static route with a higher AD so it activates only when the dynamic route disappears.

### Special route types

| Type | Meaning |
|---|---|
| **Connected** | the prefix of an up interface |
| **Local** (host route, /32) | the interface's own address: packets are delivered to the router itself |
| **Static** | manual: via next-hop, via interface, or to `Null0`/`blackhole` |
| **Default** (`0.0.0.0/0`) | "gateway of last resort" |
| **Discard / unreachable / prohibit** | silently drop / drop with ICMP unreachable / drop with ICMP admin-prohibited |

### Recursive resolution

A route's next hop is usually an IP address that must itself be resolved:

```text
B 203.0.113.0/24 via 10.9.9.9              (BGP route, next hop is a loopback of the peer)
O 10.9.9.9/32   via 192.0.2.2, eth1        (IGP route to that next hop)
C 192.0.2.0/30  directly connected, eth1
```

The FIB entry for 203.0.113.0/24 resolves to `192.0.2.2 via eth1`. When the IGP changes, the recursive routes follow without BGP touching every prefix, the reason large routers use **hierarchical FIBs / PIC (prefix independent convergence)**. Pitfall: **recursive loop** or a next hop learned from the very prefix it must reach.

## Linux routing in practice

```bash
ip route show                             # main table
ip route get 203.0.113.5 from 10.0.0.3    # simulate lookup: shows chosen route, source, mtu
ip route add 198.51.100.0/24 via 10.0.0.1 dev eth0 metric 100
ip route add blackhole 192.0.2.0/24
ip route add 10.5.0.0/16 via 10.0.0.9 onlink          # next hop not in a connected subnet
ip -6 route show; ip route show table all             # all tables
```

Linux has multiple tables identified by number (names in `/etc/iproute2/rt_tables`): **local** (255; local and broadcast addresses, maintained by the kernel), **main** (254), **default** (253). **Policy routing** adds the *which table?* step before the lookup:

```bash
ip rule show
# 0:      from all lookup local
# 32766:  from all lookup main
# 32767:  from all lookup default
ip rule add from 10.0.1.0/24 table 100 priority 1000     # source-based routing
ip route add default via 192.0.2.1 table 100
ip rule add fwmark 0x1 table 200                         # decide by firewall mark (nft/iptables can set it)
```

Typical uses: multihoming with two ISPs (reply out the interface the request came in), VPN split routing, and per-tenant **VRF** (`ip link add vrf-red type vrf table 10`; VRF is the cleaner way to get separate routing instances). Rules are evaluated in priority order; the first table that yields a *matching route* wins (an unreachable/prohibit route stops the search).

### Source address selection

For locally originated packets the kernel picks the source address from the route (`src` attribute), the outgoing interface's primary address in the destination's subnet (scope rules), or `RFC 6724` ordering for IPv6. Surprises on multihomed hosts are common: `ip route get` shows the answer. Always set `src` on routes for loopback/VIP sourcing.

### Reverse-path filtering

`rp_filter=1` (strict) drops packets arriving on an interface that is not the route back to their source (RFC 3704). `2` (loose) only needs *some* route. Strict mode breaks asymmetric routing and policy routing; use loose (2) or disable it where needed, with a firewall providing spoof protection.

```bash
sysctl net.ipv4.ip_forward=1                  # act as a router
sysctl net.ipv4.conf.all.rp_filter=2
```

## ECMP: equal-cost multipath

When several next hops have equal cost, the router splits flows across them. Requirements and details:

- **Per-flow hashing** (not per-packet) keeps packets of one flow in order. Hash inputs: source/destination IP, protocol, and L4 ports (and for tunnels the inner header or flow label).
- The hash selects among `N` next hops (modulo or hash-threshold; RFC 2992). **When N changes** (one next hop fails), plain modulo reshuffles *most* flows; hash-threshold and "resilient hashing" / consistent hashing keep unaffected flows on their paths (essential with stateful devices or anycast TCP).
- Linux: `net.ipv4.fib_multipath_hash_policy` = 0 (L3: src/dst IP), 1 (L4 five-tuple), 2 (inner L3 for encapsulated traffic), 3 (custom fields). Newer kernels use **nexthop objects** (`ip nexthop add id 1 via 10.0.0.1 dev eth0`, groups with `ip nexthop add id 10 group 1/2` and `type resilient`) for large ECMP sets and BGP route sharing.
- Weighted ECMP (UCMP) assigns unequal weights, e.g. proportional to link bandwidth (`weight` on nexthops).
- A dead next-hop: Linux needs link-down detection (`ignore_routes_with_linkdown=1`) or a protocol/BFD to remove it, otherwise it keeps hashing packets to a black hole until neighbor state expires.
- Elephant flows hash to one path regardless of others' load; consider flowlet switching, adaptive routing, or multiple connections (multipath TCP, QUIC with connection IDs).

## Longest-prefix match algorithms (briefly)

- **Binary trie / Patricia / LC-trie** (Linux): lookup time O(W) in address bits W, with level compression to make it ≈ log-ish; fast but memory accesses are serialized.
- **TCAM**: parallel search of all prefixes in one cycle by storing them ordered by length; expensive and power-hungry, limited size (typically hundreds of thousands to a few million entries depending on platform; the IPv4 Internet table has more than 900,000 prefixes, so older platforms periodically hit their FIB limit).
- **DIR-24-8** (DPDK `rte_lpm`): a 2^24 table for /24 and shorter plus extension tables for longer prefixes; one or two memory reads per lookup.
- **Algorithmic LPM** in ASICs: compressed multibit tries with SRAM/HBM.

## Packet paths

- **Fast path**: hardware or kernel FIB lookup, no CPU interaction per packet.
- **Slow path / punt**: packets needing special handling go to the CPU: for-me traffic, TTL expiry, IP options, ARP resolution (glean adjacency: first packet triggers ARP), ICMP generation, fragmentation (on many platforms). Rate-limit this path (CoPP, `net.ipv4.icmp_ratelimit`).

## Troubleshooting flow

1. `ip route get <dst>`: which route, interface, source, next hop?
2. `ip neigh show <next hop>`: is it resolved (REACHABLE/STALE) or FAILED/INCOMPLETE?
3. `tcpdump -eni <if> host <dst>`: does the packet leave with the right MAC? Does it return?
4. Check `ip rule` and VRFs if the result surprises you; `sysctl net.ipv4.ip_forward`; `nft list ruleset` for drops.
5. For hardware platforms compare the RIB with the FIB (`show ip cef`, `show route forwarding-table`): a route can be in the RIB but not installed in the FIB (table full).

## Key takeaways

- Forwarding = longest prefix match first; then AD, then metric, then ECMP. Specifics always win.
- The FIB holds resolved next hops; recursive routes let BGP prefixes follow IGP changes.
- Linux uses multiple tables + `ip rule` for policy routing; VRFs give full separation.
- ECMP is per-flow hashing; know which fields feed the hash and how the set changes when a path fails.
- `rp_filter` strict mode breaks asymmetric paths.
