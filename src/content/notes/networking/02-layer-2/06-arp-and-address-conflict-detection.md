---
title: ARP and IPv4 Address Conflict Detection
track: networking
module: Layer 2
moduleOrder: 2
order: 6
level: intermediate
summary: The ARP packet, the Linux neighbor state machine, gratuitous ARP, proxy ARP, conflict detection (RFC 5227) and the knobs that matter on multihomed hosts.
prerequisites:
  - Ethernet Frames and MAC Learning
sources:
  - label: "RFC 826 — An Ethernet Address Resolution Protocol"
    url: https://www.rfc-editor.org/rfc/rfc826
  - label: "RFC 5227 — IPv4 Address Conflict Detection"
    url: https://www.rfc-editor.org/rfc/rfc5227
  - label: "RFC 1027 — Using ARP to Implement Transparent Subnet Gateways (Proxy ARP)"
    url: https://www.rfc-editor.org/rfc/rfc1027
  - label: "RFC 1122 §2.3.2 — ARP requirements for hosts"
    url: https://www.rfc-editor.org/rfc/rfc1122
  - label: "Linux kernel — Documentation/networking/ip-sysctl.rst (arp_*), net/core/neighbour.c"
    url: https://docs.kernel.org/networking/ip-sysctl.html
---

ARP binds an IPv4 address to a link-layer address on a single broadcast domain. It is not routed, has no authentication, and is carried directly in Ethernet (EtherType `0x0806`). IPv6 replaces it with Neighbor Discovery (see the IPv6 module).

## Packet format (28 bytes for Ethernet/IPv4)

| Field | Size | Value for Ethernet + IPv4 |
|---|---|---|
| Hardware type (HTYPE) | 2 | 1 (Ethernet) |
| Protocol type (PTYPE) | 2 | `0x0800` (IPv4) |
| Hardware length (HLEN) | 1 | 6 |
| Protocol length (PLEN) | 1 | 4 |
| Operation (OPER) | 2 | 1 request, 2 reply (3/4 RARP, 8/9 InARP) |
| Sender hardware address (SHA) | 6 | |
| Sender protocol address (SPA) | 4 | |
| Target hardware address (THA) | 6 | `00:00:00:00:00:00` in a request |
| Target protocol address (TPA) | 4 | the IP being resolved |

The Ethernet frame is padded to 60 bytes (+4 FCS) because the ARP payload is only 28 bytes.

### Resolution exchange

1. Host A wants the MAC for 10.0.0.2 (the next hop). It broadcasts `who-has 10.0.0.2 tell 10.0.0.1` (Ethernet dst `ff:ff:ff:ff:ff:ff`, THA zeros).
2. Every host receives it. A host owning the TPA replies **unicast** to A's MAC with `10.0.0.2 is-at bb:bb`.
3. A caches the mapping. B can also learn A's mapping from the request itself (RFC 826 specifies that a receiver *updates* an existing entry for the sender and adds one if it's the target), so the reverse resolution usually needs no extra packet.

## The neighbor cache on Linux

The kernel keeps ARP (and NDP) entries in the generic **neighbour** table with a small state machine:

| State | Meaning |
|---|---|
| INCOMPLETE | request sent, no reply yet; outgoing packets are queued (a few) |
| REACHABLE | confirmed recently (by ARP reply or upper-layer confirmation like TCP ACKs) |
| STALE | not confirmed within `base_reachable_time` (~30 s, randomized 0.5×–1.5×); still usable, validated on next use |
| DELAY | used while STALE: wait up to 5 s for confirmation by upper layers before probing |
| PROBE | unicast ARP requests being sent to re-verify |
| FAILED | no answer after `ucast_solicit`/`mcast_solicit` tries → "Destination Host Unreachable" |
| PERMANENT / NOARP | static entry / no resolution needed |

```bash
ip neigh show                          # IP, device, lladdr, state
ip neigh add 10.0.0.9 lladdr 02:00:00:00:00:09 dev eth0 nud permanent
ip neigh flush dev eth0
sysctl net.ipv4.neigh.default.gc_thresh{1,2,3}   # table size limits: 128/512/1024 by default
```

The **gc thresholds** matter on routers and big flat L2 domains: if the table overflows you get `neighbour: arp_cache: neighbor table overflow!` in `dmesg`, and intermittent connectivity.

## Gratuitous ARP and probes

A **gratuitous ARP (GARP)** has sender IP = target IP: "10.0.0.1 is-at aa:aa", sent as request or reply, normally broadcast. Uses:

- **Failover**: after a VIP moves (VRRP, keepalived, cluster failover, VM live migration) the new owner sends GARP so that switches update their FDB and neighbors update their caches immediately instead of waiting for the timeouts.
- **Announce** a new or changed address.

Because receivers update caches from them, GARP is also the lever for **ARP spoofing**.

### RFC 5227 address conflict detection (ACD)

Before using a new address a host should check for duplicates:

1. **Probe**: send 3 ARP requests (spaced 1–2 s apart, after a random 0–1 s initial delay) with **SPA = 0.0.0.0**, THA zeros, TPA = candidate. The zero sender prevents poisoning other caches.
2. If any ARP packet arrives with SPA = candidate (a reply) or a probe for the same address from another host, there is a **conflict**: do not use it.
3. **Announce**: send 2 ARP announcements (sender = target = candidate) 2 s apart.
4. **Ongoing defense**: if a conflicting ARP appears later, defend once (send an announcement) and, if it continues within 10 s, back off.

Linux `arping -D -I eth0 10.0.0.5` performs duplicate address detection; `dhclient` and NetworkManager use it before accepting a lease. Windows reports "IP address conflict" from the same mechanism.

## Proxy ARP

A device answers ARP requests **on behalf of** another address (RFC 1027). A router with proxy ARP answers for any destination it can route to, so hosts on different physical segments believe the destinations are local. Uses today: legacy subnet splitting without mask change; **VPN and Wi-Fi bridging**; hosts behind a firewall in transparent mode; **EVPN/VXLAN fabrics** (ARP suppression, where the leaf answers from its table to avoid broadcast flooding).

Risks: hosts have no default gateway (they ARP for everything), ARP tables grow large, and the router becomes a black hole for typos. On Linux: `net.ipv4.conf.<if>.proxy_arp=1` (answer for routable destinations) and `proxy_arp_pvlan` for PVLAN.

## Multihomed host tuning

By default Linux answers an ARP request for **any local address on any interface** ("ARP flux"). On hosts with several NICs in the same subnet, or with VIPs on loopback (e.g. DSR load balancing), this makes a MAC answer for the wrong IP. Control with:

| sysctl | Value | Effect |
|---|---|---|
| `arp_ignore` | 0 | reply for any local IP on any interface (default) |
| | 1 | reply only if the TPA is configured on the receiving interface |
| | 2 | as 1, and the sender must be in the same subnet as the target |
| `arp_announce` | 0 | use any local address as SPA |
| | 1 | prefer an address in the target's subnet |
| | 2 | always use the best local address for the outgoing interface |
| `arp_filter` | 1 | answer only if the route to the sender would use that interface |

Direct-server-return load balancers use `arp_ignore=1`, `arp_announce=2` on real servers that hold the VIP on `lo`, so only the balancer answers for the VIP.

## Security: ARP spoofing and defenses

ARP is unauthenticated: any host can send a reply or GARP claiming someone else's IP, redirecting traffic through itself (man-in-the-middle), or blackholing it.

- **Dynamic ARP Inspection (DAI)** on the switch validates ARP packets against a trusted binding table (built by DHCP snooping or static ARP ACLs) and drops mismatches. See the L2 security note.
- **Static ARP entries** for critical gateways in small environments.
- **Private VLANs / per-port isolation** limit who can hear a spoof.
- **802.1X + MACsec** authenticates hosts and cryptographically protects L2 frames.
- Detection: `arpwatch`, flapping `IP → MAC` in Wireshark (`arp.duplicate-address-detected`), switch MAC-move logs.

## Troubleshooting

```bash
tcpdump -eni eth0 arp                   # see who-has / is-at
arping -I eth0 10.0.0.2                 # unicast-style probing; shows the responding MAC
ip -s neigh                             # per-entry counters and state transitions
```

| Symptom | Likely cause |
|---|---|
| `INCOMPLETE`/`FAILED` for the gateway | wrong VLAN, gateway down, port security, filtering |
| Two different MACs answering for one IP | duplicate IP or proxy ARP/ spoofing |
| Works for a while then stops after failover | switch/host still has old MAC → missing GARP |
| Intermittent loss on a large flat network | neighbor table overflow or ARP storm (broadcast) |

## Key takeaways

- ARP resolves the **next hop's** MAC; requests are broadcast, replies unicast, and caches age through a STALE/PROBE cycle.
- Gratuitous ARP drives fast failover; RFC 5227 probes (SPA = 0) detect duplicate IPs safely.
- Proxy ARP is useful for fabrics but dangerous as a default; tune `arp_ignore`/`arp_announce` for multihomed hosts and DSR.
- ARP has no authentication, so rely on DAI, 802.1X, and monitoring.
