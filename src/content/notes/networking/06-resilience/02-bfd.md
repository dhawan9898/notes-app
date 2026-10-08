---
title: BFD — Bidirectional Forwarding Detection
track: networking
module: Resilience
moduleOrder: 6
order: 2
level: advanced
summary: Sub-second failure detection independent of media and routing protocol — packet fields, session states, timers, modes and the pitfalls of aggressive intervals.
sources:
  - label: "RFC 5880 — Bidirectional Forwarding Detection (BFD)"
    url: https://www.rfc-editor.org/rfc/rfc5880
  - label: "RFC 5881 — BFD for IPv4 and IPv6 (Single Hop)"
    url: https://www.rfc-editor.org/rfc/rfc5881
  - label: "RFC 5883 — BFD for Multihop Paths"
    url: https://www.rfc-editor.org/rfc/rfc5883
  - label: "RFC 5884 / RFC 7130 / RFC 8971 — BFD for MPLS LSPs; BFD on LAG members (micro-BFD); BFD for VXLAN"
    url: https://www.rfc-editor.org/rfc/rfc7130
  - label: "RFC 7880 — Seamless BFD (S-BFD)"
    url: https://www.rfc-editor.org/rfc/rfc7880
---

Routing protocols detect neighbor failure by missing Hellos: OSPF's 40 s dead interval, BGP's hold time of 90–180 s. That is far too slow for modern applications. Lowering protocol timers burdens the control plane and still doesn't work for media that give no link-down signal (e.g. switches, optical gear, tunnels). **BFD** is a lightweight hello protocol that is independent of the routing protocols, media and encapsulation, and designed for hardware implementation.

## How it works

Two systems exchange BFD control packets over UDP at a negotiated rate. If no packet arrives for `detection time`, the session goes down and BFD notifies its **clients** (OSPF, IS-IS, BGP, static routes, VRRP, PIM, MPLS, ...), which immediately tear down the adjacency or remove the route, without waiting for their own timers.

| Transport | Port |
|---|---|
| Single-hop control (RFC 5881) | UDP **3784**, source port 49152–65535, **TTL/hop limit 255** (receivers verify) |
| Echo | UDP **3785** |
| Multihop (RFC 5883) | UDP **4784** |
| Micro-BFD on LAG (RFC 7130) | UDP 6784 |

### Control packet (24 bytes, optional authentication)

| Field | Size | Meaning |
|---|---|---|
| Version | 3 bits | 1 |
| Diagnostic | 5 | why the session last went down (0 none, 1 control detection time expired, 2 echo failed, 3 neighbor signaled down, 4 forwarding plane reset, 5 path down, 7 administratively down...) |
| State | 2 | 0 AdminDown, 1 Down, 2 Init, 3 Up |
| Flags | 6 | **P**oll, **F**inal, **C** (control plane independent), **A** (authentication present), **D** (demand mode), **M** (multipoint) |
| Detect Mult | 8 | multiplier for detection time |
| Length | 8 | |
| My Discriminator / Your Discriminator | 32 + 32 | identify the session at each end (so many sessions can share the same IP pair) |
| Desired Min TX Interval | 32 | µs the local system would like to use |
| Required Min RX Interval | 32 | µs the local system can accept |
| Required Min Echo RX Interval | 32 | 0 = no echo support |

### Session establishment and timers

A **three-way handshake** (Down → Init → Up) with discriminators prevents false positives. While not Up, packets are sent at ≥ 1 s intervals ("slow start"), until Up, when the negotiated fast rate is used.

- **Transmit interval** = `max(local Desired Min TX, remote Required Min RX)`.
- **Detection time** at the receiver = `remote Detect Mult × agreed remote transmit interval`.

Example: both sides at 300 ms and multiplier 3 → detect failure after 900 ms. At 50 ms × 3 → 150 ms. Parameters can be changed in-session using **Poll/Final** sequences without flapping.

### Modes

- **Asynchronous mode** (the common one): both ends send periodic packets.
- **Echo function**: one end sends packets that the other loops back in the forwarding plane (not seen by its BFD process); it tests forwarding without involving the peer's CPU. It runs at a higher rate while control packets slow down. Support varies, and it requires care with rp_filter/ICMP redirects.
- **Demand mode**: periodic packets are suppressed and a system checks only on demand; used for large numbers of sessions or links with their own detection.

## Using BFD

BFD registers with clients per neighbor:

```text
# FRR: bfdd profile and clients
bfd
 profile fast
  detect-multiplier 3
  transmit-interval 300
  receive-interval 300
 peer 192.0.2.2 interface eth0
  profile fast
!
router ospf
interface eth1
 ip ospf bfd
router bgp 65001
 neighbor 192.0.2.2 bfd profile fast
ip route 10.2.0.0/16 192.0.2.2 bfd 
```

```text
show bfd peers           # state, discriminators, intervals, diagnostics, counters
```

Typical settings: 300 ms × 3 on software platforms and campus, **50 ms × 3** on hardware-offloaded platforms (ASIC or line-card CPU) in a data center / provider core.

### Variants

- **BFD for LAG members** (micro-BFD, RFC 7130): one session per member link tests each physical member that LACP's 1–30 s timers can't detect quickly.
- **BFD for MPLS LSPs** (RFC 5884) and **multihop** sessions for loopback-to-loopback (eBGP multihop, iBGP): the detection reflects the IGP path.
- **BFD over VXLAN** (RFC 8971), **S-BFD** (RFC 7880: stateless reflector, no per-session negotiation, used in SR policies).

## Pitfalls

1. **Aggressive timers on a loaded CPU cause false downs**: a software BFD session at 50 ms needs real-time scheduling; a CPU spike or control-plane policing drop produces flaps that drop BGP/OSPF adjacencies, amplifying instability. Prefer hardware-assisted BFD for < 300 ms.
2. **Control-plane independence (C-bit)**: does BFD continue when the control plane restarts (graceful restart)? If C=1 and the forwarding plane is healthy, BFD must stay up while the routing process restarts. Combining GR and BFD requires matching settings, otherwise BFD declares the failure that GR wanted to mask.
3. **Asymmetric failures**: BFD detects one-way failures because both directions' packets are needed to stay Up.
4. **ACLs / CoPP** must permit UDP 3784/4784 from known peers, with TTL 255 enforcement.
5. **Scale**: hundreds of sessions at 50 ms multiply packet rates (each is 20 pps per direction); check platform limits.
6. **Does not detect everything**: BFD proves the path works for BFD packets; QoS, ACL or hashing issues that affect only data traffic are invisible, and with ECMP/LAG a single BFD flow tests only the path its hash selects.
7. **Security**: authenticate (keyed SHA1 / meticulous modes, RFC 5880 §6.7) for multihop, and rely on TTL 255 for single hop.

## Key takeaways

- BFD gives fast, protocol-agnostic failure detection with a simple UDP hello and three-way handshake; detection time = detect-mult × negotiated interval.
- Clients (OSPF, IS-IS, BGP, static, VRRP, MPLS, LAG) tear down state on BFD down; the timers are negotiated per session.
- Use hardware-offloaded BFD for ≤ 50 ms; software implementations should stay at several hundred ms to avoid false positives.
- Plan for GR interactions, CoPP/ACL permits, and LAG/ECMP coverage (micro-BFD).
