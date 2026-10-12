---
title: IPv4 Header, Fragmentation and MTU
track: networking
module: IPv4
moduleOrder: 3
order: 1
level: intermediate
summary: Every field of the IPv4 header and what routers do with it, the header checksum, how fragmentation works byte by byte, and why modern networks avoid it.
prerequisites:
  - What Is a Network?
  - Binary, Hexadecimal and Bits
sources:
  - label: "RFC 791 — Internet Protocol"
    url: https://www.rfc-editor.org/rfc/rfc791
  - label: "RFC 1071 / RFC 1624 — Computing the Internet Checksum / incremental update"
    url: https://www.rfc-editor.org/rfc/rfc1624
  - label: "RFC 2474 / RFC 3168 — DSCP field / ECN"
    url: https://www.rfc-editor.org/rfc/rfc3168
  - label: "RFC 6864 — Updated Specification of the IPv4 ID Field"
    url: https://www.rfc-editor.org/rfc/rfc6864
  - label: "RFC 8900 — IP Fragmentation Considered Fragile"
    url: https://www.rfc-editor.org/rfc/rfc8900
  - label: "RFC 1191 / RFC 4821 / RFC 8899 — Path MTU Discovery, PLPMTUD, DPLPMTUD"
    url: https://www.rfc-editor.org/rfc/rfc1191
---

## Header layout

The IPv4 header is 20 bytes plus options (up to 60 bytes in total). All multi-byte fields are in **network byte order** (big endian).

```text
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-------+-------+-----------+---+-------------------------------+
|Version|  IHL  |   DSCP    |ECN|         Total Length          |
+-------+-------+-----------+---+-------------------------------+
|        Identification         |Flags|      Fragment Offset     |
+---------------+---------------+-----+--------------------------+
|      TTL      |   Protocol    |        Header Checksum         |
+---------------+---------------+--------------------------------+
|                       Source Address                           |
+----------------------------------------------------------------+
|                    Destination Address                         |
+----------------------------------------------------------------+
|                    Options (if IHL > 5)                        |
+----------------------------------------------------------------+
```

| Field | Bits | Details |
|---|---|---|
| Version | 4 | `4` |
| IHL | 4 | header length in 32-bit words; min 5 (20 B), max 15 (60 B). Routers use it to find the payload |
| DSCP | 6 | Differentiated Services Code Point (RFC 2474): per-hop QoS class, e.g. EF = 46, AF41 = 34, CS6 = 48 |
| ECN | 2 | explicit congestion notification: `00` not-ECT, `01`/`10` ECT, `11` CE (congestion experienced) |
| Total Length | 16 | header + payload in bytes; max 65,535 |
| Identification | 16 | groups fragments of one datagram |
| Flags | 3 | bit 0 reserved (0); **DF** = Don't Fragment; **MF** = More Fragments |
| Fragment Offset | 13 | position of this fragment's data, in **8-byte units** |
| TTL | 8 | decremented at each router; 0 → discard + ICMP Time Exceeded |
| Protocol | 8 | upper-layer protocol (1 ICMP, 6 TCP, 17 UDP, 41 IPv6-in-IPv4, 47 GRE, 50 ESP, 51 AH, 89 OSPF, 103 PIM, 112 VRRP) |
| Header Checksum | 16 | covers **the header only** |
| Source / Destination | 32 each | |

The old "Type of Service" byte was redefined as DSCP+ECN. Initial TTL values differ by OS (Linux 64, Windows 128, many network devices 255), so the TTL you observe hints at the sender and hop count.

### The header checksum

The 16-bit one's-complement of the one's-complement **sum of all 16-bit words** in the header (checksum field set to 0 while computing). A receiver sums the entire header including the checksum; a valid header yields `0xFFFF`.

Worked example (header `45 00 00 73 00 00 40 00 40 11 [chk] c0 a8 00 01 c0 a8 00 c7`):

```text
4500 + 0073 + 0000 + 4000 + 4011 + 0000 + c0a8 + 0001 + c0a8 + 00c7 = 0x2479C
fold the carry:  0x479C + 0x0002 = 0x479E
invert:          ~0x479E = 0xB861                      ← the checksum
```

Because each router decrements TTL (one byte of one word), the checksum is updated **incrementally** (RFC 1624: `HC' = ~(~HC + ~m + m')`) instead of recomputed. IPv6 has *no* header checksum, since link-layer CRCs and transport checksums already protect the data and removing it speeds up forwarding. TCP and UDP checksums include a **pseudo-header** (source, destination, protocol, length), which is why NAT must rewrite them.

### Options (rarely used)

Present when IHL > 5. Examples: Record Route, Timestamp, Loose/Strict Source Route, Router Alert (RFC 2113, used by IGMP, RSVP). Packets with options are often **processed in the slow path** (CPU), so many networks drop them; source routing is routinely blocked as a security risk.

## Fragmentation

If a packet is larger than the **MTU** of the outgoing link, the router can either fragment (DF = 0) or drop it and report ICMP "Fragmentation needed and DF set" (DF = 1).

Rules (RFC 791):

- All fragments share the original **Identification**, source, destination and protocol.
- Each fragment has its own full IP header. Fragment data (except the last) must be a **multiple of 8 bytes**, because the offset is in 8-byte units.
- **MF = 1** on all fragments except the last; the last has MF = 0 and a nonzero offset.
- Fragments are **reassembled only at the destination**, never in transit. Fragments can take different paths or arrive out of order, and may be fragmented again.

### Worked example

A 4000-byte datagram (20 B header + 3980 B data) crosses a link with MTU 1500:

| Fragment | Total Length | Data bytes | MF | Offset field | Data range |
|---|---|---|---|---|---|
| 1 | 1500 | 1480 | 1 | 0 | 0–1479 |
| 2 | 1500 | 1480 | 1 | 185 (= 1480/8) | 1480–2959 |
| 3 | 1040 | 1020 | 0 | 370 | 2960–3979 |

Data per fragment = `floor((1500 − 20) / 8) × 8 = 1480`. The receiver collects all pieces with the same `(src, dst, protocol, ID)` and starts a **reassembly timer** (Linux `net.ipv4.ipfrag_time` = 30 s). If a fragment is lost, the whole datagram is dropped after the timeout, with ICMP Time Exceeded code 1 (reassembly time exceeded) if the first fragment (offset 0) was received.

### Why fragmentation is fragile (RFC 8900)

- **Loss amplification**: losing one of *n* fragments loses the whole datagram; TCP then retransmits the whole segment.
- **L4 header only in the first fragment**: firewalls, load balancers and ECMP hashes (which use ports) cannot classify non-first fragments, so they are often dropped or hashed to a different path. Stateless filters must reassemble or pass them.
- **Reassembly cost and state**: memory exhaustion attacks (fragment floods), overlapping-fragment evasion (Teardrop, Rose, tiny fragment attacks), and "ping of death" bugs. Hosts bound the reassembly buffers (`ipfrag_high_thresh` / `ipfrag_low_thresh`).
- **ID collisions**: a 16-bit ID wraps quickly on fast links (65,536 datagrams in a few ms at 10 Gb/s), so fragments of different datagrams can be mixed: data corruption that the transport checksum catches only most of the time. RFC 6864 forbids fragmenting "atomic" datagrams (DF = 1, offset 0, MF = 0) and removes the uniqueness requirement for them.
- **UDP/DNS** over fragmentation, EDNS(0) buffer sizes ≈ 1232 B (DNS Flag Day 2020), to avoid it.

### Path MTU Discovery (PMTUD, RFC 1191)

Hosts avoid fragmentation by sending with **DF = 1** and learning the smallest MTU along the path: a router that cannot forward replies with ICMP type 3 code 4, containing the **next-hop MTU** in the lower 16 bits of the "unused" field. The sender lowers its cached path MTU (Linux keeps it in the route cache exceptions, `ip route get` shows `mtu`) and retransmits smaller packets. TCP then uses an MSS derived from the new MTU.

**The PMTUD black hole**: if a firewall drops the ICMP errors, the sender never learns the limit: small packets (TCP handshake, ping) work, large ones hang. This is the classic symptom of tunnels, PPPoE (MTU 1492), VPNs and IPsec. Mitigations:

- Allow ICMP type 3 code 4 (and IPv6 Packet Too Big) everywhere, as RFC 4890/7126 recommend.
- **MSS clamping** on the tunnel endpoint: `iptables -t mangle -A FORWARD -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu`.
- **PLPMTUD** (RFC 4821, and DPLPMTUD RFC 8899 for UDP/QUIC): probe with progressively larger packets using transport-layer acknowledgments, without depending on ICMP. Linux: `net.ipv4.tcp_mtu_probing` = 1 or 2.
- Choose consistent MTUs in the network and account for encapsulation overhead.

```bash
ping -M do -s 1472 8.8.8.8            # DF set; 1472 + 8 + 20 = 1500
tracepath 8.8.8.8                     # reports path MTU hop by hop
ip route get 8.8.8.8                  # includes cached "mtu" if learned
nstat -az | egrep 'Frag|Reasm'        # IpFragCreates, IpReasmFails, ...
```

Minimum guarantees: every IPv4 host must accept **576 bytes** datagrams reassembled (RFC 791 §3.1: 68 B minimum link MTU, but hosts must accept 576). IPv6 raised the minimum link MTU to 1280 and *forbids* router fragmentation.

## What the router does with the header (summary)

Validate version, IHL ≥ 5, total length ≤ frame, checksum → check destination (local? forward?) → TTL − 1 (drop at 0) → incremental checksum → LPM lookup → MTU check/fragment/ICMP → L2 rewrite. Packets with options, TTL ≤ 1, or addressed to the router are punted to the CPU.

## Key takeaways

- Header: 20 bytes minimum; IHL in words; Total Length, DF/MF and 8-byte-unit fragment offsets drive fragmentation.
- The checksum covers the header only and is updated incrementally after TTL changes; IPv6 removed it.
- Fragments reassemble only at the destination; losing one loses all, and non-first fragments lack L4 information.
- Rely on PMTUD (DF=1 + ICMP type 3/4) or PLPMTUD, never block the needed ICMP, and clamp MSS on tunnels.
