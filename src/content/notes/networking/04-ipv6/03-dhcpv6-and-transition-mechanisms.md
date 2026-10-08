---
title: DHCPv6 and IPv4/IPv6 Transition Mechanisms
track: networking
module: IPv6
moduleOrder: 4
order: 3
level: advanced
summary: Stateful and stateless DHCPv6, prefix delegation, and the toolbox for living between two protocols — dual-stack, tunnels, NAT64/DNS64, 464XLAT and IPv6-mostly networks.
sources:
  - label: "RFC 8415 — Dynamic Host Configuration Protocol for IPv6 (DHCPv6)"
    url: https://www.rfc-editor.org/rfc/rfc8415
  - label: "RFC 8305 — Happy Eyeballs Version 2"
    url: https://www.rfc-editor.org/rfc/rfc8305
  - label: "RFC 4213 — Basic Transition Mechanisms for IPv6 Hosts and Routers (6in4, dual stack)"
    url: https://www.rfc-editor.org/rfc/rfc4213
  - label: "RFC 6146 / RFC 6147 / RFC 6052 — Stateful NAT64, DNS64, IPv6 addressing of IPv4/IPv6 translators"
    url: https://www.rfc-editor.org/rfc/rfc6146
  - label: "RFC 6877 — 464XLAT"
    url: https://www.rfc-editor.org/rfc/rfc6877
  - label: "RFC 8925 — IPv6-Only Preferred Option for DHCPv4"
    url: https://www.rfc-editor.org/rfc/rfc8925
  - label: "RFC 5969 / RFC 6333 / RFC 7597 / RFC 7599 — 6rd, DS-Lite, MAP-E, MAP-T"
    url: https://www.rfc-editor.org/rfc/rfc5969
  - label: "RFC 7526 — Deprecating the Anycast Prefix for 6to4 Relay Routers"
    url: https://www.rfc-editor.org/rfc/rfc7526
---

## DHCPv6

DHCPv6 (RFC 8415) is a **separate protocol** from DHCPv4, not an extension of it.

- Transport: UDP, client port **546**, server/relay port **547**. Clients use their **link-local** address as source and send to the multicast `ff02::1:2` (All_DHCP_Relay_Agents_and_Servers).
- Client identity: a **DUID** (DHCP Unique Identifier: DUID-LLT = link-layer + time, DUID-EN, DUID-LL, DUID-UUID) rather than the MAC. Addresses are tracked per **IA** (identity association): `IA_NA` for non-temporary addresses, `IA_TA` for temporary, `IA_PD` for delegated prefixes.
- What it does **not** provide: the default gateway (that comes only from Router Advertisements) and the prefix length for addresses (assumed from RA's on-link prefix; DHCPv6 gives /128 addresses).

### Messages

| Client → server | Server → client |
|---|---|
| SOLICIT (find servers) | ADVERTISE (offers) |
| REQUEST | REPLY |
| CONFIRM (am I still on the right link?) | REPLY |
| RENEW (unicast to server) / REBIND (multicast) | REPLY |
| RELEASE / DECLINE | REPLY |
| INFORMATION-REQUEST (stateless) | REPLY |
| | RECONFIGURE (server → client, asks it to renew/inform) |
| Relay: RELAY-FORW (relay → server, encapsulates the client message, with `link-address`, `peer-address`) | RELAY-REPL |

**Four-message exchange** (SOLICIT, ADVERTISE, REQUEST, REPLY) corresponds to DORA; **Rapid Commit** reduces it to SOLICIT + REPLY.

### Modes

1. **Stateful** (RA `M=1`): server assigns addresses (IA_NA) and options, tracks leases (T1/T2 timers as in v4).
2. **Stateless** (RA `O=1`): SLAAC address, DHCPv6 INFORMATION-REQUEST only for DNS, NTP, domain list, etc. No server state.
3. **Prefix Delegation (IA_PD)**: a CPE router requests a prefix (e.g. /56) from the ISP's server and subdivides it into /64s for its LANs, the standard residential/branch IPv6 mechanism. The delegating router creates a route to the requesting router's link-local address for that prefix.

Operational notes: relays use `link-address` to select the subnet scope; use DUID-based reservations; Android doesn't speak DHCPv6, Windows/macOS/Linux do; and since RAs carry DNS (RDNSS), DHCPv6 is optional in many networks. To track address ownership (logging, security), stateful DHCPv6 or ND/snooping tables are necessary because SLAAC + privacy addresses are self-assigned.

```bash
dhclient -6 -N -v eth0         # stateful addresses
dhclient -6 -S -v eth0         # stateless (information-request)
tcpdump -ni eth0 'udp port 546 or udp port 547'
```

## Transition mechanisms

IPv4 and IPv6 are not interoperable, so a transition requires **coexistence** (run both), **tunnels** (carry one over the other), or **translation** (convert between them). Pick based on where you are.

### 1. Dual-stack (default recommendation)

Every host and router runs both IPv4 and IPv6. Applications resolve names to A and AAAA records and choose an address with **Happy Eyeballs v2** (RFC 8305): start IPv6, and 250 ms later start IPv4 in parallel; use whichever connects first and remember the result. Costs: two sets of addressing, routing, ACLs, monitoring and security policy (**firewall parity** is a frequent gap: IPv6 left open). Dual-stack doesn't solve IPv4 exhaustion on its own.

### 2. Tunneling (IPv6 over an IPv4 network)

| Mechanism | How | Status |
|---|---|---|
| **6in4 / configured tunnel** | IPv6 packet inside IPv4, protocol 41 (RFC 4213); used by tunnel brokers | stable; needs public IPv4 endpoints, not NAT |
| **6to4** (`2002::/16`, RFC 3056) | auto-tunnel using the IPv4 address embedded in the prefix | **deprecated** (RFC 7526), unreliable |
| **Teredo** | IPv6 over UDP through NAT | legacy |
| **6rd** (RFC 5969) | 6to4-like but ISP-owned prefix, operated by the ISP | used in some ISP deployments |
| **ISATAP** | intra-site automatic tunneling | obsolete |

### 3. Tunneling (IPv4 over an IPv6 network, for IPv6-only ISPs)

- **DS-Lite** (RFC 6333): CPE tunnels IPv4 in IPv6 to an AFTR (carrier-grade NAT44), so the access network is IPv6-only.
- **MAP-E** (RFC 7597, encapsulation) and **MAP-T** (RFC 7599, translation): stateless, deterministic mapping of IPv4 address + port range into IPv6, avoiding a stateful CGNAT and its logging.
- **Lightweight 4over6** (RFC 7596): stateful per-subscriber binding in the AFTR.

### 4. Translation

**NAT64 + DNS64** lets IPv6-only clients reach IPv4-only servers:

1. The client asks DNS64 for `example.com`. The name has only an A record (`192.0.2.33`), so DNS64 synthesizes an AAAA `64:ff9b::c000:221` (prefix + embedded IPv4, RFC 6052).
2. The client sends IPv6 packets to that address; routing sends them to the NAT64 gateway.
3. The **NAT64** (RFC 6146, stateful, like NAPT) maps the 5-tuple, builds an IPv4 packet from its pool address, and translates replies back (IP header translation per SIIT, RFC 7915).

Limits: applications using **IPv4 literals** or old APIs break (no DNS involved), DNSSEC validation conflicts with DNS64 synthesis (the validating client must be aware), and the gateway holds state.

**464XLAT** (RFC 6877) fixes IPv4-literal apps: a **CLAT** on the device (stateless translation IPv4→IPv6, e.g. Android, iOS, Windows) translates to IPv6, and the provider **PLAT** (NAT64) takes it to the IPv4 Internet. The user sees an "IPv4 address" (192.0.0.x) on a device that only has IPv6. Widely deployed by mobile carriers (T-Mobile US, etc.).

### 5. IPv6-mostly networks

RFC 8925 defines DHCPv4 **option 108 (IPv6-Only Preferred)**: capable clients that send it in the parameter request list receive a "V6ONLY_WAIT" time, and no IPv4 address; the network provides NAT64/DNS64 (+PREF64 in RA, option 38). Legacy clients still get IPv4, so one VLAN supports both. This allows a gradual removal of IPv4 from the LAN.

### Choosing

| Scenario | Common choice |
|---|---|
| Enterprise campus/data center | dual-stack, move to IPv6-only internally with NAT64 at the edge |
| Mobile carrier | IPv6-only + 464XLAT |
| Fixed ISP | dual-stack with CGNAT, or IPv6 + DS-Lite/MAP |
| Cloud workloads | dual-stack or IPv6-only subnets with NAT64 (provider feature) |
| Isolated IPv6 island, no native IPv6 | 6in4 tunnel |

### Linux examples

```bash
# 6in4 tunnel
ip link add he-ipv6 type sit remote 203.0.113.9 local 198.51.100.2 ttl 255
ip link set he-ipv6 up; ip addr add 2001:db8:1::2/64 dev he-ipv6
ip route add ::/0 dev he-ipv6

# NAT64 with Jool (kernel module) / tayga (userspace)
jool instance add "default" --netfilter --pool6 64:ff9b::/96
```

## Security considerations during transition

- Treat both stacks equally: same ACLs, IDS, logging, and inventory. An unfiltered IPv6 path is a common bypass ("IPv6 leak" in VPNs).
- Disable unused tunnels (6to4, Teredo, ISATAP) on hosts and block protocol 41 at the edge where unneeded.
- Filter RA/DHCPv6 on user ports; monitor for rogue routers (mitm6-style attacks target Windows hosts that prefer DHCPv6 over IPv4).
- NAT64 gateways need the same logging and abuse handling as CGNAT.

## Key takeaways

- DHCPv6 differs from DHCPv4: DUIDs, IA_NA/IA_PD, relays via RELAY-FORW, no default gateway (RA provides it).
- RA flags M/O choose stateful vs. stateless; prefix delegation is how CPE routers obtain LAN prefixes.
- Dual-stack is the baseline; tunnels and translation (NAT64/DNS64, 464XLAT, DS-Lite, MAP) cover IPv6-only access networks.
- IPv6-mostly (option 108) enables gradual IPv4 retirement; keep security policy at parity.
