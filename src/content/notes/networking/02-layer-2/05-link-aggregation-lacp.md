---
title: Link Aggregation (802.1AX / LACP) and Load Balancing
track: networking
module: Layer 2
moduleOrder: 2
order: 5
level: advanced
summary: "How parallel links are bundled into one logical link, how LACP negotiates membership (with a decoded real LACPDU), how flows are hashed (measured per policy), failover behavior, a static-vs-LACP misconfiguration, and where MLAG fits."
prerequisites:
  - Ethernet Frames and MAC Learning
sources:
  - label: "IEEE 802.1AX-2020 — Link Aggregation (LACP, distribution/collection, DRNI)"
  - label: "IEEE 802.3-2022 Annex 57A — Slow Protocols (LACP carried as a slow protocol)"
  - label: "Linux kernel docs — Linux Ethernet Bonding Driver HOWTO"
    url: https://docs.kernel.org/networking/bonding.html
  - label: "RFC 2992 — Analysis of an Equal-Cost Multi-Path Algorithm"
    url: https://www.rfc-editor.org/rfc/rfc2992
---

**Link aggregation** (LAG, port-channel, bond, team, EtherChannel) combines several physical links into one logical link between two systems, giving more bandwidth and resilience. The logical link looks like a single port to the layers above, so **STP sees one link** and does not block members.

The standard is IEEE **802.1AX** (previously 802.3ad, which is why Linux still calls the mode `802.3ad`). It defines the **Link Aggregation Control Protocol (LACP)** and the rules for distributing frames across members.

## Rules the aggregation must obey

1. **Frame order must be preserved per conversation.** The distributor assigns each flow ("conversation") to exactly one member link, so frames of one flow never reorder. Therefore a LAG does not make a *single flow* faster than one member.
2. Members must have the **same speed and duplex**, and belong to the same system on each end (aside from DRNI/MLAG).
3. Frames are distributed with no changes: no header added, no loops: a **collector** on the receive side merges the streams.

## LACP

LACPDUs are **Slow Protocols** frames: EtherType `0x8809`, subtype `0x01`, destination `01-80-C2-00-00-02` (not forwarded by bridges). Each end ("actor") describes itself and its view of the partner:

| Actor info | Meaning |
|---|---|
| System ID | system priority (2 B, default 32768) + MAC |
| Key | an administrative value; ports with the same key on a system *may* aggregate |
| Port ID | port priority + port number |
| State | 8 flags (below) |

State flags: **Activity** (active=1: sends LACPDUs periodically; passive=0: only replies), **Timeout** (1 = short, 0 = long), **Aggregation** (can aggregate), **Synchronization** (in-sync with the partner), **Collecting**, **Distributing**, **Defaulted** (using default partner info), **Expired**.

- **Active/passive**: a LAG forms if at least one side is active. Both passive → nothing forms. Prefer *active* on both ends.
- **Timers**: fast = LACPDU every 1 s, timeout 3 s; slow = every 30 s, timeout 90 s. Fast gives quicker detection of a link whose partner has stopped, but physical link-down is detected immediately regardless.
- Members are only used for traffic once both sides report `Synchronization`, then `Collecting` and `Distributing`. This **bring-up handshake** is the main benefit over *static* (mode on) aggregation: a miscabled or misconfigured member is never put into service.
- With **static LAG (no LACP)**, one side mis-bundled → frames loop or black-hole with no protocol to notice.
- **Selection**: when more members than the system supports (usually 8 active) exist, the system with the **lower System ID** decides which ports are active, ordering by port priority; the rest are *standby* ("hot-standby").
- `min-links` / minimum active links: bring the whole LAG down if too few members remain so upper layers (routing) reroute instead of squeezing traffic into one link.

## Distribution (hashing)

The sender picks a member by hashing header fields of each frame, then `hash mod N`. Common inputs:

| Policy | Hash inputs | Use |
|---|---|---|
| L2 | src/dst MAC | simple; terrible for hosts behind one router (all frames use one router MAC) |
| L2+L3 | MACs + src/dst IP | good default |
| L3+L4 | IPs + TCP/UDP ports | best spread; non-first fragments hash differently (no L4 header) |
| Encapsulation-aware | inner headers of VXLAN/GRE/MPLS | required in overlays, otherwise all tunnel traffic hashes to one link |

Because it is a hash, balance is statistical. With few flows (or one elephant flow) utilization is lopsided: 2 flows on 4 links can leave two links idle. Remedies: more entropy in the hash (L4 ports), more flows, or use ECMP at L3 with per-flow hashing.

**Hash polarization**: if every tier of a multi-stage network uses the same hash function and seed, then flows that chose link A at the first tier all choose the same link at the next tier, concentrating traffic. Vendors offer per-device hash seeds to avoid it.

## MLAG: aggregation across two switches

Plain 802.1AX requires both ends to be single systems. For switch redundancy, vendors implement **Multi-Chassis LAG** (Cisco vPC, Arista MLAG, Juniper ICCP/EVPN-LAG, Cumulus CLAG). A pair of peer switches present themselves to the server as one LACP system (same System ID), synchronize MAC/ARP state over a **peer link**, and let the server dual-home with active-active forwarding. Characteristics:

- Not an interoperable standard (802.1AX-2014's **DRNI** specified one but is rarely implemented); both peers must be from the same vendor.
- Needs care for split-brain (peer link down, keepalive over a separate path) and consistent configuration (VLANs, STP).
- EVPN multihoming (RFC 7432 Ethernet Segments, all-active) is the standards-based replacement in routed fabrics.

## Linux bonding

```bash
ip link add bond0 type bond mode 802.3ad miimon 100 lacp_rate fast xmit_hash_policy layer3+4
ip link set eth0 down; ip link set eth0 master bond0
ip link set eth1 down; ip link set eth1 master bond0
ip link set bond0 up
cat /proc/net/bonding/bond0     # shows LACP actor/partner info, aggregator ID, per-slave state
```

| Mode | Behavior | Needs switch support |
|---|---|---|
| 0 balance-rr | round-robin per packet (reorders!) | static LAG |
| 1 active-backup | one active NIC, failover | none |
| 2 balance-xor | hash-based, static | static LAG |
| 3 broadcast | duplicates each frame | static |
| 4 802.3ad | LACP | LACP |
| 5 balance-tlb | adaptive TX balance | none |
| 6 balance-alb | adaptive TX + RX balance via ARP tricks | none |

For anything serious use mode 1 (simplest, no switch config) or mode 4 (LACP).  `ethtool -i bond0` and `ip -d link show bond0` give you the settings; `ip -s link` per-member counters reveal hash skew.

## Lab: two LACP switches on one Linux host

Two network namespaces play two switches, `a` and `b`, joined by two veth links, with a Linux `802.3ad` bond on each side. The script shows the negotiated aggregator, captures and decodes the LACPDU that `a` sends, counts which member carries each flow under three hash policies, takes a member down, and finally connects an LACP bond to a static bundle.

```bash
#!/bin/sh
# Two "switches" (namespaces a and b) joined by two veth links, each side running a Linux 802.3ad (LACP) bond.
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
ip netns add a; ip netns add b
for i in 1 2; do ip link add a$i type veth peer name b$i; ip link set a$i netns a; ip link set b$i netns b; done
mkbond() { # ns policy
    ip -n $1 link add bond0 type bond mode 802.3ad lacp_rate fast xmit_hash_policy $2 miimon 100
    for i in 1 2; do ip -n $1 link set $1$i down; ip -n $1 link set $1$i master bond0; done
    ip -n $1 link set bond0 up
}
mkbond a layer3+4; mkbond b layer3+4
ip -n a addr add 10.0.0.1/24 dev bond0; ip -n b addr add 10.0.0.2/24 dev bond0
sleep 4
echo '== negotiated state, as seen by switch a =='
ip netns exec a awk '/^Slave Interface/{s=$3} /MII Status/{if(s!="")print s ": link " $3} /Aggregator ID/{print s ": aggregator ID " $3}' /proc/net/bonding/bond0
echo; echo '== the LACPDU that switch a sends on a1 =='
ip netns exec a python3 lacpdu.py a1 4

echo; echo '== distribution: 64 UDP flows a -> b with different source ports, packets sent on each member =='
flows() { ip netns exec a python3 -c "
import socket
for i in range(64):
    s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.bind(('',30000+i)); s.sendto(b'x',('10.0.0.2',9)); s.close()"; }
txp() { for i in 1 2; do ip -n a -s -j link show a$i | python3 -c "import json,sys; print(json.load(sys.stdin)[0]['stats64']['tx']['packets'], end=' ')"; done; echo; }
ip netns exec a ping -c 1 -W 2 10.0.0.2 > /dev/null          # warm-up: ARP
for pol in layer2 layer2+3 layer3+4; do
    ip -n a link set bond0 down; ip -n a link set bond0 type bond xmit_hash_policy $pol; ip -n a link set bond0 up; sleep 3
    before=$(txp); flows; after=$(txp)
    python3 -c "b='$before'.split(); a='$after'.split(); print('%-10s a1: %d  a2: %d' % ('$pol', int(a[0])-int(b[0]), int(a[1])-int(b[1])))"
done

echo; echo '== failure: link a1 goes down while pinging =='
ip -n a link set bond0 down; ip -n a link set bond0 type bond xmit_hash_policy layer3+4; ip -n a link set bond0 up; sleep 3
ip netns exec a ping -c 20 -i 0.1 -W 1 10.0.0.2 > /tmp/ping.$$ &
sleep 0.5; ip -n a link set a1 down; wait
grep -E "transmitted" /tmp/ping.$$; rm -f /tmp/ping.$$
sleep 2
ip netns exec a awk '/^Slave Interface/{s=$3} /MII Status/{if(s!="")print s ": link " $3}' /proc/net/bonding/bond0
echo -n "flows after failure:  a1/a2 packets sent: "; before=$(txp); flows; after=$(txp)
python3 -c "b='$before'.split(); a='$after'.split(); print('%d / %d' % (int(a[0])-int(b[0]), int(a[1])-int(b[1])))"
ip -n a link set a1 up; sleep 4
echo -n "after recovery: "; ip netns exec a awk '/Aggregator ID/{printf "%s ", $3} END{print "(aggregator IDs of the two members)"}' /proc/net/bonding/bond0

echo; echo '== misconfiguration: switch c runs LACP, switch d has a static bundle (balance-xor, no LACP) =='
ip netns add c; ip netns add d
for i in 1 2; do ip link add c$i type veth peer name d$i; ip link set c$i netns c; ip link set d$i netns d; done
mk2() { ip -n $1 link add bond0 type bond mode $2 miimon 100 $3; for i in 1 2; do ip -n $1 link set $1$i down; ip -n $1 link set $1$i master bond0; done; ip -n $1 link set bond0 up; }
mk2 c 802.3ad "lacp_rate fast"; mk2 d balance-xor ""
ip -n c addr add 10.0.1.1/24 dev bond0; ip -n d addr add 10.0.1.2/24 dev bond0; sleep 6
ip netns exec c awk '/^Slave Interface/{s=$3} /Aggregator ID/{print s ": aggregator ID " $3}' /proc/net/bonding/bond0
echo "two different IDs: LACP never saw a partner, so each port is its own aggregator and only one carries traffic"
ip netns exec c ping -c 2 -W 1 10.0.1.2 | grep transmitted
```

The LACPDU decoder reuses the ring-buffer capture from the packet-capture note:

```python
#!/usr/bin/env python3
"""Capture one LACPDU leaving an interface and decode it: lacpdu.py IFACE SECONDS"""
import struct, sys
from ring import Tap

def state(b):
    names = ["Activity", "Timeout(short)", "Aggregation", "Synchronization", "Collecting", "Distributing", "Defaulted", "Expired"]
    return ",".join(n for i, n in enumerate(names) if b >> i & 1)

def info(tlv):
    prio, mac, key, pprio, port, st = struct.unpack("!H6sHHHB", tlv[:15])
    return f"system priority {prio}, key {key}, port priority {pprio}, port number {port}, state 0x{st:02x} [{state(st)}]"

for kind, pkt, ts in Tap(sys.argv[1]).packets(float(sys.argv[2])):
    dst, src, etype = pkt[:6], pkt[6:12], struct.unpack("!H", pkt[12:14])[0]
    if etype != 0x8809 or pkt[14] != 1: continue                                    # Slow Protocols, subtype 1 = LACP
    print("frame: dst", ":".join(f"{b:02x}" for b in dst), f"ethertype 0x{etype:04x}, subtype {pkt[14]}, version {pkt[15]}, {len(pkt)} bytes")
    p = 16
    while p < len(pkt) and pkt[p]:
        t, l = pkt[p], pkt[p + 1]; body = pkt[p + 2:p + l]
        if t == 1: print("  actor   :", info(body))
        elif t == 2: print("  partner :", info(body))
        elif t == 3: print("  collector max delay:", struct.unpack("!H", body[:2])[0])
        p += l
    break
else:
    print("no LACPDU seen")
```

```text
$ unshare -Urnm sh lab.sh 2>&1
== negotiated state, as seen by switch a ==
a1: link up
a1: aggregator ID 1
a2: link up
a2: aggregator ID 1

== the LACPDU that switch a sends on a1 ==
frame: dst 01:80:c2:00:00:02 ethertype 0x8809, subtype 1, version 1, 124 bytes
  actor   : system priority 65535, key 15, port priority 255, port number 1, state 0x3f [Activity,Timeout(short),Aggregation,Synchronization,Collecting,Distributing]
  partner : system priority 65535, key 15, port priority 255, port number 1, state 0x3f [Activity,Timeout(short),Aggregation,Synchronization,Collecting,Distributing]
  collector max delay: 0

== distribution: 64 UDP flows a -> b with different source ports, packets sent on each member ==
layer2     a1: 65  a2: 0
layer2+3   a1: 0  a2: 64
layer3+4   a1: 34  a2: 32

== failure: link a1 goes down while pinging ==
20 packets transmitted, 20 received, 0% packet loss, time 1986ms
a1: link down
a2: link up
flows after failure:  a1/a2 packets sent: 0 / 64
after recovery: 1 1 (aggregator IDs of the two members)

== misconfiguration: switch c runs LACP, switch d has a static bundle (balance-xor, no LACP) ==
c1: aggregator ID 1
c2: aggregator ID 2
two different IDs: LACP never saw a partner, so each port is its own aggregator and only one carries traffic
2 packets transmitted, 2 received, 0% packet loss, time 1021ms
```

What to take from the output:

- **Negotiation**: both members of `a` report the same aggregator ID, so they form one logical link. The decoded LACPDU is a 124-byte frame to the slow-protocols address `01:80:c2:00:00:02`, subtype 1. The actor and partner blocks carry the system priority (default 65535), the **key** (which ports may aggregate together, derived from speed and duplex), the port number, and the state byte. `0x3f` means Activity, short Timeout, Aggregation, Synchronization, Collecting and Distributing are all set: this port is fully in the bundle and passing traffic in both directions.
- **Hash policy** decides balance. With `layer2` (source and destination MAC) every flow between the same two hosts hits the same member: all 64 flows land on one link (which one depends on the MAC addresses, so it differs between runs). `layer2+3` adds IP addresses, which are also identical for these 64 flows, so again one member. Only `layer3+4`, which includes the UDP ports, spreads them about evenly. The one or two extra packets on a member are ARP or LACPDU related. This is the same rule as for ECMP: a single conversation never uses more than one member, and balance appears only with many distinct flows and a hash that sees what distinguishes them.
- **Failure**: with `miimon`, losing a member's carrier is noticed immediately; the aggregate stays up and the flows rehash onto the remaining member (all 64 flows now leave on one link). Whether the 20 pings during the failure lose any packets depends on whether their flow was on the failed member and on timing (the run above lost none); traffic that was in flight on the failed link is lost, then everything continues on the survivor. When the link returns, LACP rebuilds the aggregate (both ports back in aggregator 1).
- **Misconfiguration**: when one side is LACP and the other a static bundle, no LACPDUs ever arrive, so the LACP side cannot find a partner and keeps each port in its own aggregator (IDs 1 and 2); only one is used. Whether the final ping succeeds is a coin toss that differs between runs: it works only when the link the LACP side picked is also the one the static side's hash sends replies on, and the static side may send frames on both links while the LACP side listens on one. That is the "frames vanish at random" symptom from the troubleshooting list, and why a static bundle against an LACP peer must never be left in production.

## Troubleshooting

- Members stay "individual"/down: speed/duplex mismatch, mismatched key/port-channel number, both ends passive, or LACPDUs filtered by an intermediate device.
- One member overloaded: check hash policy; use `ethtool -S` counters on each member.
- Frames disappear at random: static bundle on one end, nothing on the other.
- Unexpected MAC flapping when a server uses mode 0/2/5/6 against a non-LAG switch: the same MAC appears on multiple ports.

## Key takeaways

- A LAG gives aggregate capacity, not single-flow speed; order is preserved by per-flow hashing.
- LACP negotiates membership, detects miscabling, and handles standby; static bundles have no safety net.
- Hash policy decides utilization; make sure it includes enough entropy and is encapsulation-aware.
- MLAG is vendor-specific; EVPN multihoming is the standards-based alternative.
- On Linux use mode 4 (LACP) or mode 1 (active-backup) and read `/proc/net/bonding/*`.
