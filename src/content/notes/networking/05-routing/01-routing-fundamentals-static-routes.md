---
title: "Routing Fundamentals: Static Routes, Protocol Families and Convergence"
track: networking
module: Routing
moduleOrder: 5
order: 1
level: intermediate
summary: How routes get into a router, the taxonomy of routing protocols, static route variants and tracking, administrative distance, and what convergence really means.
prerequisites:
  - "Forwarding: Routing Table, Longest-Prefix Match, Policy Routing and ECMP"
sources:
  - label: "RFC 1812 — Requirements for IP Version 4 Routers (§5.2.4 Routing table, §7 Routing protocols)"
    url: https://www.rfc-editor.org/rfc/rfc1812
  - label: "RFC 1136 / RFC 1930 — Administrative domains; Guidelines for creation of an AS"
    url: https://www.rfc-editor.org/rfc/rfc1930
  - label: "RFC 5880 — Bidirectional Forwarding Detection (for static route tracking)"
    url: https://www.rfc-editor.org/rfc/rfc5880
  - label: "FRRouting documentation — Static Route Commands"
    url: https://docs.frrouting.org/en/latest/static.html
  - label: "Doyle & Carroll — Routing TCP/IP, Vol. 1 (Cisco Press)"
---

## Where routes come from

A router's RIB gets entries from four sources:

1. **Connected**: automatically from configured, up interfaces.
2. **Static**: manually configured by the operator.
3. **Dynamic routing protocols**: learned from neighbors (OSPF, IS-IS, BGP, RIP, EIGRP).
4. **Other**: DHCP-installed defaults, VPN/ tunnel routes, redistributed routes, controllers (SDN/PCEP).

Each source has a different level of trust, expressed by **administrative distance (AD)** (Cisco/Linux-FRR term; "route preference" on Junos):

| Source | AD (Cisco/FRR default) |
|---|---|
| Connected | 0 |
| Static | 1 |
| eBGP | 20 |
| EIGRP internal | 90 |
| OSPF | 110 |
| IS-IS | 115 |
| RIP | 120 |
| EIGRP external | 170 |
| iBGP | 200 |
| Unknown/unusable | 255 |

AD is a **local** concept (not carried in any protocol) used only to choose between routes to the *same prefix* learned from different sources. Metrics are only comparable within one protocol.

## Static routing

A static route says "to reach prefix P, use next hop N / interface I".

| Form | Example (FRR) | Notes |
|---|---|---|
| Next-hop IP | `ip route 10.2.0.0/16 192.0.2.2` | recursive: next hop must be reachable; the usual choice on multi-access networks |
| Interface only | `ip route 10.2.0.0/16 eth1` | router ARPs for the *destination* (proxy ARP on the far side); avoid on Ethernet |
| Next hop + interface | `ip route 10.2.0.0/16 192.0.2.2 eth1` | "fully specified": unambiguous and required for link-local IPv6 next hops |
| Null / blackhole | `ip route 10.0.0.0/8 blackhole` | discard; used for summaries (loop prevention), DDoS RTBH |
| Default | `ip route 0.0.0.0/0 192.0.2.1` | gateway of last resort |

Linux equivalents: `ip route add 10.2.0.0/16 via 192.0.2.2 dev eth1`.

### Floating static routes

A static route with a **higher AD** than the dynamic protocol serves as a backup: it stays in the configuration but is not installed while the dynamic route exists.

```text
! primary learned via OSPF (AD 110); backup via LTE/secondary link (AD 200)
ip route 10.2.0.0/16 198.51.100.1 200
```

When OSPF withdraws the prefix, the static route enters the RIB. Caveat: if the primary path is up but black-holed (a failure beyond the next hop), a static route stays installed because the next hop is still ARP-reachable. Use **route tracking** to remove it when a probe fails: IP SLA / BFD for static routes (`ip route ... bfd`), `track` objects in Cisco, `ip route ... bfd` in FRR's `bfdd`, or `ip-monitor`/`keepalived` scripts on Linux.

### When static routes are the right tool

- Stub networks (single exit), default routes toward ISPs, management networks, and anchoring routes redistributed into a protocol.
- Small networks with stable topology; **data-center host routing** (host routes to VIPs).
- When security demands deterministic routes with no protocol attack surface.

They scale badly: every topology change requires manual edits, and they cannot detect failures beyond the local link.

## Classifying dynamic routing protocols

| Axis | Categories |
|---|---|
| **Scope** | **IGP** (interior, within an AS): OSPF, IS-IS, RIP, EIGRP. **EGP** (exterior, between ASes): BGP |
| **Algorithm** | **Distance vector** (RIP, EIGRP-ish) · **Link-state** (OSPF, IS-IS) · **Path vector** (BGP) |
| **Addressing** | classful (RIPv1, obsolete) vs classless (all modern) |
| **Convergence** | fast (link-state with BFD, subsecond) vs slow (RIP, BGP timers) |

What differs between them:

- **Distance vector**: each router tells neighbors its *routing table* (prefix + distance). It sees only neighbors' views ("routing by rumor").
- **Link-state**: every router floods its *links* to the whole area, builds an identical topology database, and runs **Dijkstra** (SPF) to compute shortest paths. Everyone has a complete map.
- **Path vector**: routes carry the entire list of ASes traversed (AS_PATH), allowing loop detection and policy decisions.

An **Autonomous System (AS)** is a set of networks under a single administrative routing policy, identified by an ASN (16/32-bit). Inside, an IGP optimizes paths by metric. Between ASes, BGP implements business policy, not shortest path.

## Convergence

Convergence is the time between a topology change and all routers having consistent, correct forwarding state. Total time ≈:

`failure detection + propagation/flooding + computation + FIB update + (hardware programming)`

| Component | Typical values |
|---|---|
| Detection | link-down: ms (interface carrier); routing hello timeouts: seconds (OSPF dead 40 s); **BFD**: 50 ms × 3 = ~150 ms |
| Propagation | LSA flooding in ms, BGP updates paced by MRAI (up to 30 s on eBGP by default) |
| Computation | SPF: ms to hundreds of ms (throttled to avoid churn) |
| FIB programming | tens of ms to seconds for large tables |

Techniques to speed up: BFD (fast detection), tuning protocol timers, **LFA/TI-LFA** and MPLS FRR (pre-computed backups giving < 50 ms), BGP PIC, ECMP.  Too fast has a cost: **flap dampening/ throttling** protects the control plane from instability.

**Loops and black holes** during convergence: before all routers agree, packets can loop (TTL protects) or be dropped (micro-loops). Distance vector is prone to **count-to-infinity**; link-state to short-lived micro-loops; BGP to *path exploration* (visiting longer and longer paths before settling).

## Choosing a design

- Campus/DC underlay: OSPF or IS-IS (link-state, fast, scalable to hundreds/thousands), or eBGP (RFC 7938) for large fabrics.
- Service provider core: IS-IS (or OSPF) + iBGP; MPLS/SR.
- Customer/Internet edge: eBGP; static/default for stubs.
- Avoid RIP in anything new; EIGRP is Cisco-centric (RFC 7868 is informational).

## FRR example: a router with an IGP and a floating static

```text
frr# configure terminal
frr(config)# router ospf
frr(config-router)# ospf router-id 10.255.0.1
frr(config-router)# network 10.0.12.0/30 area 0
frr(config-router)# exit
frr(config)# ip route 10.2.0.0/16 198.51.100.1 200
frr(config)# end
frr# show ip route
```

## Key takeaways

- Routes enter the RIB from connected, static, dynamic and other sources; AD chooses between sources for the same prefix, metrics within a protocol.
- Static routes are simple but blind; floating statics need tracking/BFD to fail over properly.
- IGP vs EGP is scope; distance vector, link-state and path vector are the algorithms.
- Convergence = detection + propagation + computation + programming; BFD and pre-computed backups are the biggest levers.
