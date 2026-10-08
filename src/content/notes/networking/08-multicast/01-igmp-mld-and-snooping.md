---
title: IP Multicast Basics — Addressing, IGMP, MLD and Snooping
track: networking
module: Multicast
moduleOrder: 8
order: 1
level: intermediate
summary: How one-to-many delivery is addressed and signalled on a LAN — multicast ranges and MAC mapping, IGMPv2/v3 and MLD operation, and how switches use snooping to avoid flooding.
sources:
  - label: "RFC 1112 — Host Extensions for IP Multicasting (STD 5)"
    url: https://www.rfc-editor.org/rfc/rfc1112
  - label: "RFC 2236 / RFC 3376 — IGMPv2 / IGMPv3"
    url: https://www.rfc-editor.org/rfc/rfc3376
  - label: "RFC 2710 / RFC 3810 — MLDv1 / MLDv2 for IPv6"
    url: https://www.rfc-editor.org/rfc/rfc3810
  - label: "RFC 4541 — Considerations for IGMP and MLD Snooping Switches"
    url: https://www.rfc-editor.org/rfc/rfc4541
  - label: "RFC 5771 / RFC 2365 — IPv4 Multicast Address Assignments; Administratively Scoped IP Multicast"
    url: https://www.rfc-editor.org/rfc/rfc5771
  - label: "RFC 4607 — Source-Specific Multicast for IP"
    url: https://www.rfc-editor.org/rfc/rfc4607
---

Unicast sends one copy to one receiver; broadcast reaches everyone; **multicast** sends one stream that the network replicates only where receivers exist. It underlies IPTV, financial market data, software distribution, routing protocols (OSPF, VRRP, PIM), mDNS/SSDP and data-center BUM replication.

## Group addressing

An IPv4 multicast **group** is a Class D address in `224.0.0.0/4` (224.0.0.0–239.255.255.255). A group address is a *destination only*; sources use their normal unicast address. Hosts **join** a group to receive; senders need not join.

| Range | Use |
|---|---|
| `224.0.0.0/24` | **link-local control**: never forwarded by routers (TTL 1): `.1` all hosts, `.2` all routers, `.5/.6` OSPF, `.9` RIP, `.13` PIM, `.18` VRRP, `.22` IGMPv3 reports, `.251` mDNS |
| `224.0.1.0/24` | internetwork control (NTP .1, ...) |
| `224.0.2.0 – 238.255.255.255` | global ("ad-hoc", SDP/SAP, GLOP `233.0.0.0/8` by ASN) |
| **`232.0.0.0/8`** | **Source-Specific Multicast (SSM)** |
| **`239.0.0.0/8`** | **administratively scoped (private)**: reuse inside an organization (RFC 2365): `239.255/16` site-local, `239.192/14` organization-local |

IPv6: `ff00::/8`, with scope in the second nibble (`ff02::` link-local, `ff05::` site, `ff0e::` global); SSM in `ff3x::/32` (see the IPv6 addressing note).

### Layer 2 mapping

IPv4 multicast MAC = `01-00-5E` + the **low 23 bits** of the group address (the top 4 bits are fixed `1110` and the next 5 are dropped). So **32 different groups map to one MAC**: e.g. `224.1.1.1`, `225.1.1.1`, `239.129.1.1` all use `01-00-5E-01-01-01`. NICs/hosts filter at L3 afterward. IPv6: `33-33` + the low 32 bits. Recommendation: avoid group addresses whose low 23 bits collide with `224.0.0.x` (e.g. `239.0.0.1`, `239.128.0.1`), because switches often flood those as "link-local control" groups.

## IGMP: host-to-router signalling

IGMP runs over IP (protocol **2**, TTL **1**, Router Alert option) between hosts and the local multicast router(s).

### IGMPv2 (RFC 2236)

| Message | Sent by → to | Purpose |
|---|---|---|
| **Membership Query (general)** | querier → `224.0.0.1` | "which groups have members?" every **125 s** (Query Interval), Max Response Time 10 s |
| **Group-specific Query** | querier → the group | after a Leave, "anyone still in group G?" (1 s, ×2) |
| **Membership Report** | host → the group address | "I'm a member of G"; sent on join and in response to queries after a random delay; **other hosts suppress** their report if they hear one |
| **Leave Group** | host → `224.0.0.2` | the last host to report leaves; triggers group-specific queries |

Querier election: lowest IP address among IGMP routers on the segment. Group Membership Interval = (robustness 2 × 125 s) + 10 s = **260 s**: without a refresh, the router deletes the group.

### IGMPv3 (RFC 3376)

Adds **source filtering**: a host reports `(G, INCLUDE {sources})` or `(G, EXCLUDE {sources})`, which enables **SSM** `(S,G)` joins, where the receiver requests a specific source and no RP/shared tree is needed. Reports go to **`224.0.0.22`** (all IGMPv3-capable routers), there's no report suppression (the router sees every host), and queries can be group-and-source specific. IGMPv3 is backward-compatible with v2/v1 (router falls back per-interface when it hears older reports).

### MLD (IPv6)

**MLDv1** (RFC 2710, from IGMPv2) and **MLDv2** (RFC 3810, from IGMPv3) are carried in ICMPv6: Query (130), Report v1 (131), Done (132), **Report v2 (143)** sent to `ff02::16`; hop limit 1 with a Hop-by-Hop Router Alert. Hosts also send MLD reports for their *solicited-node* groups; this is why networks with MLD snooping must handle NDP correctly.

## IGMP/MLD snooping

Without help, a switch floods multicast to all ports in the VLAN (treated like broadcast), so one video stream floods every access port. **IGMP snooping** (RFC 4541) lets the switch inspect IGMP/MLD packets between hosts and routers and build a per-VLAN table `(group → ports)`; multicast is forwarded only to member ports and to **multicast-router ports**.

Mechanics:

- Reports/Joins add the receiving port to the group's port list; entries age out after the membership timeout, unless refreshed by reports; Leave triggers a fast-leave or a group-specific query.
- **mrouter ports** (where a querier or PIM Hellos are seen, or statically configured) always receive multicast and all IGMP reports (so upstream routers learn membership).
- **Report suppression** (IGMPv2) can hide members from the switch: the switch forwards only one report per group per query upstream.
- **Fast leave** (immediate leave) removes the port as soon as a Leave arrives; valid only when one host per port (otherwise other hosts lose the stream).
- **IGMP querier on the VLAN**: if no multicast router exists in a VLAN (e.g. a pure L2 video network), the snooping switch needs an **IGMP snooping querier**; without a querier, hosts report only when asked and entries age out after about 260 s, so the stream **stops after a few minutes**, a classic symptom.
- Unknown multicast (no snooping entry) is flooded or dropped, as configured.
- Link-local `224.0.0.x` groups are always flooded.
- Don't forget **MLD snooping** for IPv6: when disabled while IGMP snooping is on, IPv6 NDP and RA multicasts can be mishandled.

Alternatives: static multicast MAC entries, **CGMP** (Cisco, obsolete), and EVPN IGMP proxy in VXLAN fabrics (type 6–8 routes).

## Linux

```bash
ip link set br0 type bridge mcast_snooping 1 mcast_querier 1   # snooping and querier on the bridge
bridge mdb show                                                # learned groups and ports
ip maddr show dev eth0                                         # groups joined by this host
cat /proc/net/igmp                                             # IGMP state
tcpdump -ni eth0 igmp or 'ip6[40] == 143'                      # IGMP and MLDv2 reports
```

Send/receive a test stream: `iperf -s -u -B 239.1.1.1 -i 1` and `iperf -c 239.1.1.1 -u -T 4 -t 5 -b 1M` (`-T` sets the TTL, which defaults to 1: a frequent cause of "multicast works only on the local subnet").

## Troubleshooting checklist

| Symptom | Likely cause |
|---|---|
| Stream stops after ~4–5 minutes | no IGMP querier; snooping timeout |
| Works on the same switch, not across routers | PIM not running; RPF failure; TTL = 1 |
| All hosts in the VLAN receive the stream | snooping off, or group maps to a flooded address (224.0.0.x MAC overlap) |
| Receiver joins, no data | wrong IGMP version mismatch with the querier, firewall blocks IGMP, host joins on the wrong interface |
| Duplicate packets | multiple paths/assert issues, snooping loops |

## Key takeaways

- Group addresses are destination-only; `232/8` is SSM, `239/8` is private scope; the 23-bit MAC mapping makes 32 groups share each MAC.
- IGMP (v2: query/report/leave; v3: source filtering, `224.0.0.22`) and MLD signal receiver interest to the local router.
- Snooping prevents flooding by tracking joins; a missing querier is the top cause of "multicast dies after a few minutes".
- Check the TTL on senders and the IGMP/MLD version when debugging.
