---
title: "Practice: A Static Routing Lab"
track: networking
module: Routing
moduleOrder: 5
order: 11
level: intermediate
summary: "A four-node routing lab on one Linux machine: hosts and two routers connected by virtual cables, showing why a ping fails with no route, why one-way routing is silent, how TTL counts hops, what ip route get reveals, and how a more specific route steals traffic, with captured output."
prerequisites:
  - "Forwarding: Routing Table, Longest-Prefix Match, Policy Routing and ECMP"
  - "Routing Fundamentals: Static Routes, Protocol Families and Convergence"
  - "OSPFv2 Operation: Neighbors, DR/BDR, Flooding and SPF"
sources:
  - label: "RFC 1812 — Requirements for IP Version 4 Routers"
    url: https://www.rfc-editor.org/rfc/rfc1812
  - label: "RFC 792 — Internet Control Message Protocol (Destination Unreachable, Time Exceeded)"
    url: https://www.rfc-editor.org/rfc/rfc792
  - label: "man 8 ip-route and man 8 ip-netns"
    url: https://man7.org/linux/man-pages/man8/ip-route.8.html
---

Routing tables are easy to read and surprisingly easy to get wrong: a route that exists in one direction is useless without the return route. This lab lets you break and fix a small network step by step on a single computer. It uses network namespaces as separate machines and veth pairs as cables; the commands run in a throwaway namespace and cannot change your real network.

```text
h1 10.0.1.2 --- 10.0.1.1 r1 10.0.12.1 --- 10.0.12.2 r2 10.0.2.1 --- 10.0.2.2 h2
   (10.0.1.0/24)             (10.0.12.0/30)                (10.0.2.0/24)
```

## Setup

```bash
unshare -Urnm bash
mount -t tmpfs tmpfs /run && ip link set lo up
for n in h1 r1 r2 h2; do ip netns add $n; ip -n $n link set lo up; done
link() {   # link NS_A NS_B IF_A IF_B ADDR_A ADDR_B
  ip link add $3 netns $1 type veth peer name $4 netns $2
  ip -n $1 addr add $5 dev $3; ip -n $2 addr add $6 dev $4
  ip -n $1 link set $3 up;     ip -n $2 link set $4 up; }
link h1 r1 eth0 eth0 10.0.1.2/24  10.0.1.1/24
link r1 r2 eth1 eth0 10.0.12.1/30 10.0.12.2/30
link r2 h2 eth1 eth0 10.0.2.1/24  10.0.2.2/24
for r in r1 r2; do ip netns exec $r sysctl -qw net.ipv4.ip_forward=1; done   # routers forward packets
ip -n h1 route add default via 10.0.1.1
ip -n h2 route add default via 10.0.2.1
```

A Linux host forwards packets between interfaces only when `net.ipv4.ip_forward=1`; that setting is what makes `r1` and `r2` routers. The hosts only know a default gateway.

## Exercises

**1. No route.** From `h1`, ping `h2` (`ip netns exec h1 ping -c 1 -W 1 10.0.2.2`). Who answers, with what message, and why? What does `ip -n r1 route` contain?

**2. A one-way route.** Add on `r1` the route to h2's network: `ip -n r1 route add 10.0.2.0/24 via 10.0.12.2`. Ping again. The packet now reaches `h2`, yet `h1` hears nothing. Why, and which single command fixes it?

**3. Count the hops.** After the fix, ping succeeds. The reply shows `ttl=62`. The ping started with TTL 64. What does 62 tell you? What would `ttl` have been if there were three routers?

**4. Ask the kernel.** What do `ip netns exec r1 ip route get 10.0.2.2` and `ip route get 10.0.1.2` print, and how do you read them?

**5. A more specific route wins.** Add on `r1` the host route `10.0.2.2/32 dev eth0` (it points back towards `h1` by mistake). Which route does `r1` now use for `10.0.2.2`, and what happens to the ping? What does this show about longest-prefix match?

**6. Short answers.** (a) What would a routing protocol such as OSPF have done in exercises 1 and 2 for you? (b) What is the difference between a *host* route and a *default* route? (c) Why is a missing *return* route so hard to notice?

## Answers

Try before opening. These outputs come from a real run of the lab (round-trip times are masked).

<details>
<summary>Show answers and captured output</summary>

**1.**

```text
$ ip netns exec h1 ping -c 1 -W 1 10.0.2.2
From 10.0.1.1 icmp_seq=1 Destination Net Unreachable
$ ip -n r1 route
10.0.1.0/24 dev eth0 proto kernel scope link src 10.0.1.1
10.0.12.0/30 dev eth1 proto kernel scope link src 10.0.12.1
```

`h1` sends the packet to its default gateway `r1`. `r1` forwards by its routing table, which holds only its two **connected** networks (the `proto kernel` lines the kernel adds when an interface gets an address). `10.0.2.0/24` is not among them, so `r1` drops the packet and answers with an ICMP **Destination Net Unreachable**, sourced from its own address `10.0.1.1`. That error message is useful: it tells you *which router* has no route, so you start debugging there.

**2.**

```text
$ ip -n r1 route add 10.0.2.0/24 via 10.0.12.2
$ ip netns exec h1 ping -c 1 -W 1 10.0.2.2
1 packets transmitted, 0 received, 100% packet loss
```

Now `r1` forwards to `r2`, and `r2` delivers the request to `h2` (it is directly connected to `10.0.2.0/24`), so `h2` receives it and sends a reply to `10.0.1.2`. The reply goes to `h2`'s default gateway `r2`, which has **no route to `10.0.1.0/24`**: it drops the reply and sends an ICMP error to `h2`, where nobody is listening. `h1` just times out with no error message at all. Routing must work in **both directions**. The fix is the return route on `r2`:

```text
$ ip -n r2 route add 10.0.1.0/24 via 10.0.12.1
```

**3.**

```text
$ ip netns exec h1 ping -c 2 -W 1 10.0.2.2
64 bytes from 10.0.2.2: icmp_seq=1 ttl=62 time=X ms
64 bytes from 10.0.2.2: icmp_seq=2 ttl=62 time=X ms
2 packets transmitted, 2 received, 0% packet loss
```

Every router that forwards a packet **decrements its TTL by one** (and drops the packet at zero, sending an ICMP Time Exceeded). The reply left `h2` with TTL 64, crossed `r2` and `r1` (two routers) and arrived with 62. With three routers on the path the value would be 61. This is exactly how `traceroute` works: it sends packets with TTL 1, 2, 3, ... and records which router sends back "Time Exceeded". The initial value (64 for Linux, 128 for Windows, 255 for many network devices) also lets you guess the operating system, and the difference to the initial value gives the hop count.

**4.**

```text
$ ip netns exec r1 ip route get 10.0.2.2
10.0.2.2 via 10.0.12.2 dev eth1 src 10.0.12.1 uid 0
$ ip netns exec r1 ip route get 10.0.1.2
10.0.1.2 dev eth0 src 10.0.1.1 uid 0
```

`ip route get` asks the kernel which route it would actually use for a destination, which is more reliable than reading the table by eye. The first line says: for `10.0.2.2` go **via** the next hop `10.0.12.2` out of `eth1`, using source address `10.0.12.1`. The second line has no `via`: `10.0.1.2` is **directly connected** on `eth0`, so the router sends straight to it (after ARP). "via" means "send to this router", its absence means "deliver locally".

**5.**

```text
$ ip -n r1 route add 10.0.2.2/32 dev eth0
$ ip netns exec r1 ip route get 10.0.2.2
10.0.2.2 dev eth0 src 10.0.1.1 uid 0
$ ip netns exec h1 ping -c 1 -W 1 10.0.2.2
1 packets transmitted, 0 received, 100% packet loss
```

Two routes now match `10.0.2.2`: the `/24` via `r2` and the `/32` out of `eth0`. The router picks the **longest prefix**, the `/32`, and sends the packet towards `h1`, a dead end, so the ping fails and nothing else about `10.0.2.0/24` changed: the other hosts in that network still work. Longest-prefix match always prefers the most specific route regardless of where it came from or how "good" the other one looks. That is why a stray `/32` or `/25` route (or a hijacker's more specific BGP announcement) silently steals traffic, and why you should check `ip route get` for the *specific* destination, not just the table. Remove the bad route with `ip -n r1 route del 10.0.2.2/32`.

**6.** (a) A routing protocol would have **learned the networks automatically**: `r1` and `r2` would exchange their connected networks as neighbours (OSPF forms the adjacency over `10.0.12.0/30`) and install both routes, so exercises 1 and 2 would not have occurred unless the adjacency itself was broken; it would also **reconverge** around link failures, which static routes never do. Static routes are right for small, stable networks and stub sites; protocols for anything that changes. (b) A **host route** is a `/32` for exactly one address; a **default route** is `0.0.0.0/0`, the shortest possible prefix, matching everything not covered by anything more specific (the "gateway of last resort"). (c) Because the forward path *looks* fine: the request arrives, nothing reports an error on the sending side, and the drop happens on a different machine, in the other direction. Always test and read the **return path** (`ip route get` from the far side, or run a capture there), and remember that asymmetric routing is legitimate in some designs but confuses stateful firewalls.

</details>

## Key takeaways

1. A router forwards only what its table contains; an unknown destination gets an ICMP Net Unreachable from the router that lacks the route, which tells you where to look.
2. Routes must exist in both directions; a missing return route fails silently from the sender's point of view.
3. TTL drops by one per router, which is what traceroute exploits; the arriving TTL minus the starting TTL counts the hops.
4. `ip route get DEST` shows the route the kernel would really use; the longest matching prefix always wins, so one stray specific route can steal traffic.
5. Static routes suit small stable networks; routing protocols learn routes and reconverge after failures.
