---
title: "OSPF Areas, LSA Types, Stub/NSSA and Summarization"
track: networking
module: Routing
moduleOrder: 5
order: 4
level: advanced
summary: Hierarchical OSPF design — what each LSA type carries and where it floods, area types and their trade-offs, external route handling (E1/E2/N1/N2), summarization and virtual links.
prerequisites:
  - "OSPFv2 Operation: Neighbors, DR/BDR, Flooding and SPF"
sources:
  - label: "RFC 2328 — OSPF Version 2 (Appendix A: LSA formats; §3 areas; §12 LSAs)"
    url: https://www.rfc-editor.org/rfc/rfc2328
  - label: "RFC 3101 — The OSPF Not-So-Stubby Area (NSSA) Option"
    url: https://www.rfc-editor.org/rfc/rfc3101
  - label: "RFC 5340 — OSPF for IPv6 (LSA types)"
    url: https://www.rfc-editor.org/rfc/rfc5340
  - label: "RFC 5250 — The OSPF Opaque LSA Option"
    url: https://www.rfc-editor.org/rfc/rfc5250
  - label: "RFC 5838 — Support of Address Families in OSPFv3"
    url: https://www.rfc-editor.org/rfc/rfc5838
  - label: "RFC 6549 / RFC 5185 — OSPFv2 multi-instance; multi-area adjacency"
    url: https://www.rfc-editor.org/rfc/rfc6549
---

## Why areas

Flooding scope and SPF cost grow with the LSDB. OSPF limits them with a **two-level hierarchy**: **area 0 (the backbone)** and any number of non-backbone areas, which must each attach to area 0. Within an area, routers have the full topology; between areas, they exchange only **summarized reachability** (like distance-vector). Benefits: smaller LSDBs, SPF runs confined to the area, instability containment, and summarization points.

Roles:

- **Internal router**: all interfaces in one area.
- **ABR** (Area Border Router): interfaces in two or more areas, one of which is area 0; keeps a separate LSDB per area and originates summary LSAs.
- **ASBR** (AS Boundary Router): redistributes external routes (static, BGP, other IGPs) into OSPF.
- **Backbone router**: has an interface in area 0.

All inter-area traffic passes through area 0 (a "hub-and-spoke" of areas). That topological rule is what makes the inter-area distance-vector-style exchange loop-free.

Practical guidance: a single area 0 comfortably handles several hundred routers on modern hardware. Introduce areas for **administrative or fault boundaries**, summarization or very large networks, not by default. Ensure each non-zero area has **two ABRs** for redundancy.

## LSA types

| Type | Name (v2) | Originated by | Flooded in | Content |
|---|---|---|---|---|
| **1** | **Router LSA** | every router | its area | the router's links in that area: type (p2p, transit, stub, virtual), cost, neighbor/network IDs; E/B bits (ASBR/ABR flags) |
| **2** | **Network LSA** | **DR** of a multi-access segment | its area | list of routers attached to the segment, with the subnet mask |
| **3** | **Summary (network) LSA** | ABR | the other areas | an inter-area prefix and the ABR's cost to it |
| **4** | **ASBR-Summary LSA** | ABR | the other areas | how to reach an ASBR (so routers in other areas can use type 5) |
| **5** | **AS-External LSA** | ASBR | whole AS (except stub/NSSA) | external prefix, metric, E1/E2 type, forwarding address, route tag |
| **7** | **NSSA External LSA** | ASBR in an NSSA | that NSSA only | as type 5; translated to 5 at the ABR |
| 9 / 10 / 11 | **Opaque** (link-local / area / AS scope) | any | scope-limited | extensibility: TE (RFC 3630), Segment Routing, graceful restart grace LSA |

Link-State ID and Advertising Router together identify an LSA. Type 1's Link-State ID is the originating Router ID; type 2's is the **DR's interface address**; type 3's is the destination prefix.

### Reading the Router LSA

Each link in a Router LSA has a *type*:

| Link type | Description | Link ID | Link Data |
|---|---|---|---|
| 1 Point-to-point | to another router | neighbor Router ID | our interface IP |
| 2 Transit | to a multi-access network with a DR | DR's interface address | our interface IP |
| 3 Stub | to a network with no neighbors | network address | netmask |
| 4 Virtual | virtual link | neighbor Router ID | our interface IP |

SPF builds edges from p2p and transit links (checking bidirectionality), and attaches stub networks as leaves. Since Router LSA size grows with the links, a router with thousands of stub links (e.g., many loopbacks/sub-interfaces) is a scalability hazard. Make interfaces passive or advertise prefixes more compactly.

### Inter-area route computation

An ABR computes intra-area routes in each of its areas, then advertises a **type 3 LSA** into every other area for each intra-area (or inter-area learned from area 0) prefix, with the cost from itself. A router in the other area computes the cost to the ABR (via SPF) and adds the advertised cost: this is distance-vector behavior between areas. To avoid loops, an ABR only considers type 3 LSAs from the **backbone** (area 0) when computing inter-area routes (and ignores those learned via non-backbone areas): a router that is not attached to area 0 is not an ABR for these purposes.

### External routes: E1 and E2

ASBR redistributes external routes as type 5 LSAs. Other routers need a path to the ASBR (via SPF in-area, or the **type 4** LSA from an ABR when the ASBR is in a different area).

| Metric type | Total cost |
|---|---|
| **E2** (default) | the external metric only: internal path cost is ignored (but used as a tiebreaker). Best when there is a single ASBR/exit |
| **E1** | external metric **+** internal cost to the ASBR. Use when multiple ASBRs advertise the same prefix and you want the nearest exit |

The **forwarding address** field (non-zero) tells routers to send traffic to a specific next hop (on a shared segment) instead of the ASBR itself; pitfalls arise when the forwarding address isn't reachable via an intra/inter-area route: the external LSA is ignored.

## Area types

| Area type | Allows | Blocks | Default route | Use |
|---|---|---|---|---|
| **Standard (normal)** | types 1,2,3,4,5 | | | the default |
| **Stub** | 1,2,3 | 4,5 | ABR injects a type 3 default (`0.0.0.0/0`) | small branch: no external routes, minimal LSDB |
| **Totally stubby** (Cisco ext.; "no-summary") | 1,2 + one type 3 default | 3,4,5 | ABR injects default | tiny sites that need only a default |
| **NSSA** | 1,2,3 + type 7 | 4,5 | optional type 7 default | a stub area that must **import** a few external routes (e.g. branch with a local ISP/static route) |
| **Totally NSSA** | 1,2,7 + default | 3,4,5 | default | NSSA with "no-summary" |

Configuration constraint: the stub / NSSA flag is in the Hello options (E bit cleared for stub), so **every router in the area must agree**. An ASBR cannot be in a (plain) stub area. Virtual links cannot cross stub areas.

**NSSA (RFC 3101)**: the ASBR inside the NSSA originates **type 7** LSAs, which flood only inside it. The NSSA ABR **translates type 7 → type 5** (if the P-bit "propagate" is set) and floods the type 5 to the backbone; with several ABRs, the one with the highest Router ID is elected translator. Type 7 routes show as N1/N2 in the table.

## Summarization

Without summarization, every intra-area prefix is advertised as a separate type 3 into other areas: any flap in one area is seen throughout.

- **Inter-area summarization at the ABR**: `area <id> range <prefix> [cost]`. The ABR advertises only the aggregate (when any component exists) and suppresses components. The ABR installs a **discard (Null0) route** for the summary to prevent loops for non-existent subnets. Summary cost = the max or min of the components (RFC 1583-compatible vs RFC 2328: highest cost by default for v2).
- **External summarization at the ASBR**: `summary-address <prefix>`.
- Design addresses hierarchically so that each area has a contiguous block (e.g. area N = 10.N.0.0/16).

```text
router ospf
 area 1 range 10.1.0.0/16
 area 1 stub no-summary          ! totally stubby (ABR only)
 area 2 nssa
 summary-address 192.168.0.0/16  ! ASBR
```

## Virtual links

When an area cannot physically connect to area 0 (or area 0 is partitioned), a **virtual link** creates a logical area-0 adjacency across a transit non-backbone area between two ABRs. It is a repair mechanism, not a design: it relies on the transit area's stability, can't cross stub areas, and is hard to debug. Prefer re-cabling or tunnels.

## OSPFv3 differences (RFC 5340)

| Aspect | OSPFv2 | OSPFv3 |
|---|---|---|
| Runs per | IP **subnet** | **link** (multiple prefixes on a link; neighbors can be on different subnets) |
| Addresses in packets | IPv4 in headers/LSAs | none in router/network LSAs; prefixes in separate LSAs; next hops are **link-local** |
| Router LSA / Network LSA | contain addresses and masks | topology only |
| New LSAs | | **Link LSA** (type 8: link-local address + prefixes of the link), **Intra-Area-Prefix LSA** (type 9) |
| Type 3 / 4 / 5 / 7 | Summary, ASBR-summary, External, NSSA | renamed **Inter-Area-Prefix**, **Inter-Area-Router**, AS-External, NSSA |
| Flooding scope | area, AS (and link-local for opaque) | encoded in the LSA type's S1/S2 bits (link-local, area, AS) |
| Authentication | in OSPF header | none in protocol; IPsec AH/ESP, or Authentication Trailer (RFC 7166) |
| Instance ID | n/a | multiple instances per link (also carries address family, RFC 5838) |
| Router ID | 32-bit, often an address | 32-bit, must be configured explicitly |

## Design checklist

1. Router IDs = loopback /32, unique, explicitly set.
2. Ethernet p2p links as `point-to-point`; passive interfaces on user networks.
3. Consistent reference bandwidth; costs reflecting actual capacity.
4. Areas only where justified; contiguous addressing per area; summaries at ABRs.
5. Stub/NSSA for edge sites; avoid redistributing large tables into OSPF.
6. BFD for failure detection; cryptographic authentication; max-metric for maintenance.
7. Monitor LSDB size, SPF run count and duration, LSA flooding rate.

## Key takeaways

- Area 0 is the hub: inter-area routing is distance-vector-like via type 3 LSAs from ABRs, which keeps it loop-free.
- Type 1/2 are intra-area topology; 3/4 describe other areas; 5/7 carry external routes (E1 adds internal cost, E2 does not).
- Stub, totally stubby and NSSA areas shrink the LSDB by blocking external (and summary) LSAs; NSSA still allows local ASBRs via type 7 → 5 translation.
- Summarize at ABR (`area range`) and ASBR (`summary-address`) with discard routes; area flags must match across the area.
- OSPFv3 changes the data model (link-based, link-local next hops, separate prefix LSAs) but keeps the machinery.
