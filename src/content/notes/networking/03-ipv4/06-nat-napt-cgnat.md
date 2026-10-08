---
title: NAT — Static, NAPT, Port Forwarding, Hairpinning and CGNAT
track: networking
module: IPv4
moduleOrder: 3
order: 6
level: advanced
summary: How address and port translation works on the wire, the RFC 4787 behavior classes that decide whether peer-to-peer works, conntrack mechanics, timers, CGNAT and what breaks.
sources:
  - label: "RFC 3022 — Traditional IP Network Address Translator"
    url: https://www.rfc-editor.org/rfc/rfc3022
  - label: "RFC 4787 — NAT Behavioral Requirements for Unicast UDP (BCP 127)"
    url: https://www.rfc-editor.org/rfc/rfc4787
  - label: "RFC 5382 — NAT Behavioral Requirements for TCP (BCP 142)"
    url: https://www.rfc-editor.org/rfc/rfc5382
  - label: "RFC 5508 — NAT Behavioral Requirements for ICMP"
    url: https://www.rfc-editor.org/rfc/rfc5508
  - label: "RFC 6888 — Requirements for CGNs; RFC 6598 — Shared Address Space"
    url: https://www.rfc-editor.org/rfc/rfc6888
  - label: "RFC 8489 / RFC 8445 — STUN / ICE"
    url: https://www.rfc-editor.org/rfc/rfc8445
  - label: "RFC 3948 — UDP Encapsulation of IPsec ESP Packets (NAT-T)"
    url: https://www.rfc-editor.org/rfc/rfc3948
  - label: "Linux — nftables wiki (NAT), conntrack-tools"
    url: https://wiki.nftables.org/wiki-nftables/index.php/Performing_Network_Address_Translation_(NAT)
---

Network Address Translation rewrites IP addresses (and usually ports) in packets as they cross a boundary. It was introduced to slow IPv4 address exhaustion and also lets private networks (RFC 1918) reach the Internet. It breaks the end-to-end principle, and most of its complexity comes from that.

## Terminology and types

| Term | Meaning |
|---|---|
| Inside local / inside global | a host's address as seen inside / outside |
| **Static NAT (1:1)** | fixed mapping of one inside address to one outside address; inbound allowed |
| **Dynamic NAT** | pool of outside addresses assigned on demand (1:1 while in use) |
| **NAPT / PAT / "masquerade"** | many inside hosts share one outside address, distinguished by **port** |
| **SNAT** | rewrite the *source* (outbound traffic) |
| **DNAT** | rewrite the *destination* (inbound port forwarding, load balancing) |
| **Twice NAT** | rewrite both source and destination (overlapping networks) |

## How NAPT works

The NAT keeps a **translation table** (Linux: **conntrack**) keyed by the flow's five-tuple. Example, an inside host `192.168.1.10:51000` connects to `203.0.113.5:443` through a NAT with public address `198.51.100.1`:

| Direction | Source | Destination |
|---|---|---|
| Inside → NAT | 192.168.1.10:51000 | 203.0.113.5:443 |
| NAT → Internet | **198.51.100.1:40001** | 203.0.113.5:443 |
| Internet → NAT (reply) | 203.0.113.5:443 | 198.51.100.1:40001 |
| NAT → Inside | 203.0.113.5:443 | **192.168.1.10:51000** |

On each packet the NAT updates the IP addresses, the L4 ports, the **IP header checksum** and the **TCP/UDP checksum** (which covers a pseudo-header with the addresses; incremental update is used). It also tracks state: TCP handshake/teardown, and removes the mapping after a timeout.

- ICMP errors for a translated flow must have their *embedded* packet translated too (RFC 5508), or PMTUD and traceroute break. Echo uses the ICMP identifier as the "port".
- **Fragments**: only the first has ports, so NAT must reassemble or track fragments (Linux conntrack defragments).
- Protocols embedding addresses in payload need an **ALG** (application-level gateway): active FTP, SIP/H.323, PPTP, IRC DCC. ALGs are fragile, and modern protocols avoid them.

### Linux (nftables)

```bash
# source NAT for everything leaving eth0 (dynamic address)
nft add table ip nat
nft add chain ip nat postrouting '{ type nat hook postrouting priority 100; }'
nft add rule ip nat postrouting oifname "eth0" masquerade
# fixed public address: faster than masquerade, no lookup of the interface address
nft add rule ip nat postrouting oifname "eth0" snat to 198.51.100.1
# port forward (DNAT)
nft add chain ip nat prerouting '{ type nat hook prerouting priority -100; }'
nft add rule ip nat prerouting iifname "eth0" tcp dport 8443 dnat to 192.168.1.20:443
sysctl -w net.ipv4.ip_forward=1
conntrack -L | head          # active translations
conntrack -S                 # stats: insert_failed, drop, early_drop → table pressure
```

NAT hooks apply only to the **first packet** of a flow; later packets follow the conntrack entry. Order matters: DNAT in `prerouting` (before routing decision), SNAT in `postrouting`.

## Behavior classes: what decides whether P2P works (RFC 4787)

Older descriptions ("full cone, restricted cone, port-restricted cone, symmetric", RFC 3489) are replaced by two independent behaviors:

**Mapping behavior** (which external ip:port is used for an internal ip:port):
- **Endpoint-Independent Mapping (EIM)**: the same external port is used for all destinations. Required by RFC 4787 (REQ-1) because it lets peers learn your external address via STUN and connect.
- **Address-Dependent / Address-and-Port-Dependent Mapping** ("symmetric NAT"): a *different* external port per destination, so the port learned from a STUN server isn't valid for another peer → hole punching fails; relays (TURN) are needed.

**Filtering behavior** (which inbound packets are accepted through a mapping): endpoint-independent (anyone), address-dependent, or address-and-port-dependent (only the exact peer contacted). RFC 4787 recommends endpoint-independent filtering where security permits, otherwise address-dependent.

Other RFC 4787 requirements: **port preservation** if possible, **hairpinning** support, UDP binding timeout of at least 2 minutes (5 minutes recommended; and keep-alives refresh it), and "paired" IP address pooling (a host keeps one external IP). RFC 5382 TCP: an established-connection idle timeout ≥ 2 h 4 min.

### Traversal techniques

- **STUN** (RFC 8489): a client asks a public server "what source ip:port do you see?"
- **ICE** (RFC 8445) gathers candidates (host, server-reflexive via STUN, relayed via **TURN**, RFC 8656) and tries pairs; used by WebRTC, VoIP.
- **UDP hole punching**: both peers send to each other's reflexive address so that each NAT creates the mapping.
- **UPnP-IGD / NAT-PMP / PCP (RFC 6887)**: ask the NAT to open a port.
- **IPsec NAT-T**: ESP has no ports, so IKE detects NAT and switches to UDP 4500 encapsulation (RFC 3948).
- Keepalives to hold mappings (typically every 15–30 s for UDP).

## Hairpinning (NAT loopback)

An inside host accessing a *public* address of an inside server (`198.51.100.1:8443`, which is DNAT-ed to `192.168.1.20`). The packet reaches the NAT from inside, gets DNAT-ed, and the server replies **directly** to the client (same LAN), with a source address that the client doesn't expect → connection fails. Fixes: **hairpin NAT** (also SNAT those flows so replies pass through the NAT), **split-horizon DNS** returning the private address inside, or configuring the reverse proxy. RFC 4787 REQ-9 requires NATs to support hairpinning.

## Carrier-grade NAT (CGNAT / LSN)

ISPs share one public address among many subscribers by adding a second NAT layer. RFC 6598 reserves **100.64.0.0/10** for the ISP-internal side (so it can't clash with customers' RFC 1918). RFC 6888 lists requirements: deterministic or port-block allocation, per-subscriber limits, logging.

| Concern | Effect |
|---|---|
| **Port exhaustion** | ~64k ports per public IP shared by up to hundreds of subscribers; per-subscriber port quotas (e.g. 1024–4096 ports) cause failures with browsers/CDNs opening many connections |
| **Logging** | for law enforcement the ISP must map (public IP, port, time) → subscriber; *port-block allocation* or deterministic NAT shrinks the logs |
| **No inbound** | customers can't host services/port-forward; P2P and gaming suffer |
| **Shared reputation** | one abusive user can get a public IP blocklisted for everyone |
| **Double NAT** | home NAT behind CGNAT: layered timeouts, no UPnP |
| **Alternatives** | IPv6 (preferred), 464XLAT, DS-Lite (RFC 6333), MAP-E/MAP-T (RFC 7597/7599) |

## NAT64 (preview)

IPv6-only clients reach IPv4 servers through a NAT64 gateway (RFC 6146) that maps IPv6 destinations in a prefix (e.g., `64:ff9b::/96`, RFC 6052) to IPv4, with **DNS64** synthesizing AAAA records. See the IPv6 transition note.

## Capacity and operations

- **Conntrack sizing**: each entry ≈ 300 bytes; set `nf_conntrack_max` and hash buckets for peak flows. "nf_conntrack: table full, dropping packet" is a common outage on busy gateways or DoS (SYN/UDP floods).
- **Timeouts** (Linux defaults): TCP established 5 days (`nf_conntrack_tcp_timeout_established=432000`), UDP 30 s (unreplied) / 120 s (stream), ICMP 30 s. Lower them on busy NATs; raise them for long-lived idle sessions (SSH, databases) or use TCP keepalives.
- Asymmetric routing around a stateful NAT drops return traffic because the NAT sees only one direction.
- NAT ≠ security. It's a side effect that inbound unsolicited traffic has no mapping, but a stateful firewall is the real control. Don't treat RFC 1918 space as a security boundary.
- NAT complicates logging (log pre-NAT addresses), geo-IP, rate-limiting by IP (many users share one IP), and IPsec/AH (AH cannot traverse NAT).

## Key takeaways

- NAPT rewrites addresses, ports and both checksums, keeping per-flow state; DNAT in prerouting, SNAT in postrouting.
- RFC 4787 splits NAT behavior into mapping (endpoint-independent is the P2P-friendly one) and filtering; "symmetric" NAT needs TURN.
- Hairpinning, ALGs, fragments and ICMP errors are the usual special cases.
- CGNAT (100.64/10) trades address space for port exhaustion, logging burden, and loss of inbound reachability.
- Size conntrack and timeouts deliberately; NAT is not a firewall.
