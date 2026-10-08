---
title: Route Redistribution, Filtering and Routing Policy
track: networking
module: Routing
moduleOrder: 5
order: 8
level: advanced
summary: Moving routes between protocols safely — metrics, tags, loops with two-point redistribution, route-maps and prefix-lists, and how BGP and IGPs should (and should not) be combined.
sources:
  - label: "RFC 1812 §5.2.4 — Routing table; RFC 2328 §16.4 — External route calculation"
    url: https://www.rfc-editor.org/rfc/rfc2328
  - label: "RFC 1403 / RFC 4271 §9 — BGP/OSPF interaction; BGP decision and route injection"
    url: https://www.rfc-editor.org/rfc/rfc4271
  - label: "RFC 3101 — NSSA (external routes inside OSPF)"
    url: https://www.rfc-editor.org/rfc/rfc3101
  - label: "RFC 8212 — Default eBGP behavior without policy"
    url: https://www.rfc-editor.org/rfc/rfc8212
  - label: "FRRouting documentation — Route Maps, Prefix Lists"
    url: https://docs.frrouting.org/en/latest/routemap.html
---

Real networks run several routing protocols or routing domains (OSPF + BGP, IGP + static, legacy + new, merged companies). **Redistribution** imports routes from one source into another protocol, and **routing policy** decides which routes are accepted, advertised and preferred.

## What redistribution really does

Redistribution takes routes **in the RIB** that were installed by protocol A and originates them into protocol B. Important consequences:

1. It operates on the best routes in the RIB (after AD), not on protocol A's database.
2. **Metrics are not comparable** between protocols: you must supply a **seed metric** (a default or explicit value) because OSPF cost, RIP hops, and BGP MED mean different things.
3. Information is lost: protocol B does not learn A's topology, only prefixes (so no loop protection from A's own mechanisms).
4. The redistributing router becomes the "entry point" for all those prefixes; capacity and failure of that router matter.

| Into | Default behavior | Notes |
|---|---|---|
| OSPF | external type 5 LSAs, metric type **E2**, metric 20 (Cisco), tag 0; subnets must be explicitly allowed on older IOS | the redistributing router becomes an **ASBR**; E1/E2 choice; `redistribute ... metric-type 1 route-map` |
| IS-IS | external TLV 130/135 | into L2 (or L1 with leaking) |
| RIP | needs a metric (hop count) ≤ 15; otherwise unreachable (16) | |
| BGP | ORIGIN incomplete, MED = IGP metric | careful with scale |
| EIGRP | requires composite metric (bandwidth, delay, reliability, load, MTU) | |

Connected and static routes are commonly redistributed into the IGP to advertise them (`redistribute connected`, `redistribute static`), or in the reverse direction, IGP into BGP (rarely recommended).

## The classic failure: loops and suboptimal routing

### Mutual redistribution at two points

R1 and R2 both redistribute between OSPF and RIP. A prefix P originates in the RIP domain. R1 redistributes P into OSPF as an external route. R2 now holds **two** routes for P: the original RIP route (AD 120) and the OSPF external copy (AD 110). R2 prefers the OSPF copy, which is just R1's redistribution of the very same prefix, so it forwards P's traffic toward R1 through OSPF instead of directly into RIP. If R2 also redistributes OSPF into RIP, it advertises P *back* into the RIP domain with a possibly better metric, and routers there may switch to the path through R2 and R1: a routing loop or a persistent suboptimal path.

The remedy has three layers:

1. **Tag** routes when they are redistributed (e.g. tag 100 = "from RIP"), and **deny** routes carrying the tag when redistributing back.
2. **Filter** with prefix-lists so that each router only redistributes prefixes belonging to the source domain.
3. **Adjust AD** (`distance`) so that native routes always win over redistributed copies.

### Suboptimal paths

Metric mismatch means redistributed routes can take a longer path or exit at a distant ASBR. Use E1 metrics (OSPF) to include internal cost, or adjust seed metrics per ASBR so that the nearest exit wins.

## Route maps

A **route-map** is an ordered list of clauses `permit`/`deny <seq>`; each clause has `match` conditions and `set` actions. Evaluation:

- The first clause (lowest sequence) whose **all match conditions** are true is applied: `permit` → accept and apply `set`; `deny` → reject.
- Different match **types** in one clause are ANDed; multiple values in one match line are ORed.
- A clause without `match` matches everything.
- At the end there is an **implicit deny** (if no clause matches the route is rejected, the classic trap when adding a route-map for a "small tweak" and silently dropping everything else).

```text
ip prefix-list RIP-ROUTES seq 5 permit 192.168.0.0/16 le 24
!
route-map RIP-TO-OSPF permit 10
 match ip address prefix-list RIP-ROUTES
 set metric-type type-1
 set metric 100
 set tag 100
route-map RIP-TO-OSPF deny 99
!
route-map OSPF-TO-RIP deny 10
 match tag 100                          ! never send back what came from RIP
route-map OSPF-TO-RIP permit 20
 set metric 3
!
router ospf
 redistribute rip route-map RIP-TO-OSPF
router rip
 redistribute ospf route-map OSPF-TO-RIP
```

### Prefix lists

Match prefix and length: `permit 10.0.0.0/8 ge 16 le 24` matches prefixes inside 10/8 whose mask length is between /16 and /24. Without `ge/le`, exact match only. Typical pitfalls: `0.0.0.0/0 le 32` matches **everything**; `0.0.0.0/0` alone matches only the default route. Always list explicit `deny` at the end for clarity; the implicit deny exists anyway.

### Other filters

- **Distribute lists** (older, with ACLs) and **inbound filtering** in distance-vector protocols.
- **AS-path access lists** (regular expressions: `^$` = locally originated, `^65001_` = from neighbor 65001, `_65001$` = originated by 65001).
- **Community lists** for tags.
- **OSPF**: `area range ... not-advertise`, filter-lists at ABRs, NSSA no-redistribution.

## BGP and the IGP

| Direction | Practice |
|---|---|
| **IGP → BGP** (advertise internal prefixes to the Internet) | prefer `network` statements or controlled `redistribute` with a route-map; aggregate; never redistribute without filters (private ranges, transit links leak) |
| **BGP → IGP** (inject external routes into OSPF/IS-IS) | **do not redistribute the Internet table into the IGP**: it overwhelms the IGP and leaks policy. Inject only a **default route** or a small set of necessary prefixes. Use iBGP everywhere for transit instead |
| Loopback/next hop reachability | carry in the IGP (iBGP next hops must be reachable there) |
| Customer edge | static routes or BGP to the customer; redistribute static into BGP with a community |

Also: BGP route advertisement of a default route (`neighbor ... default-originate`) vs conditional advertisement (advertise-map/non-exist-map) for failover.

## Policy-based routing vs. route policy

- **Route policy** (route-maps, prefix filters) controls what the *control plane* learns and advertises.
- **Policy-based routing (PBR)** overrides the forwarding decision per packet by matching ACL/DSCP/source and setting next hop or VRF (`ip rule`, `ip route ... table`, `route-map ... set ip next-hop`). Powerful but easy to misconfigure: PBR is applied before the RIB lookup, ignores reachability unless tracked, and is often hardware-limited. Use sparingly with tracked next hops.

## Linux/FRR notes

```text
# FRR: tag-based loop prevention
route-map STATIC-TO-OSPF permit 10
 match ip address prefix-list MY-STATICS
 set metric-type type-2
 set tag 9000
router ospf
 redistribute static route-map STATIC-TO-OSPF
```

```bash
show ip route 192.168.5.0/24     # source (O E2, R, B), tag, metric
show ip ospf database external   # type 5: advertising router, tag, forwarding address
```

## Troubleshooting redistribution

1. Is the route in the RIB of the redistributing router? (Only RIB-best routes can be redistributed.)
2. Does a route-map clause match? (Check implicit deny, prefix-list `le/ge`.)
3. Is the metric valid for the target (RIP ≤ 15)? Is the seed metric set?
4. In OSPF: is it E1/E2? Is the ASBR reachable (type 4 across areas)? Is the forwarding address reachable? Stub areas block externals.
5. Look for loops: traceroute shows bouncing between two routers; tag routes at each redistribution point to find the source.

## Design principles

1. Prefer a **single IGP**; redistribute only when migrating or merging.
2. If you must redistribute, do it at **one point** (or with strict tagging at two), with explicit filters and tags both ways.
3. Redistribute **specific, summarized** prefixes, and put a discard route for summaries.
4. Keep BGP out of the IGP: IGP for infrastructure reachability only.
5. Document each redistribution: from, to, metric, tags, filter.

## Key takeaways

- Redistribution copies RIB-best routes between protocols, needing a seed metric, and discards protocol-specific information.
- Mutual redistribution at two points causes loops and suboptimal routes unless you tag, filter and manage AD.
- Route-maps are ordered match/set clauses with an implicit deny; prefix-list `ge/le` semantics matter.
- Never dump the BGP table into the IGP; use iBGP and default routes, and keep next-hop reachability in the IGP.
