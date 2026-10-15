---
title: Redundant Paths, ECMP and Failure Detection
track: networking
module: Resilience
moduleOrder: 6
order: 1
level: intermediate
minutes: 45
summary: "A lab with two equal-cost paths: how a multipath route spreads flows, how a link failure marks a next hop dead so that all flows move to the survivor without losing connectivity, how traffic rebalances on recovery, plus the arithmetic of failure-detection timers for VRRP, OSPF and BFD, with the script and captured output."
prerequisites:
  - "First-Hop Redundancy — VRRP, HSRP and Anycast Gateways"
  - "BFD — Bidirectional Forwarding Detection"
  - "ACLs, Stateless vs Stateful Filtering and Infrastructure Protection"
sources:
  - label: "RFC 5798 — Virtual Router Redundancy Protocol (VRRP) Version 3"
    url: https://www.rfc-editor.org/rfc/rfc5798
  - label: "RFC 5880 — Bidirectional Forwarding Detection (BFD)"
    url: https://www.rfc-editor.org/rfc/rfc5880
  - label: "RFC 2992 — Analysis of an Equal-Cost Multi-Path Algorithm"
    url: https://www.rfc-editor.org/rfc/rfc2992
  - label: "Linux kernel documentation — ip-sysctl (fib_multipath_hash_policy, ignore_routes_with_linkdown)"
    url: https://docs.kernel.org/networking/ip-sysctl.html
---

Redundancy has two halves: **having** a second path and **noticing** quickly that the first one failed. This lab shows the first half with a real kernel and does the arithmetic for the second. A client `c` reaches a server address `10.9.9.9` through two routers, `r1` and `r2`, over two equal-cost paths (the topology is the same four-namespace technique as *Practice: A Static Routing Lab*).

```text
          10.0.1.0/24             10.0.11.0/24
   +--- c1 [ r1 ] r1s ---------------+
 c                                    s (10.9.9.9)
   +--- c2 [ r2 ] r2s ---------------+
          10.0.2.0/24             10.0.12.0/24
```

## The lab

```bash
unshare -Urnm bash
mount -t tmpfs tmpfs /run && ip link set lo up
for n in c r1 r2 s; do ip netns add $n; ip -n $n link set lo up; done
mk() { ip link add $3 netns $1 type veth peer name $4 netns $2; ip -n $1 addr add $5 dev $3; ip -n $2 addr add $6 dev $4; ip -n $1 link set $3 up; ip -n $2 link set $4 up; }
mk c r1 c1 r1c 10.0.1.2/24 10.0.1.1/24
mk c r2 c2 r2c 10.0.2.2/24 10.0.2.1/24
mk r1 s r1s sr1 10.0.11.1/24 10.0.11.2/24
mk r2 s r2s sr2 10.0.12.1/24 10.0.12.2/24
for r in r1 r2; do ip netns exec $r sysctl -qw net.ipv4.ip_forward=1; done
ip netns exec c sysctl -qw net.ipv4.fib_multipath_hash_policy=1       # hash on L4 ports too, so flows spread
ip -n s addr add 10.9.9.9/32 dev lo
ip -n s route add 10.0.1.0/24 via 10.0.11.1; ip -n s route add 10.0.2.0/24 via 10.0.12.1
ip -n r1 route add 10.9.9.9/32 via 10.0.11.2; ip -n r2 route add 10.9.9.9/32 via 10.0.12.2
ip -n c route add 10.9.9.9/32 nexthop via 10.0.1.1 dev c1 nexthop via 10.0.2.1 dev c2
x() { ip netns exec "$@"; }
echo "--- 1. the multipath route on the client"; ip -n c route show 10.9.9.9 | sed 's/^/ /'
pick() { for p in $(seq 40000 40011); do x c ip route get 10.9.9.9 ipproto udp sport $p dport 5000 | grep -oE "dev c[12]"; done | sort | uniq -c | awk '{printf "%s: %s flows  ", $3, $1}'; echo; }
echo "--- 2. which next hop do 12 different UDP flows choose?"; pick
echo "--- 3. cut the link towards r1 (link goes down)"; ip -n c link set c1 down
ip -n c route show 10.9.9.9 | sed 's/^/ /'
echo "--- 4. the same 12 flows now"; pick
echo "--- 5. connectivity survives"; x c ping -c 2 -W 1 10.9.9.9 | grep transmitted
echo "--- 6. restore"; ip -n c link set c1 up; sleep 0.5; ip -n c route show 10.9.9.9 | sed 's/^/ /'; pick
```

Output:

```text
--- 1. the multipath route on the client
 10.9.9.9 
 	nexthop via 10.0.1.1 dev c1 weight 1 
 	nexthop via 10.0.2.1 dev c2 weight 1 
--- 2. which next hop do 12 different UDP flows choose?
c1: 5 flows  c2: 7 flows  
--- 3. cut the link towards r1 (link goes down)
 10.9.9.9 
 	nexthop via 10.0.1.1 dev c1 weight 1 dead linkdown 
 	nexthop via 10.0.2.1 dev c2 weight 1 
--- 4. the same 12 flows now
c2: 12 flows  
--- 5. connectivity survives
2 packets transmitted, 2 received, 0% packet loss, time 1007ms
--- 6. restore
 10.9.9.9 
 	nexthop via 10.0.1.1 dev c1 weight 1 
 	nexthop via 10.0.2.1 dev c2 weight 1 
c1: 5 flows  c2: 7 flows
```

## Exercises

**1. Read the multipath route.** What does the route to `10.9.9.9` with two `nexthop` lines mean, and what does `weight 1` do? Which setting made UDP *flows* choose different paths?

**2. Distribution.** Twelve UDP flows (different source ports) chose 5 and 7 next hops. Is that unbalanced? Why must packets of *one* flow always use the same path?

**3. Failure.** After `ip link set c1 down` the route shows `dead linkdown`. What happened to the twelve flows, and why did the ping still work? What would happen if the *far* side of the path failed instead (the link between `r1` and `s`) while `c1` stayed up?

**4. Detection times.** Compute how long it takes to notice a dead neighbour with (a) VRRP advertisements every 1 s, (b) OSPF defaults (hello 10 s, dead interval 40 s), (c) BFD with 300 ms transmit interval and multiplier 3, and (d) BFD at 50 ms with multiplier 3. Which would you choose for a path that carries voice?

**5. Short answers.** (a) What does VRRP provide that ECMP does not? (b) Why is a stateful firewall in an ECMP path a risk? (c) What is the difference between a *link failure* and a *path failure* for the client?

## Answers

Try before opening.

<details>
<summary>Show the explanations</summary>

**1.** A **multipath (ECMP) route** has several next hops for one destination, and the kernel uses all of them. `weight 1` on each means equal shares: a next hop with weight 2 would receive about twice as many flows. The sysctl **`net.ipv4.fib_multipath_hash_policy=1`** told the kernel to choose the next hop by hashing the packet's **layer 4 five-tuple** (addresses, protocol and *ports*); the default policy hashes only the addresses, which would send every flow between the same two hosts down one path, so a single client-server pair could never use both links.

**2.** 5 and 7 is the kind of spread a hash gives with only twelve samples (the expected split is 6 and 6; with large numbers of flows the shares converge). ECMP balances **flows, not packets**, so one big transfer still uses only one path and cannot exceed that link's speed; ECMP helps when there are many flows. The **same flow must always take the same path** because packets sent over different paths would arrive out of order (different delays), which TCP interprets as loss and reacts to by slowing down, and because stateful devices on a path (firewalls, NAT) need to see every packet of a connection. The hash keeps a flow's packets together, and a flow only changes path when the set of next hops changes.

**3.** Marking the link down made the kernel flag that next hop **`dead linkdown`** (the route still exists in the table but is no longer used), and all twelve flows moved to `c2` immediately (12 on the survivor), with **no ping lost**: the client detected the failure *locally*, from the loss of the interface's carrier, in microseconds. After `ip link set c1 up` the next hop returned and the flows spread again as before (5 and 7). The result is **failover with no routing protocol at all**, for a failure the client can *see*. The hard case is the far one: if the link between `r1` and `s` fails while `c1` stays up, `c` sees nothing wrong, keeps sending half of its flows into `r1`, which has no working route or drops them: **a black hole** until something notices. That is why networks use a routing protocol (neighbours withdraw routes when they stop hearing from each other) and fast failure detectors, and why the next exercise matters.

**4.** Detection time is roughly *interval × number of missed intervals*:

| Mechanism | Calculation | Detection time |
|---|---|---|
| VRRP, advertisement 1 s, master-down interval = 3 × 1 s + skew (up to 1 s) | 3 × 1 s + about 0.6 s | **about 3.6 s** |
| OSPF defaults, dead interval 4 × hello | 4 × 10 s | **40 s** |
| BFD, 300 ms × 3 | 3 × 0.3 s | **0.9 s** |
| BFD, 50 ms × 3 | 3 × 0.05 s | **0.15 s** |

For voice, where a gap of more than a few hundred milliseconds is audible, only the **BFD** numbers are acceptable: OSPF's 40 seconds would drop every call. BFD is a lightweight hello protocol running between the two neighbours at a fast, fixed rate; routing protocols register with it and withdraw routes the moment it reports the neighbour down (*BFD — Bidirectional Forwarding Detection*). Faster is not free: very short intervals consume CPU, can cause false alarms on a busy or jittery link (a flapping route is worse than a slow one), and need hardware support at 50 ms and below. The kernel-visible link failure in exercise 3 is faster still (carrier loss is detected in well under a millisecond) but works only when the failed element is the directly attached link.

**5.** (a) **VRRP** provides a **shared virtual gateway address** (and virtual MAC) for *hosts* that cannot run a routing protocol: they are configured with one default gateway, `10.0.1.1` say, and if the active router fails the standby takes over that same address, so hosts need no change. ECMP, in contrast, spreads traffic across several next hops that the sender must know about; end hosts normally do not do ECMP, routers do. They are used together: VRRP at the access edge for hosts, ECMP in the routed core. (b) A **stateful firewall** keeps a connection table, and with ECMP the *return* packets may take a different path than the outgoing ones and reach a different firewall that has no state for them, which drops them (**asymmetric routing**). Place stateful devices where traffic is symmetric (or synchronise state between a pair, or make the hash symmetric so both directions of a flow use the same device), as discussed in *ACLs, Stateless vs Stateful Filtering and Infrastructure Protection*. (c) A **link failure** is local and visible (carrier lost): the host or router reacts at once, as in exercise 3. A **path failure** is anywhere beyond the first hop (a remote link, a failed router, a misconfigured filter): the local interface stays up, so detection needs a protocol that checks the **end-to-end or neighbour-to-neighbour** state (BFD, routing protocol hellos, IP SLA probes, or application health checks), and the speed of recovery is set by those timers.

</details>

## Key takeaways

1. An ECMP route has several next hops of equal cost; hash the layer 4 five-tuple to spread flows, since the default address-only hash puts a host pair on one path.
2. ECMP balances flows, not packets, and keeps each flow on one path to avoid reordering and to keep stateful devices consistent.
3. A local link failure marks the next hop dead and traffic moves instantly; a remote failure needs a protocol or BFD to detect it, otherwise traffic is black-holed.
4. Detection time is interval times missed count: OSPF 40 s, VRRP about 3.6 s, BFD 0.15 to 0.9 s with typical settings.
5. VRRP gives hosts a shared virtual gateway; ECMP spreads traffic among routers; stateful firewalls need symmetric paths.
