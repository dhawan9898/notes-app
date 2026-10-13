---
title: BFD — Bidirectional Forwarding Detection
track: networking
module: Resilience
moduleOrder: 6
order: 2
level: advanced
summary: "Sub-second failure detection independent of media and routing protocol: packet fields, session states, timers, modes, pitfalls, and a working BFD implementation run in a two-router lab (bring-up, silent-loss detection, timer negotiation, TTL 255 check)."
prerequisites:
  - "OSPFv2 Operation: Neighbors, DR/BDR, Flooding and SPF"
  - "BGP-4 Fundamentals: Sessions, Messages, Attributes and Best-Path Selection"
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

## Lab: a working BFD implementation

The program below implements the BFD asynchronous mode of RFC 5880 with the single-hop rules of RFC 5881: real 24-byte control packets, three-way state machine, slow start at 1 s, transmit interval and detection time computed from the peer's advertised values, 75-100 % jitter on the transmit timer, UDP port 3784 with a random source port in 49152-65535, TTL 255 on send and a TTL check on receive. It deliberately omits authentication, Poll/Final parameter changes, demand and echo modes. Two namespaces `r1` and `r2` each run one copy.

{{include networking/bfd/bfd.py}}

{{include networking/bfd/lab.sh}}

{{run networking/bfd :: unshare -Urnm sh lab.sh 2>&1}}

Reading the four scenarios (exact millisecond values vary from run to run because of timer jitter and process start-up):

1. **Bring-up** takes roughly one to two seconds, not 100 ms, because a session that is not Up sends only once per second (the "slow start" rule that stops a flapping neighbor from flooding the link). The states move Down, Init and Up through the discriminator handshake. Note the first `negotiated` line: right after the session comes Up the detection time is still 3000 ms, because it is computed from the **last packet received**, which still carried the peer's slow-start interval of 1 s. It drops to 300 ms (3 x 100 ms) as soon as a packet with the fast interval arrives. A real implementation uses Poll/Final to make both ends switch together; this one just follows the peer's packets.
2. **Failure** with the link still electrically up (a netem 100 % loss, the equivalent of a broken switch or a firewall silently dropping packets): `r1` declares `Up -> Down` with diagnostic 1 (control detection time expired) about 300 ms after the last good packet, in the neighbourhood of 4.2-4.4 s here, where the loss started at about 4.0 s. A routing protocol relying on its own hello timers would have needed 40 s (OSPF default dead interval) or up to 3 minutes (BGP hold time). Carrier-based detection would not have noticed at all.
3. **Timer negotiation**: `r1` wants to send every 50 ms but `r2` says it can receive only every 400 ms, so `r1` transmits every 400 ms (`max(own desired, remote required RX)`). `r2` wants 200 ms and `r1` accepts 50 ms, so `r2` sends every 200 ms. The detection times are asymmetric as a result: `r1` waits 5 x 200 ms = 1000 ms for `r2`, while `r2` waits 3 x 400 ms = 1200 ms for `r1`. The multiplier belongs to the **sender**, the interval to the agreement between the two.
4. **TTL 255 check**: packets sent with TTL 64 are discarded on receive, so `r1` never leaves Down and counts the discards. A device that is not directly attached cannot craft a packet that arrives with TTL 255, which is the entire security argument of RFC 5881 (the Generalized TTL Security Mechanism) and why BFD needs no authentication on a single hop but does on multihop sessions (RFC 5883).

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
