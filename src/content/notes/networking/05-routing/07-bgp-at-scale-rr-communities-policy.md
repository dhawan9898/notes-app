---
title: "BGP at Scale: Route Reflectors, Communities, Policy, Security and Convergence"
track: networking
module: Routing
moduleOrder: 5
order: 7
level: expert
summary: Scaling iBGP with route reflectors and confederations, communities and traffic engineering, business-relationship policy, RPKI and filtering, data-center eBGP and convergence behavior.
sources:
  - label: "RFC 4456 — BGP Route Reflection"
    url: https://www.rfc-editor.org/rfc/rfc4456
  - label: "RFC 5065 — Autonomous System Confederations for BGP"
    url: https://www.rfc-editor.org/rfc/rfc5065
  - label: "RFC 1997 / RFC 4360 / RFC 8092 — Communities, Extended Communities, Large Communities"
    url: https://www.rfc-editor.org/rfc/rfc8092
  - label: "RFC 7999 — BLACKHOLE Community; RFC 8326 — Graceful BGP Session Shutdown"
    url: https://www.rfc-editor.org/rfc/rfc7999
  - label: "RFC 6811 / RFC 6480 / RFC 9582 — RPKI Prefix Origin Validation; RPKI architecture; ROA profile"
    url: https://www.rfc-editor.org/rfc/rfc6811
  - label: "RFC 7908 / RFC 9234 — Problem Definition and Classification of BGP Route Leaks; Route leak prevention using roles (OTC)"
    url: https://www.rfc-editor.org/rfc/rfc9234
  - label: "RFC 7938 — Use of BGP for Routing in Large-Scale Data Centers"
    url: https://www.rfc-editor.org/rfc/rfc7938
  - label: "RFC 7911 / RFC 5549 / RFC 5880 — Add-Path; Extended Next Hop (unnumbered); BFD"
    url: https://www.rfc-editor.org/rfc/rfc7911
  - label: "BCP 194 (RFC 7454) — BGP Operations and Security"
    url: https://www.rfc-editor.org/rfc/rfc7454
  - label: "MANRS — Mutually Agreed Norms for Routing Security"
    url: https://www.manrs.org/
---

## Scaling iBGP

A full iBGP mesh needs `n(n−1)/2` sessions (100 routers = 4,950), and each router holds all peers' state. Two standard solutions:

### Route reflectors (RFC 4456)

A **route reflector (RR)** is allowed to break the iBGP split-horizon rule:

- Peers are classified as **clients** or **non-clients**. Clients peer only with their RR(s).
- Rules: a route from a **client** is reflected to all clients and non-clients; a route from a **non-client** is reflected only to clients; eBGP routes go to everyone.
- The RR advertises only its **best** path for each prefix (so clients see less path diversity unless Add-Path/Best-External is used).
- **Loop prevention**: `ORIGINATOR_ID` (router ID of the originating iBGP speaker; a router rejects routes with its own ID) and `CLUSTER_LIST` (a list of cluster IDs the route traversed; an RR rejects a route containing its own cluster ID). A **cluster** = RR(s) + their clients; redundant RRs of the same cluster share a cluster ID (or use distinct IDs and accept more paths).
- Attributes (NEXT_HOP, AS_PATH, LOCAL_PREF, MED) are **not modified** by reflection, so clients still need IGP reachability to the next hop.

Design: two RRs per cluster, placed topologically close to their clients (so the IGP-metric tie-break they use matches clients' view), RRs in a full mesh at the top (or hierarchical RRs). The RR is a control-plane element; it need not be in the forwarding path, which allows dedicated virtual RRs. Caveats: loss of path diversity can cause **sub-optimal routing** or MED oscillations; reflected best path is *the RR's choice*.

### Confederations (RFC 5065)

Split an AS into sub-ASes (**member ASes**), full-mesh inside each, eBGP-like between them. Outside the confederation, the AS appears as one. Rarely used in new designs, but found in large legacy networks. Confed-specific path segment types (AS_CONFED_SEQUENCE/SET) are removed at the boundary. Complexity: policy semantics (LOCAL_PREF and MED are preserved between sub-ASes).

## Communities: signalling policy

| Type | Format | Notes |
|---|---|---|
| **Standard** (RFC 1997) | 32 bits, `ASN:value` (16:16) | most common; cannot carry 4-byte ASNs |
| **Well-known** | `NO_EXPORT` (65535:65281, 0xFFFFFF01): don't advertise outside the AS (or confed); `NO_ADVERTISE` (0xFFFFFF02): don't advertise to any peer; `NO_EXPORT_SUBCONFED`; `GRACEFUL_SHUTDOWN` (65535:0); `BLACKHOLE` (65535:666, RFC 7999) | |
| **Extended** (RFC 4360) | 8 bytes, typed | Route Target / Route Origin for VPNs, EVPN, bandwidth |
| **Large** (RFC 8092) | 12 bytes: `Global Admin(4):Local1(4):Local2(4)` | for 4-byte ASNs; free-form functions |

Typical community schemes (published by transit providers) let customers control **inbound traffic engineering**: "prepend once/twice/thrice to peer X", "set LOCAL_PREF 80 (backup)", "don't announce to Europe", "blackhole this /32". Customers tag their prefixes with origin communities (e.g. `65001:100` = learned from customer at site A) so that the policy can match on tags, not on prefixes.

**RTBH** (remotely triggered black hole): announce an attacked /32 with a BLACKHOLE community; the upstream null-routes it, sacrificing that address to protect the rest. Flowspec (RFC 8955) is the finer-grained alternative that distributes ACL-like filters.

## Policy: business relationships

BGP policy models money flow: customers pay you, peers exchange traffic settlement-free, and you pay providers.

| Learned from | Prefer | Advertise to |
|---|---|---|
| **Customer** | highest LOCAL_PREF (e.g. 200) | everyone (customers, peers, providers) |
| **Peer** | middle (150) | customers only |
| **Provider (transit)** | lowest (100) | customers only |

The export rules produce **valley-free** (Gao–Rexford) paths: up toward providers, at most one peering step, then down toward customers. Violations are **route leaks** (e.g. a customer re-advertising one provider's routes to another provider), a recurrent source of Internet outages (RFC 7908). RFC 9234 adds BGP **roles** in the OPEN message and the **Only-To-Customer (OTC)** attribute to detect leaks automatically.

### Route maps / policy language building blocks

```text
ip prefix-list CUST-A seq 5 permit 203.0.113.0/24 le 24
bgp as-path access-list CUST-A-AS permit ^65010(_65010)*$
route-map CUST-IN permit 10
 match ip address prefix-list CUST-A
 match as-path CUST-A-AS
 set local-preference 200
 set community 65001:100 additive
route-map CUST-IN deny 99
```

Philosophy for edge routers: **default deny** with explicit permits; *filter on prefix and AS_PATH for customers, on max-prefix for peers, and drop bogons for everyone*.

## Traffic engineering knobs

| Goal | Outbound (traffic leaving you) | Inbound (traffic entering you) |
|---|---|---|
| Control | LOCAL_PREF, weight, IGP cost to exit | **AS_PATH prepending**, MED, provider communities, **more specific** prefixes (de-aggregation), selective advertisement |
| Strength | strong: your own decision | weak: you influence neighbors' decisions |

Notes: prepending only affects the path-length step (too late if the neighbor sets LOCAL_PREF by relationship); MED is honored by many but compared only within one neighbor AS; more-specific announcements win by longest-prefix match but bloat the global table and may be filtered (/24 limit for IPv4, /48 for IPv6). Traffic on the Internet is inherently asymmetric.

## Routing security

| Threat | Mitigation |
|---|---|
| **Prefix hijack / origin error** | **RPKI Route Origin Validation**: a ROA (signed by the address-holder's RIR certificate) binds prefix, max-length and origin ASN. Routers fetch validated ROA payloads (via RTR protocol, RFC 8210) and classify routes as **Valid / Invalid / NotFound** (RFC 6811). **Drop Invalid** at the edge; prefer Valid. Keep ROAs' max-length tight |
| Path manipulation | **BGPsec** (RFC 8205), little deployed; **ASPA** (AS Provider Authorization) validates the upstream path (in standardization) |
| Route leaks | strict customer filters, IRR-based prefix lists (generated by `bgpq4`), max-prefix, RFC 9234 roles/OTC |
| Session attacks | TCP-AO, GTSM, control-plane ACL/CoPP, dedicated management of peers |
| Bogons/overly specific | filter reserved prefixes, private ASNs in AS_PATH, > /24 (v4), > /48 (v6), default routes from peers |
| Resource exhaustion | **max-prefix** limits with warning thresholds |

The **MANRS** program and BCP 194 (RFC 7454) summarize the baseline: filter customers, validate origin, anti-spoofing (uRPF/BCP 38), and maintain accurate contact/IRR data.

## Data-center BGP (RFC 7938)

Large Clos fabrics often use **eBGP as the only protocol** (no IGP):

- Each leaf (or each rack) has its own ASN (private, 4-byte), spines share an ASN per tier or per pod. All adjacencies are eBGP, so the AS_PATH naturally prevents loops and gives simple ECMP semantics.
- Enable **multipath** (`bgp bestpath as-path multipath-relax`) so paths with different (but equal-length) AS_PATHs are used together.
- Leaves advertise server subnets/loopbacks; **allowas-in** or unique ASNs avoid loop-prevention problems when ASNs are reused.
- Use **BGP unnumbered** (RFC 5549 extended next hop: IPv4 prefixes with IPv6 link-local next hops) so you don't need /31 addresses on every link and you can auto-discover neighbors using RAs.
- Tune timers: BFD, short hold times (3 s), `advertisement-interval 0`, ECMP resilient hashing; **graceful shutdown** (RFC 8326) drains traffic before maintenance.
- Overlay: **EVPN** over the same BGP sessions carries MAC/IP routes for VXLAN (see overlay notes).

## Convergence and stability

- **Path exploration**: after a withdrawal, BGP can cycle through progressively longer alternative paths before settling (each step gated by MRAI), so global convergence takes minutes.
- **Add-Path** (RFC 7911): advertise multiple paths per prefix so alternate next hops exist (needed for PIC and for RRs to preserve diversity). **Best-external**: advertise the best eBGP path even if the iBGP path is preferred.
- **BGP PIC** (Prefix Independent Convergence): the FIB keeps a precomputed backup for the shared next hop, so thousands of prefixes switch in a single update when a next hop fails, instead of per prefix.
- **Route flap damping** (RFC 2439) suppresses unstable routes but can penalize legitimate ones; RIPE-580 recommends against or with conservative parameters.
- **Scale**: the global table has on the order of a million IPv4 prefixes and well over 200,000 IPv6 prefixes (check current statistics at bgp.potaroo.net / CIDR Report). Memory ~ prefixes × paths × attributes; FIB TCAM capacity is the hard limit on hardware.

## Operational checklist

1. Router ID, ASN, session auth, `ttl-security`, max-prefix, BFD.
2. Inbound policy per neighbor type (customer/peer/transit), explicit default deny, RPKI invalid drop.
3. Outbound: only your prefixes (and customers'), set communities, strip private ASNs, no default unless intended.
4. iBGP: loopback peering, `next-hop-self`, RRs in pairs, consistent cluster-IDs, IGP reachability for next hops.
5. Monitoring: session state, received/accepted prefixes vs. baseline, RPKI invalids, BGP update rate, looking glasses/BMP (RFC 7854) feeds.
6. Change control: graceful shutdown, staged policy changes, route-refresh/soft reconfiguration for policy changes.

## Key takeaways

- Scale iBGP with route reflectors (clients, ORIGINATOR_ID, CLUSTER_LIST), usually in redundant pairs; reflection hides alternatives unless Add-Path is used.
- Communities carry policy: use them for TE, tagging origin and RTBH; large communities support 4-byte ASNs.
- Business relationships define LOCAL_PREF and export policy (valley-free); mistakes become route leaks.
- Secure the edge: filters, max-prefix, RPKI ROV, TCP-AO/GTSM; adopt roles/OTC and MANRS practices.
- In data centers, eBGP with per-device ASNs, multipath relax and unnumbered peering is the common underlay; use PIC, BFD and graceful shutdown for fast, safe changes.
