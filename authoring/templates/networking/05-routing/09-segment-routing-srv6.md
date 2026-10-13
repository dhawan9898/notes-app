---
title: "Segment Routing and SRv6"
track: networking
module: Routing
moduleOrder: 5
order: 9
level: expert
summary: "Source routing with segments: the idea, the SRH and SRv6 behaviors (End, End.DT6, encapsulation), a working five-node Linux lab whose per-hop packets show the segment list being consumed and a capture pitfall (in-place header rewrites seen late by a plain packet socket), header overhead, SR-MPLS, protection and operations."
prerequisites:
  - "IPv6 Header, Extension Headers and Addressing"
  - "Forwarding: Routing Table, Longest-Prefix Match, Policy Routing and ECMP"
sources:
  - label: RFC 8402 — Segment Routing Architecture
    url: https://www.rfc-editor.org/rfc/rfc8402
  - label: RFC 8754 — IPv6 Segment Routing Header (SRH)
    url: https://www.rfc-editor.org/rfc/rfc8754
  - label: RFC 8986 — Segment Routing over IPv6 (SRv6) Network Programming
    url: https://www.rfc-editor.org/rfc/rfc8986
  - label: RFC 8660 — Segment Routing with the MPLS Data Plane
    url: https://www.rfc-editor.org/rfc/rfc8660
  - label: RFC 9352 — IS-IS Extensions to Support Segment Routing over the IPv6 Data Plane
    url: https://www.rfc-editor.org/rfc/rfc9352
  - label: RFC 9252 — BGP Overlay Services Based on Segment Routing over IPv6 (SRv6)
    url: https://www.rfc-editor.org/rfc/rfc9252
  - label: RFC 9800 — Compressed SRv6 Segment List Encoding in the Segment Routing Header
    url: https://www.rfc-editor.org/rfc/rfc9800
  - label: Linux kernel — seg6 (net/ipv6/seg6*.c); ip-route(8) "encap seg6" and "seg6local"
    url: https://man7.org/linux/man-pages/man8/ip-route.8.html
---

Ordinary IP routing is **destination-based and hop-by-hop**: every router looks at the destination address and decides alone where to send the packet, so the path is whatever the routing protocol computed (shortest by metric). To steer traffic along a *specific* path (avoid a congested link, pass through a firewall, follow a low-latency route, keep a customer's traffic in a VPN) the classic tools are per-hop policy routing or MPLS-TE with per-path signalling state (RSVP-TE) in every router on the path.

**Segment Routing (SR)**, built on **source routing**, takes a different approach: the *ingress* router writes the path into the packet as an ordered list of **segments**, and the network just follows the list. A **segment** is an instruction ("go to router X by the shortest path", "leave through this link", "decapsulate and deliver in VRF Y"). Intermediate routers keep **no per-path state**: the state is in the packet. Only the ingress node needs to know the path, usually computed by a controller or by the IGP itself.

SR has two data planes. **SR-MPLS** (RFC 8660) encodes segments as a stack of MPLS labels (see the MPLS note). **SRv6** encodes segments as **IPv6 addresses** carried in a new IPv6 extension header, the **Segment Routing Header (SRH)**, and is what this note builds.

## Segment identifiers and behaviors

In SRv6 a segment is a **SID**, a 128-bit IPv6 address that identifies a router and *a function to perform there*. A typical SID has a structure `Locator : Function : Arguments`: the **locator** is a routable prefix that leads to the node (advertised in the IGP like any other prefix, so ordinary routing delivers packets to it); the **function** says what the node does when it receives a packet addressed to the SID. The set of functions is called the **network programming** model (RFC 8986). The important ones:

| Behavior | What the node does |
|---|---|
| **End** | the plain "node" segment: decrement Segments Left, copy the next SID into the destination address, forward by normal routing |
| **End.X** | like End, but forward to a *specific adjacency/next hop*, ignoring the routing table: steers over a chosen link |
| **End.DT6 / DT4 / DT46** | decapsulate the outer IPv6 header and look up the inner destination in a given table (a VRF): the egress of an SRv6 VPN |
| **End.DX6 / DX4** | decapsulate and forward to a fixed next hop |
| **End.B6.Encaps** | bind: push a *new* outer header with its own segment list (stitching policies) |
| **H.Encaps** (headend) | at the ingress: encapsulate the packet in a new IPv6 header, plus an SRH if more than one segment is needed |

SRH-less operation is possible: with a single SID the outer destination address alone does the job, with no extension header at all.

## The Segment Routing Header

The SRH (RFC 8754) is an IPv6 routing header with type 4:

| Field | Size | Meaning |
|---|---|---|
| Next Header | 8 bits | what follows (41 = encapsulated IPv6 packet) |
| Hdr Ext Len | 8 bits | length in 8-byte units, not counting the first 8 bytes |
| Routing Type | 8 bits | 4 |
| **Segments Left** | 8 bits | index of the *active* segment (how many are still to visit) |
| Last Entry | 8 bits | index of the last element of the list |
| Flags, Tag | 8 + 16 bits | rarely used |
| **Segment List[0..n]** | 128 bits each | **stored in reverse order**: element 0 is the *last* segment of the path |

The packet's IPv6 **destination address always holds the active SID** (`Segment List[Segments Left]`). When a node finishes executing its behavior, it decrements Segments Left and copies the new active segment into the destination address; ordinary routing then carries the packet to that SID's locator. Because every SID is routable, **SRv6 uses the existing IPv6 forwarding plane and the IGP**: a router that is not an SR node (no SID processing) simply forwards a packet addressed to a SID like any other IPv6 packet.

## A working five-node lab

The lab builds `h1 — r1 — r2 — r3 — h2` with Linux network namespaces. Plain IPv6 routing carries the underlay. Two SIDs are configured: `fc00:2::1` on **r2** with the **End** behavior and `fc00:3::d6` on **r3** with **End.DT6** (decapsulate and look the inner address up in the main table). On **r1**, the headend, a route for h2's prefix `2001:db8:2::/64` applies `encap seg6 mode encap segs fc00:2::1,fc00:3::d6`: encapsulate the customer packet in an outer IPv6 header with an SRH and send it via r2 then r3. h1 and h2 know nothing about SRv6.

{{include networking/srv6/lab.sh}}

{{include networking/srv6/sniff6.py}}

{{run networking/srv6 :: unshare -Urnm sh lab.sh 2>&1}}

Reading the output (the kernel notation `segs 2 [ fc00:2::1 fc00:3::d6 ]` lists segments in *travel order*):

1. **Configuration lines**: r1 has an `encap seg6 mode encap` route for the customer prefix; r2 has `seg6local action End`; r3 has `seg6local action End.DT6 table main`.
2. **The ping works**: h1 and h2 communicate normally, with a TTL of 61 on the reply (three router hops).
3. **On the r1—r2 link** the packet is **encapsulated**: outer source `fc00:12::1` (r1), outer destination `fc00:2::1` (the *first* SID), an SRH with `segments_left=1` and the list stored in reverse (`['fc00:3::d6', 'fc00:2::1']`), and the customer packet (h1's ICMPv6 echo) inside.
4. **On the r2—r3 link**, after r2 executed **End**: `segments_left` has dropped to **0**, the destination address is now the second SID `fc00:3::d6`, and the hop limit has been decremented. r2 did not look at the inner packet at all.
5. **On the r3—h2 link**, after **End.DT6**: the outer header and SRH are gone; the packet is h1's original IPv6 packet again, routed to h2 by the main table.

Return traffic from h2 to h1 uses ordinary IPv6 routing: there is no SR policy in that direction, so SR paths are **unidirectional**: a bidirectional policy needs headends at both ends.

### A capture pitfall found while building this lab

The first version of the lab used a normal `AF_PACKET` socket and showed the packet leaving r1 *already* with `segments_left=0` and the second SID as destination, which was impossible. The cause is not a kernel bug but how packet taps work: a plain packet socket queues a **clone that shares the packet's data buffer**, and the data is copied to the application only when it calls `recvfrom`. SRv6 `End` rewrites the destination address and Segments Left **in place**, so by the time the sniffer read r1's transmitted packet, r2 had already modified the shared bytes. Removing r2's SID made r1's packet look correct, which exposed the artifact. The fix is a **memory-mapped RX ring** (`PACKET_RX_RING`, the same mechanism `tcpdump` uses by default on modern systems), where the kernel copies the packet into the ring at the instant it passes the tap; the `ring.py` helper in the lab does exactly that, and the output above is therefore faithful. The lesson is general: when a protocol rewrites headers in place on a virtual (veth) path, a late capture can show the *next* hop's state; capture with a ring buffer, or capture at a point the rewrite cannot reach.

## Overhead and MTU

Encapsulation costs bytes: an outer IPv6 header (40 bytes) plus the SRH (8 bytes + 16 bytes per segment): two segments add 80 bytes; five add 128. On a 1500-byte MTU path a full-size customer packet no longer fits, so the core MTU must be raised (jumbo frames) or the headend fragments/limits the inner MTU. With a single segment no SRH is needed (40 bytes). Long segment lists also tax hardware: parsing and rewriting many 16-byte entries is slow in some forwarding ASICs, and routers differ in the **maximum SID depth** they can process (**MSD**), advertised in the IGP so a controller does not build lists the headend or transit nodes cannot handle.

**Compression** shrinks this: **uSID / C-SID** (RFC 9800 and the earlier uSID work) packs several short 16- or 32-bit SIDs into one 128-bit IPv6 address so that a whole path fits in the destination address with *no SRH* in many cases; each node shifts the address by one SID as the packet moves.

## Where the state lives: IGP, BGP, controllers

- **The IGP** (IS-IS or OSPFv3; RFC 9352 for IS-IS) advertises each node's **locator** (so the SIDs are reachable) and the SID functions available at each node (End, End.X per adjacency) and the node's capabilities (MSD). Shortest-path routing to a locator is itself a segment ("go to X by the IGP's shortest path": a *prefix/node SID*).
- **BGP** (RFC 9252) carries service SIDs for overlay VPN routes: a PE advertises a VPN route with an `End.DT46`-style SID and remote PEs encapsulate towards it, replacing MPLS labels in L3VPN and EVPN (see *MPLS, LDP and MPLS L3VPN* and *VXLAN and BGP EVPN*).
- **Traffic engineering** policies (an ordered segment list per colour and endpoint) are computed by the headend from the IGP topology database or provisioned by a **controller** over BGP-SR-Policy or PCEP; only the headend holds per-policy state. This is the main advantage over RSVP-TE, which needs state in every router on every LSP.

## Fast reroute

**TI-LFA** (topology-independent loop-free alternate) uses segment lists to pre-compute a repair path around any failed link or node that is guaranteed loop-free *and* post-convergence-optimal, by encoding the detour as a short segment list; the backup is installed in advance and switching takes tens of milliseconds, typically well under 50 ms, without any signalling (compare *BFD*, which detects the failure that triggers it).

## Security

The SRH is a **source-routing** header, and source routing has a history of abuse (IPv6 Type 0 routing headers were deprecated because they enabled traffic amplification). RFC 8754 therefore defines an **SR domain**: SIDs must be reachable only from inside it. The operator should **filter at the domain boundary**: drop packets from outside whose destination is a SID/locator of the domain, and drop packets entering from outside with an SRH. An optional **HMAC TLV** can authenticate the SRH. The Linux kernel only processes SRH packets on interfaces where `seg6_enabled` is set (the lab sets it) and, by default, does nothing on others; leaving that sysctl off at edge interfaces is a cheap guard.

## Linux building blocks

| Command | Role |
|---|---|
| `ip -6 route add PREFIX encap seg6 mode encap segs S1,S2 dev IF` | headend: encapsulate traffic to a prefix (also `mode inline` inserts an SRH without an outer header, `mode l2encap` carries Ethernet) |
| `ip -6 route add SID/128 encap seg6local action End dev IF` | endpoint behavior (also `End.X nh6 ADDR`, `End.DT6 table N`, `End.DX6 nh6 ADDR`, `End.DT4`, `End.B6.Encaps`, and flavors such as `psp`) |
| `sysctl net.ipv6.conf.IF.seg6_enabled=1` | accept and process SRH on an interface |
| `ip -6 route show` | shows the `encap seg6`/`seg6local` routes |

Remember that interfaces created *after* you set `net.ipv6.conf.all.forwarding=1` take their forwarding setting from `net.ipv6.conf.default.forwarding`, which the lab also sets: forgetting it makes routers forward locally generated pings but silently drop forwarded traffic (the second mistake made while debugging this lab).

## Failure modes

| Symptom | Likely cause | Check |
|---|---|---|
| Traffic enters the SR policy but never arrives | a SID is not reachable (locator not advertised/routed), or `seg6_enabled` is off on the receiving interface, or the behavior is mis-configured | `ip -6 route get SID` on the previous hop; capture on each link |
| Large packets disappear, small ones pass | MTU exceeded by encapsulation overhead and ICMP "packet too big" filtered | `ping -s`, path MTU |
| Return traffic takes another path | SR policies are unidirectional | check the reverse headend |
| Works in the lab, not on a vendor router | unsupported behavior or SID depth, SRH processing in software slow path | platform MSD, behavior support |
| Packets with SRH from outside the domain reach SIDs | missing ingress filtering | ACLs at the boundary |

## Key takeaways

1. Segment Routing is source routing with state in the packet: the ingress encodes the path as a list of segments, and transit routers keep no per-path state.
2. In SRv6 a segment is an IPv6 address (locator, function, arguments); the destination address always holds the active SID, the SRH lists the rest in reverse order, and Segments Left counts down as nodes run their End behaviors.
3. The lab demonstrates the whole life cycle: encapsulation at the headend (SL=1), End at r2 (SL=0, new destination), End.DT6 at r3 (decapsulation).
4. SRv6 reuses the IPv6 data plane and the IGP; the costs are header overhead (40 + 8 + 16n bytes), hardware limits on SID depth, and the need for domain-edge filtering. Compression (uSID) reduces the overhead.
5. SR gives TE without RSVP-TE state, VPNs without MPLS labels, and TI-LFA fast reroute; paths are unidirectional.
6. When capturing header-rewriting protocols on veth paths, use a ring-buffer capture: a late read of a shared buffer can show the next hop's state.
