---
title: IS-IS — Levels, NETs, LSPs and Why Providers Love It
track: networking
module: Routing
moduleOrder: 5
order: 5
level: advanced
summary: The ISO link-state IGP — NET addressing, L1/L2 hierarchy, PDU types, DIS election, TLV-based extensibility, wide metrics, and how it differs from OSPF.
sources:
  - label: "ISO/IEC 10589:2002 — Intermediate System to Intermediate System intra-domain routing information exchange protocol"
  - label: "RFC 1195 — Use of OSI IS-IS for Routing in TCP/IP and Dual Environments"
    url: https://www.rfc-editor.org/rfc/rfc1195
  - label: "RFC 5305 / RFC 5308 — IS-IS extensions for Traffic Engineering (wide metrics); Routing IPv6 with IS-IS"
    url: https://www.rfc-editor.org/rfc/rfc5308
  - label: "RFC 5120 — M-ISIS: Multi Topology Routing in IS-IS"
    url: https://www.rfc-editor.org/rfc/rfc5120
  - label: "RFC 5304 / RFC 5310 — IS-IS cryptographic authentication (HMAC-MD5; generic)"
    url: https://www.rfc-editor.org/rfc/rfc5310
  - label: "RFC 3787 — Recommendations for Interoperable Networks using IS-IS"
    url: https://www.rfc-editor.org/rfc/rfc3787
  - label: "RFC 8667 — IS-IS Extensions for Segment Routing"
    url: https://www.rfc-editor.org/rfc/rfc8667
---

**IS-IS** (Intermediate System to Intermediate System) is a link-state IGP like OSPF, from the same algorithmic family (flooding LSPs, Dijkstra SPF). It originates from the OSI stack (DECnet Phase V). RFC 1195 adapted it to carry IP prefixes ("Integrated IS-IS"). It is the dominant IGP in large service-provider and many data-center-underlay networks.

## Key differences from OSPF

| Aspect | OSPF | IS-IS |
|---|---|---|
| Transport | IP protocol 89 | **directly on layer 2** (CLNS, no IP header): works for IPv4, IPv6 and others |
| Identity | 32-bit Router ID | **NET** (NSAP): area + 6-byte System ID |
| Hierarchy | areas attach to backbone *area 0*; ABR is a router in two areas | **Level-1 / Level-2**; the area boundary is on the **link**; a router belongs to one area |
| Packet types | 5, with many LSA types | 4 PDU kinds: IIH, LSP, CSNP, PSNP; extensibility by **TLVs** |
| Extensibility | opaque LSAs, new LSA types | just add TLVs/sub-TLVs, so new features (TE, SR, flex-algo) integrate with ease |
| DR | DR + BDR, non-preemptive | **DIS** only, preemptive; no backup; pseudonode |
| Metric | 16-bit interface cost | 6-bit "narrow" (max 63 per link) or **24-bit "wide"** (RFC 5305) |
| Fragmentation of control | LSAs limited by MTU | LSPs fragmented by the originator into up to 256 fragments (an LSP carries up to ~1492 bytes) |
| IPv6 | separate protocol (OSPFv3) | same process, new TLVs (single instance) |

## Addressing: the NET

Routers are identified with an **NSAP** called a **Network Entity Title (NET)**: `49.0001.1921.6800.1001.00`.

```text
49 . 0001 . 1921.6800.1001 . 00
AFI  Area ID    System ID     NSEL
```

- **AFI 49** = private addressing authority (like RFC 1918).
- **Area ID**: variable length (1–13 bytes). Routers with the same area ID form a Level-1 area.
- **System ID**: 6 bytes, unique within the domain at each level. Often derived from the loopback address (`192.168.1.1` → `1921.6800.1001`) or the MAC.
- **NSEL**: `00` for a router.

Exactly one NET per router is the usual practice, set on the loopback.

## Levels and hierarchy

- **Level 1 (L1)** routers know the topology of their own area only and send everything else to the nearest L1/L2 router (a default route, via the **ATT** bit in LSPs).
- **Level 2 (L2)** routers form the backbone connecting areas; they route between areas using area prefixes (and, nowadays, full IP prefixes).
- **L1/L2** routers (the default, Cisco) have both databases (separate LSDBs and SPF runs) and act as border routers.
- The **L2 backbone must be contiguous** (a chain of L2-capable routers/links), but it is not a special "area"; there is no area 0. Area borders are *links* between routers of different areas, not routers.
- Adjacency forms between routers of the same level and (for L1) the same area; L2 adjacency can span areas.

Route leaking: L2→L1 and L1→L2 leaking (RFC 5302) lets prefix details cross levels for optimal routing and TE; default L1→L2 propagation sends all L1 prefixes up, and L2→L1 only a default route. Most modern networks use **a single flat level (L2-only)** and add levels only when scale demands it.

## PDUs

| PDU | Role | Notes |
|---|---|---|
| **IIH (IS-IS Hello)** | discover neighbors, elect DIS | LAN IIH (L1 or L2) vs point-to-point IIH (three-way handshake, RFC 5303) |
| **LSP** (Link-State PDU) | carries topology and reachability TLVs | like an LSA; has a sequence number, remaining lifetime, checksum; **fragment number** (LSP ID = System ID + pseudonode ID + fragment) |
| **CSNP** (Complete Sequence Numbers PDU) | lists *all* LSP headers | sent periodically by the DIS on LANs (every 10 s) and once on p2p adjacency start |
| **PSNP** (Partial SNP) | acknowledge LSPs (p2p) / request missing ones (LAN) | |

Multicast MACs on LANs: `01-80-C2-00-00-14` (all L1 ISs), `01-80-C2-00-00-15` (all L2 ISs), `09-00-2B-00-00-05` (all ISs). All are in the IEEE reserved/DEC ranges. PDUs are encapsulated in 802.2 LLC with NLPID `0x83`, and IS-IS requires the L2 frame to be big enough: hellos are **padded to the interface MTU** by default, which verifies MTU compatibility (catching the OSPF-style mismatch problems). Disable padding after the adjacency to save bandwidth (`hello padding`).

### Timers and operation

| Parameter | Default |
|---|---|
| Hello interval | 10 s (DIS: 3.33 s) |
| Hold time | 3 × hello (30 s) |
| LSP max lifetime | 1200 s; refresh at 900 s |
| LSP generation / SPF | throttled with exponential back-off (RFC 8405-style) |
| CSNP interval | 10 s on LANs |

Operations: on a **LAN**, the **DIS** (Designated IS) is elected by highest priority (default 64), then highest MAC. Unlike OSPF's DR, the DIS is **preemptive**, there is no backup DIS (everyone adjacent to everyone), and the LAN is represented by a **pseudonode** LSP originated by the DIS. Synchronization is by periodic CSNPs; a router missing an LSP asks with a PSNP. On **point-to-point** links (recommended even on Ethernet: `isis network point-to-point`), a reliable exchange with PSNP acknowledgments is used and no DIS election occurs.

## LSP content: TLVs

| TLV | Name | Purpose |
|---|---|---|
| 1 | Area Addresses | the NETs' area IDs |
| 2 / **22** | IS Neighbors / **Extended IS Reachability** | adjacencies with metric (wide, 24-bit) and sub-TLVs (TE, link attributes, SR adjacency SIDs) |
| 128 / 130 / **135** | IP Internal / External / **Extended IP Reachability** | prefixes with 32-bit metric, up/down bit, sub-TLVs (tags, prefix SIDs) |
| 129 | Protocols Supported | NLPID: `0xCC` = IPv4, `0x8E` = IPv6 |
| 132 | IP Interface Address | |
| **137** | Dynamic Hostname | readable names in `show` output |
| **232 / 236** | IPv6 Interface Address / IPv6 Reachability | |
| 222 / 235 / 237 | multi-topology versions | M-ISIS, RFC 5120 |
| 10 | Authentication | cleartext or HMAC (RFC 5304, 5310) |
| 242 | Router Capability | SR, flex-algo, etc. |

Use **wide metrics only** (`metric-style wide`) on all routers; mixing narrow (6-bit) and wide metrics breaks routing. Metrics are typically configured manually (default 10 per link) or from bandwidth with a reference.

**Attached bit (ATT)**: an L1/L2 router sets it in its L1 LSP when it can reach other areas via L2; L1 routers install a default route to the closest router with ATT set. **Overload bit (OL)**: a router sets it to signal "don't use me for transit" (maintenance, boot, or when out of memory); similar to OSPF max-metric but built in.

## IPv6 and topologies

By default IS-IS runs a **single topology** for IPv4 and IPv6: SPF is computed once, so every link must support both protocols. If not, use **multi-topology (MT)** IS-IS (RFC 5120), which runs a separate SPF per topology (MT 0 = IPv4, MT 2 = IPv6).

## Security

IS-IS isn't IP, so remote attackers can't reach it. But adjacent devices can form adjacencies or inject LSPs. Use **HMAC-SHA authentication** on hellos and LSPs (RFC 5310), set interfaces passive or not enabled for IS-IS on user ports, and monitor LSP database changes.

## Configuration (FRR)

```text
interface lo
 ip router isis CORE
 isis passive
!
interface eth1
 ip router isis CORE
 ipv6 router isis CORE
 isis network point-to-point
 isis circuit-type level-2-only
 isis metric 10
 isis bfd
!
router isis CORE
 net 49.0001.1921.6800.1001.00
 is-type level-2-only
 metric-style wide
 lsp-mtu 1492
 spf-interval 1
```

```text
show isis neighbor
show isis database detail
show isis route
```

Troubleshooting neighbors: wrong level/circuit type, area mismatch for L1, duplicate System ID, authentication mismatch, MTU (padded hellos dropped), mixed metric styles, an interface not point-to-point on both ends.

## Why providers choose IS-IS

- **Protocol independence**: runs without IP addresses on the links (useful for unnumbered interfaces and when bootstrapping); IPv4 and IPv6 in one process.
- **Extensibility**: TLVs make it easy to add features (TE, Segment Routing, flex-algo, BGP-LS export) without new LSA types.
- **Stability and scale**: LSP fragmentation and pacing; fewer LSA types; flat L2 networks of thousands of nodes; smaller attack surface.
- **Operational simplicity** at scale: one level, wide metrics, point-to-point links.

Reasons to choose OSPF instead: familiarity, wider vendor/enterprise tooling, and an existing deployment.

## Key takeaways

- IS-IS runs over L2 directly, identified by NET, area boundaries lie on links, and it uses L1 (intra-area) and L2 (backbone) levels.
- Four PDU kinds, with all extensions as TLVs; the DIS and pseudonode replace OSPF's DR/BDR, with preemption.
- Use point-to-point circuits, wide metrics, single-level (L2-only) for most designs.
- ATT and Overload bits implement default routing between levels and draining.
- Extensibility (TE, SR, MT) is the reason service providers prefer it.
