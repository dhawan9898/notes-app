---
title: VLSM, Summarisation, Longest-Prefix Match and Fragmentation
track: networking
module: IPv4
moduleOrder: 3
order: 1
level: intermediate
minutes: 40
env: browser
terminal:
  - ./ipv4_practice
summary: "Four exercises on the IPv4 module: a VLSM addressing plan, summarising routes (and when it fails), choosing routes by longest-prefix match, and fragmenting a datagram by hand, with a C program that computes every answer and a check of the reasoning."
prerequisites:
  - "IPv4 Header, Fragmentation and MTU"
  - "IPv4 Addressing, Subnetting, VLSM and CIDR"
  - "Forwarding: Routing Table, Longest-Prefix Match, Policy Routing and ECMP"
sources:
  - label: "RFC 4632 — Classless Inter-domain Routing (CIDR): the Internet Address Assignment and Aggregation Plan"
    url: https://www.rfc-editor.org/rfc/rfc4632
  - label: "RFC 791 — Internet Protocol (fragmentation and reassembly)"
    url: https://www.rfc-editor.org/rfc/rfc791
  - label: "RFC 1812 — Requirements for IP Version 4 Routers (longest-prefix match)"
    url: https://www.rfc-editor.org/rfc/rfc1812
---

Subnetting skill is the foundation of every network design. Do each exercise on paper, then run the program at the end to check yourself. The simpler "same network?" and "network and broadcast address" drills are in *Practice: Networking Exercises*; these four are the ones that appear in real design work.

## Exercises

**1. A VLSM plan.** Divide `192.168.50.0/24` into five subnets for: Servers (100 hosts), Office (50), Guests (25), Management (10) and a router-to-router link (2). Allocate the largest first, aligned on block boundaries, and wasting as little as possible. What is the first address still free afterwards?

**2. Summarise.** The four routes `10.1.0.0/24`, `10.1.1.0/24`, `10.1.2.0/24` and `10.1.3.0/24` should be advertised as one. Give the summary. Then replace the last route by `10.1.4.0/24`: can they still be summarised into one route without including addresses you do not own?

**3. Longest-prefix match.** The routing table holds `0.0.0.0/0` (ISP), `10.0.0.0/8` (core), `10.1.0.0/16` (site A), `10.1.2.0/24` (lab) and `10.1.2.128/25` (lab servers). Which route does a router choose for `10.1.2.200`, `10.1.2.7`, `10.1.9.9`, `10.9.9.9` and `8.8.8.8`, and why is "the most specific route wins" the rule?

**4. Fragmentation.** A host sends 4000 bytes of IP payload over a link with MTU 1500 (header 20 bytes, no options). How many fragments are there, what is each fragment's total length, offset field and More-Fragments flag? Why must every fragment except the last carry a multiple of 8 bytes of data?

## Answers

Try before opening.

<details>
<summary>Show the program and its output</summary>

{{include vm/ipv4_practice.c}}

{{vm :: ./ipv4_practice}}

**1. VLSM.** For each requirement, find the smallest block with at least *hosts + 2* addresses (`2^bits − 2 ≥ hosts`): 100 hosts need `/25` (126 usable), 50 need `/26` (62), 25 need `/27` (30), 10 need `/28` (14) and 2 need `/30` (2). Allocate **largest first**: each block must start at a multiple of its own size, and placing big blocks first keeps everything aligned with no gaps, which is why the plan packs perfectly into the first 244 addresses of the /24. The next free address is `192.168.50.244`, leaving room for a later `/30`, `/29` or `/28`. Plan growth when you choose sizes: a `/27` for 25 guests leaves only 5 spare addresses. Giving each subnet its own VLAN and gateway address (the first usable address by convention) completes the design.

**2. Summarisation.** Four consecutive `/24`s starting on a multiple of four (`10.1.0.0` to `10.1.3.0`) collapse exactly into `10.1.0.0/22`: the first 22 bits are identical and the last two bits of the third octet enumerate the four networks. Summarisation shrinks routing tables and hides instability (a flapping `/24` inside the summary no longer causes updates elsewhere). With `10.1.4.0/24` in place of `10.1.3.0/24` the best collapse is three routes (`10.1.0.0/23`, `10.1.2.0/24`, `10.1.4.0/24`): the smallest single covering prefix would be `10.1.0.0/21`, which also covers `10.1.3.0/24`, `10.1.5.0/24` and so on that you may not own, and **advertising addresses you do not own attracts traffic you cannot deliver (a black hole)**. Good address plans allocate blocks that summarise cleanly.

**3. Longest-prefix match.** Every route whose prefix contains the destination *matches*; the router uses the **longest** (most specific) one:

- `10.1.2.200` matches five routes; the longest is `10.1.2.128/25`, so it goes to **lab-servers**.
- `10.1.2.7` is not in `10.1.2.128/25` (that covers .128 to .255), so the `/24` **lab** route wins.
- `10.1.9.9` leaves the lab, so `10.1.0.0/16` (**site A**) wins; `10.9.9.9` only matches the `/8` (**core**); `8.8.8.8` falls through to the default route (**ISP**).

Specific-beats-general lets you carve exceptions out of larger blocks and keeps the default route as a catch-all. The rule is why a *more specific* route announced by an attacker or by mistake pulls traffic away (route hijacks), and why `/32` host routes are powerful. Ties between equal prefixes are broken by administrative distance and metric, not by prefix length (*Forwarding*).

**4. Fragmentation.** A fragment can carry at most `MTU − header = 1480` bytes of data, and the **fragment offset field counts in units of 8 bytes** (it is only 13 bits wide), so every fragment except the last must carry a multiple of 8 bytes: 1480 is exactly 185 × 8. The 4000 bytes become three fragments: 1480 + 1480 + 1040 bytes of data, with total lengths 1500, 1500 and 1060, offset fields 0, 185 and 370, and More-Fragments set on the first two only. The receiver reassembles by (source, destination, protocol, identification) and the offsets, and discards everything if any fragment is lost, so one lost fragment wastes the whole datagram. This is why modern stacks avoid fragmentation with **Path MTU Discovery** (set the Don't Fragment bit, and shrink on ICMP "fragmentation needed") and why blocking ICMP breaks connections that cross smaller-MTU links (see *IPv4 Header, Fragmentation and MTU*).

</details>

## Key takeaways

1. VLSM: size each subnet for `hosts + 2`, allocate the largest first and align every block on its own size.
2. Contiguous, aligned blocks summarise exactly into one shorter prefix; summarising non-contiguous blocks advertises addresses you do not own.
3. Routers choose the longest matching prefix; the default route `0.0.0.0/0` matches everything and loses to anything more specific.
4. Fragment data is a multiple of 8 bytes (offset units); losing one fragment loses the whole datagram, so prefer Path MTU Discovery.
