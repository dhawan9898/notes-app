---
title: IPv4 Addressing, Subnetting, VLSM and CIDR
track: networking
module: IPv4
moduleOrder: 3
order: 2
level: intermediate
summary: Prefix arithmetic you can do in your head, special-purpose address blocks, VLSM design with a worked plan, and route summarization rules.
prerequisites:
  - What Is a Network?
  - Binary, Hexadecimal and Bits
sources:
  - label: "RFC 4632 — Classless Inter-domain Routing (CIDR)"
    url: https://www.rfc-editor.org/rfc/rfc4632
  - label: "RFC 1918 — Address Allocation for Private Internets"
    url: https://www.rfc-editor.org/rfc/rfc1918
  - label: "RFC 6890 — Special-Purpose IP Address Registries"
    url: https://www.rfc-editor.org/rfc/rfc6890
  - label: "RFC 6598 — IANA-Reserved IPv4 Prefix for Shared Address Space"
    url: https://www.rfc-editor.org/rfc/rfc6598
  - label: "RFC 3021 — Using 31-Bit Prefixes on IPv4 Point-to-Point Links"
    url: https://www.rfc-editor.org/rfc/rfc3021
  - label: "RFC 3927 — Dynamic Configuration of IPv4 Link-Local Addresses"
    url: https://www.rfc-editor.org/rfc/rfc3927
---

## Addresses and prefixes

An IPv4 address is 32 bits, written as four decimal octets. A **prefix** `a.b.c.d/n` fixes the first *n* bits (the **network** part) and leaves 32 − n **host** bits. The old **classful** scheme (A: /8, B: /16, C: /24; determined by leading bits) was abandoned in 1993 for **CIDR** (RFC 4632), where the prefix length is carried explicitly. "Class C" only survives as shorthand for a /24.

| Notation | Meaning |
|---|---|
| `192.168.10.64/26` | prefix length 26 |
| `255.255.255.192` | netmask: 26 one-bits |
| `0.0.0.63` | wildcard (inverse) mask, used by ACLs and OSPF `network` statements |

For a prefix of length *n*:

- addresses in block = `2^(32−n)`
- **network address** = address AND mask (all host bits 0)
- **broadcast address** = all host bits 1
- usable hosts = `2^(32−n) − 2` (for n ≤ 30)

Example, `192.168.10.64/26`: block size 64, network `.64`, broadcast `.127`, hosts `.65`–`.126` (62).

### Doing it in your head

Find the interesting octet (the one where the mask is neither 0 nor 255): the **block size** is `256 − mask_octet`, and the network address is the largest multiple of block size ≤ the address's octet.

`10.20.77.130/21`: mask 255.255.248.0 → third octet block size 8; 77 → 72 → network `10.20.72.0`, broadcast `10.20.79.255`. (Verify: /21 spans 8 /24s: 72–79.)

### Special cases

- **/31** (RFC 3021): two addresses, no network/broadcast, used on **point-to-point** links to save address space. Supported by modern routers and Linux.
- **/32**: a single host; loopbacks, router IDs, anycast/VIPs.
- **/30**: older point-to-point convention (2 usable).
- **/0**: the default route.

## Special-purpose blocks (RFC 6890)

| Block | Purpose |
|---|---|
| `0.0.0.0/8` | "this network"; `0.0.0.0` = unspecified / default route |
| `10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16` | **private** (RFC 1918): not routed on the Internet |
| `100.64.0.0/10` | **shared address space** for carrier-grade NAT (RFC 6598), neither private nor global |
| `127.0.0.0/8` | loopback (the whole /8, not just 127.0.0.1) |
| `169.254.0.0/16` | link-local (RFC 3927), self-assigned when DHCP fails; never routed |
| `192.0.0.0/24` | IETF protocol assignments |
| `192.0.2.0/24`, `198.51.100.0/24`, `203.0.113.0/24` | documentation (TEST-NET-1/2/3) |
| `198.18.0.0/15` | benchmarking (RFC 2544) |
| `224.0.0.0/4` | multicast (`224.0.0.0/24` link-local control: .1 all hosts, .2 all routers, .5/.6 OSPF, .18 VRRP) |
| `240.0.0.0/4` | reserved ("Class E"); `255.255.255.255` limited broadcast |

These are not globally routable and should be filtered at the Internet edge ("bogon" filtering). In outage analysis, a source of `169.254.x.x` means DHCP failed, and `100.64.x.x` suggests CGNAT.

## VLSM: variable-length subnet masks

VLSM lets you carve a block into subnets of different sizes to avoid waste. The method:

1. List requirements; add growth and **2** for network+broadcast (and gateway/HSRP addresses).
2. Round each up to a power of two and sort **largest first**.
3. Allocate sequentially from the start of the parent block; every subnet must start on a multiple of its own size ("aligned"), which largest-first allocation guarantees.

**Example.** Parent block `172.16.0.0/22` (1024 addresses). Requirements: A = 500 hosts, B = 120, C = 50, D = a router link.

| Subnet | Need | Block | Prefix | Allocation | Usable range |
|---|---|---|---|---|---|
| A | 500 + 2 | 512 | /23 | `172.16.0.0/23` | .0.1 – .1.254 |
| B | 120 + 2 | 128 | /25 | `172.16.2.0/25` | 2.1 – 2.126 |
| C | 50 + 2 | 64 | /26 | `172.16.2.128/26` | 2.129 – 2.190 |
| D | 2 | 4 (or 2 with /31) | /30 | `172.16.2.192/30` | 2.193 – 2.194 |

Free afterwards: `172.16.2.196` to `172.16.3.255` (≈ 316 addresses), which is available for later /30–/25 subnets. Keep a spreadsheet or IPAM (NetBox, phpIPAM) as the source of truth; leave **gaps for growth** at each aggregation point so that summarization stays possible.

## Summarization (supernetting)

Combining contiguous prefixes into a shorter one so routers advertise a single route. Requirements: the blocks must be **contiguous, equal-sized or composing a power of two, and the first must be aligned** to the size of the summary.

- `10.1.0.0` – `10.1.3.255` → `10.1.0.0/22` ✓ (four /24s, start aligned on a multiple of 4)
- `10.1.4.0` – `10.1.7.255` → `10.1.4.0/22` ✓
- `10.1.4.0` – `10.1.8.255` → needs two routes, `10.1.4.0/22` + `10.1.8.0/24` (five /24s cannot form one aligned block)
- `10.1.1.0/24` – `10.1.4.0/24` cannot be one /22 or /21, because 10.1.1 is not aligned to a multiple of 4.

Benefits: smaller routing tables, **fault isolation** (flaps inside the summary are hidden from the rest of the network), faster convergence. Risk: a summary can **attract traffic** for addresses that don't exist (black hole), so routers install a **discard route** (Null0/blackhole) for the summary to prevent loops with the default route.

Design principle: align address plan to the topology (a /16 per region, /20 per site, /24 per VLAN) so hierarchical summarization works.

## Scripting it

```python
import ipaddress as ip
n = ip.ip_network("192.168.10.64/26")
n.network_address, n.broadcast_address, n.netmask, n.num_addresses - 2
list(ip.summarize_address_range(ip.ip_address("10.1.4.0"), ip.ip_address("10.1.8.255")))
list(ip.ip_network("10.1.0.0/22").subnets(new_prefix=24))
ip.ip_address("100.64.1.1").is_global      # False (shared space)
```

```bash
ipcalc 10.20.77.130/21                 # or: sipcalc
ip -4 addr show                        # "scope global/link/host", "brd" shows broadcast
```

## Typical mistakes

- Overlapping prefixes (two sites both using 10.0.0.0/24) block VPN/merger projects. NAT is a painful fix. Plan a non-overlapping scheme or use less common ranges (e.g. 10.x with a registry).
- Using the all-zeros/all-ones subnet is fine today (classless).
- Forgetting the gateway and HA addresses (VRRP VIP + two physical = 3) in host counts.
- Using `/24` everywhere: wasteful on links, too small for large VLANs; yet huge flat L2 domains (/20 and up) have broadcast and ARP scaling limits.

## Key takeaways

- A prefix of length *n* has `2^(32−n)` addresses; usable hosts = that − 2, except /31 and /32.
- Block size in the interesting octet = 256 − mask octet; network = floor to a multiple.
- Know the special-purpose ranges: RFC 1918, 100.64/10 (CGNAT), 169.254/16, 127/8, 224/4, documentation blocks.
- VLSM: allocate largest first, aligned; summarization requires contiguous aligned power-of-two blocks.
- Add a discard route for every summary to avoid loops.
