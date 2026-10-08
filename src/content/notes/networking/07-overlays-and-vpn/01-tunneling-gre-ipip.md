---
title: Tunneling Fundamentals — GRE, IP-in-IP and GRE-in-UDP
track: networking
module: Overlays and VPN
moduleOrder: 7
order: 1
level: intermediate
summary: How encapsulation tunnels work, GRE header options, MTU/MSS and recursive-routing pitfalls, mGRE/DMVPN, and Linux tunnel configuration.
sources:
  - label: "RFC 2784 / RFC 2890 — Generic Routing Encapsulation (GRE); Key and Sequence Number Extensions"
    url: https://www.rfc-editor.org/rfc/rfc2890
  - label: "RFC 2003 — IP Encapsulation within IP; RFC 4213 — IPv6-in-IPv4 (protocol 41)"
    url: https://www.rfc-editor.org/rfc/rfc2003
  - label: "RFC 8086 — GRE-in-UDP Encapsulation"
    url: https://www.rfc-editor.org/rfc/rfc8086
  - label: "RFC 2332 / RFC 2735 — NBMA Next Hop Resolution Protocol (NHRP)"
    url: https://www.rfc-editor.org/rfc/rfc2332
  - label: "RFC 4459 — MTU and Fragmentation Issues with In-the-Network Tunneling"
    url: https://www.rfc-editor.org/rfc/rfc4459
  - label: "Linux — ip-tunnel(8), ip-link(8) (gre, gretap, ipip, sit, vti)"
    url: https://man7.org/linux/man-pages/man8/ip-link.8.html
---

A **tunnel** carries one protocol's packets inside another's: the *payload* (passenger) packet is wrapped with a *delivery* (outer) header and unwrapped at the far end. Tunnels create **virtual point-to-point links** across an underlay, to carry non-IP protocols, multicast, overlapping address space or IPv6 over IPv4, and to build VPNs or overlays.

```text
Passenger:  | IP(A→B) | payload |
Tunnel:     | outer IP(R1→R2, proto 47) | GRE | IP(A→B) | payload |
```

## Encapsulations

| Protocol | IP proto / transport | Overhead | Carries | Notes |
|---|---|---|---|---|
| **IP-in-IP** (RFC 2003) | 4 | 20 B (outer IPv4 header) | IPv4 only | simplest; no multicast/keys |
| **IPv6-in-IPv4 (6in4/SIT)** | 41 | 20 B | IPv6 | tunnel brokers, 6rd |
| **IPv4/IPv6 over IPv6 (ip6tnl)** | 4 or 41 inside IPv6 | 40 B | | DS-Lite, MAP-E |
| **GRE** (RFC 2784) | 47 | 24 B (20 + 4) | almost anything: IPv4, IPv6, MPLS, Ethernet | optional key/sequence/checksum add 4 B each |
| **GRE-in-UDP** (RFC 8086) | UDP port 4754 | 20 + 8 + 4 | as GRE | UDP source port gives **ECMP entropy** and passes NAT |
| **VXLAN / Geneve** | UDP 4789 / 6081 | ~50 B | Ethernet | covered in the VXLAN note |
| **IPsec** | ESP 50 / UDP 4500 | 50–70 B | IP | provides security (next note) |
| **L2TPv3, PPTP, MPLS-over-X** | various | | | legacy / specialized |

## GRE in detail

Header: 4 mandatory bytes, then optional fields indicated by flag bits.

```text
 0                   1                   2                   3
+-+-+-+-+-----------------+-----+-------------------------------+
|C|R|K|S|s| Recur |Flags | Ver |       Protocol Type            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-------------------------------+
|      Checksum (optional)       |       Reserved1 (optional)    |
|                        Key (optional)                          |
|                 Sequence Number (optional)                     |
```

- **Protocol Type** is the EtherType of the passenger: `0x0800` IPv4, `0x86DD` IPv6, `0x8847` MPLS, `0x6558` **Transparent Ethernet Bridging** (used by `gretap` to carry L2 frames).
- **K (Key)** flag, RFC 2890: a 32-bit key used to *demultiplex* multiple tunnels between the same endpoints or to add basic anti-injection hygiene (not a secret).
- **S (Sequence)**: allows in-order delivery detection.
- **C (Checksum)**: rarely used.
- GRE has **no authentication, no encryption**, and no inherent liveness mechanism. Anyone who can inject packets with the right outer addresses can inject into the tunnel; filter protocol 47 at the edge and combine with IPsec (GRE over IPsec) when crossing untrusted networks.

### Why GRE over IPsec?

Plain IPsec tunnel mode is a point-to-point *policy* mechanism and does not carry multicast or dynamic routing protocols in a natural way. GRE gives a routable interface that supports **multicast, OSPF/BGP and non-IP traffic**; IPsec (usually in transport mode, to avoid double encapsulation) encrypts the GRE packets. This is the foundation of DMVPN.

### mGRE, NHRP and DMVPN

A **multipoint GRE** interface has a single tunnel interface with many remote endpoints, resolved dynamically by **NHRP**: spokes register their NBMA (public) address with a hub (NHS), and ask it for other spokes' addresses to build **direct spoke-to-spoke tunnels** on demand. Combined with IPsec and a routing protocol (BGP/OSPF/EIGRP), this is **DMVPN** (Cisco, and implementations like FRR NHRPD + strongSwan). It avoids the n² static tunnels of a full mesh.

## MTU and fragmentation

Encapsulation adds overhead, so the tunnel's MTU is smaller: `tunnel MTU = path MTU − overhead` (1500 − 24 = **1476** for GRE over IPv4; Linux sets this automatically unless told otherwise).

The painful cases:

- The **outer** packet exceeds the underlay path MTU: fragmentation of the outer packet (if DF is clear) or a drop with ICMP "frag needed" sent to the tunnel source, which must translate it into an ICMP error to the original sender (RFC 4459). Many tunnel implementations cannot or do not.
- PMTUD **black holes**: users can ping but HTTPS stalls.

Mitigations: set the tunnel MTU explicitly (1400 is a common safe value); **clamp TCP MSS** to `MTU − 40` on the tunnel interface (`iptables -t mangle -A FORWARD -o gre1 -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu`); copy DF from the inner packet or set `pmtudisc` appropriately; ensure the underlay forwards 1500+ (jumbo) when possible.

## Recursive routing

The tunnel's destination (the far endpoint's underlay address) **must be reachable via the underlay, not via the tunnel itself**. If a routing protocol over the tunnel learns a route to the remote endpoint's own address (or a covering prefix) via the tunnel interface, the router will try to send the encapsulated packets into the tunnel: the tunnel goes down, the route disappears, the tunnel comes up... a flapping loop (log message: `%TUN-5-RECURDOWN`). Prevent with route filters, longer-prefix routes for tunnel endpoints, or by placing tunnels in a separate VRF ("front-door VRF").

## Other properties

- **TTL**: outer TTL may be fixed (e.g. 64) or inherited. The inner TTL is decremented once per tunnel traversal (one hop), so traceroute sees the tunnel as a single hop.
- **Keepalives**: GRE has none natively; Cisco emits GRE keepalives (self-addressed inner packets), Linux relies on routing protocol/BFD, or the tunnel stays "up" regardless of reachability (it's stateless). Use BFD or monitoring scripts.
- **ECMP hashing**: all tunnel packets between two endpoints share the same outer 5-tuple (protocol 47 with no ports), hashing onto **one path**: no load balancing on the underlay. Fix with **GRE-in-UDP** (entropy in the source port) or by using multiple tunnels.
- **QoS**: copy DSCP/ECN from inner to outer (`tos inherit`) so the underlay can classify; ECN handling rules in RFC 6040.
- **Security**: spoofed GRE packets; unauthorized tunnel endpoints; use uRPF, ACLs for protocol 47, and IPsec.

## Linux examples

```bash
# GRE (L3) between 198.51.100.1 and 203.0.113.9
ip link add gre1 type gre local 198.51.100.1 remote 203.0.113.9 ttl 64 key 42
ip addr add 10.99.0.1/30 dev gre1
ip link set gre1 up mtu 1400
ip route add 10.2.0.0/16 dev gre1

# GRETAP: carries Ethernet frames, can be bridged
ip link add gt1 type gretap local 198.51.100.1 remote 203.0.113.9
ip link add br0 type bridge; ip link set gt1 master br0; ip link set eth1 master br0
ip link set gt1 up; ip link set br0 up

# IP-in-IP, 6in4 (sit)
ip link add ipip1 type ipip local 198.51.100.1 remote 203.0.113.9
ip link add sit1 type sit local 198.51.100.1 remote 203.0.113.9

# GRE over UDP (FOU) for ECMP entropy
ip fou add port 5555 ipproto 47
ip link add gre2 type gre local 198.51.100.1 remote 203.0.113.9 encap fou encap-sport auto encap-dport 5555
```

Verify: `ip -d link show gre1`, `tcpdump -ni eth0 proto gre` (shows outer + inner), `ip -s link show gre1` (tunnel counters).

## Choosing a tunnel type

| Need | Use |
|---|---|
| Simple IPv4 site-to-site, no security needed | IP-in-IP or GRE |
| Multicast/routing/non-IP across a network | GRE |
| Encryption | IPsec (VTI/xfrm) or GRE over IPsec; WireGuard for simple cases |
| Many sites, dynamic spoke-to-spoke | DMVPN (mGRE + NHRP + IPsec) or EVPN/VXLAN |
| Extend L2 | gretap (small scale), VXLAN/EVPN (data center), L2TPv3 |
| ECMP over the underlay | GRE-in-UDP, VXLAN, Geneve |

## Key takeaways

- A tunnel adds an outer header; overhead (20–24 B for IP-in-IP/GRE) lowers the MTU: set MTU, clamp MSS, and allow ICMP.
- GRE is multi-protocol but unauthenticated and unencrypted; pair with IPsec for security, and use mGRE/NHRP for dynamic meshes.
- Never let the tunnel endpoint be reachable through the tunnel (recursive routing).
- Single outer 5-tuple means a single underlay path; use UDP encapsulation for ECMP entropy.
