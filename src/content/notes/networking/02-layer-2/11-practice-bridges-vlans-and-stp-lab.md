---
title: "Practice: A Layer 2 Lab with Bridges, VLANs and STP"
track: networking
module: Layer 2
moduleOrder: 2
order: 11
level: intermediate
summary: "A hands-on lab that needs no hardware: three virtual hosts on a Linux bridge to watch MAC learning, a VLAN-aware bridge that isolates two hosts, and the spanning-tree timers, with the exact commands, captured output and the reasoning behind each result."
prerequisites:
  - "Ethernet Frames and MAC Learning"
  - "VLANs, Trunking, Private VLANs and QinQ"
  - "Spanning Tree: STP, RSTP and MSTP"
sources:
  - label: "IEEE 802.1Q — Bridges and Bridged Networks (VLAN tagging and bridge forwarding)"
    url: https://standards.ieee.org/ieee/802.1Q/10323/
  - label: "man 8 bridge, man 8 ip-link, man 8 ip-netns"
    url: https://man7.org/linux/man-pages/man8/bridge.8.html
  - label: "Linux kernel documentation — Ethernet bridging"
    url: https://wiki.linuxfoundation.org/networking/bridge
---

A Layer 2 switch is a MAC table, a flooding rule and a few timers. You can build one in software and watch every part work. The lab below uses **network namespaces** as virtual hosts (each behaves like a separate computer with its own interfaces), **veth pairs** as virtual cables and a **Linux bridge** as the switch. It needs no root on the host: the commands run inside a throwaway user, network and mount namespace.

## Setup

Start a scratch environment (nothing you do here touches your real network). `mount -t tmpfs tmpfs /run` gives `ip netns` a private place to keep namespace names:

```bash
unshare -Urnm bash
mount -t tmpfs tmpfs /run && ip link set lo up
```

Then build the topology: three hosts `h1`, `h2`, `h3`, each connected by a veth pair to one bridge `br0`, with addresses `10.0.0.1`, `.2` and `.3` in `10.0.0.0/24`.

```bash
ip netns add h1; ip netns add h2; ip netns add h3
ip link add br0 type bridge
for i in 1 2 3; do
  ip link add v$i type veth peer name p$i        # v$i stays at the bridge, p$i goes to the host
  ip link set p$i netns h$i
  ip link set v$i master br0; ip link set v$i up
  ip -n h$i addr add 10.0.0.$i/24 dev p$i; ip -n h$i link set p$i up
done
ip link set br0 up
```

## Exercises

**1. Watch MAC learning.** Before any traffic, how many MAC addresses has the bridge learned? Ping from `h1` to `h2`, then list the bridge's forwarding database with `bridge fdb show br br0`. Which addresses appear, on which ports, and why is `h3` listed although it did not take part in the ping?

**2. Isolate with VLANs.** Turn on VLAN filtering and put `h1` and `h3` in VLAN 10 and `h2` in VLAN 20 (untagged access ports). Which pings succeed? What does this prove about broadcast domains, and what would you need for `h1` and `h2` to communicate?

**3. Read the STP settings.** Enable spanning tree on the bridge and show its timers and the port states. What do `hello_time`, `forward_delay` and `max_age` mean, and in which unit does Linux print them? Why does a loop-free lab still need STP in real life?

**4. Short answers.** (a) What does a switch do with a frame whose destination MAC it has not learned? (b) How long does a learned entry live by default, and what happens when the host moves to another port? (c) Why is the bridge itself able to hold an address (and how does a "Layer 3 switch" differ)?

## Answers

Try before opening. The output comes from a real run of exactly these commands (MAC addresses are random and differ on every run).

<details>
<summary>Show answers and captured output</summary>

**1.**

```text
$ bridge fdb show br br0 | grep -v permanent | wc -l        # before any traffic
learned entries: 0
$ ip netns exec h1 ping -c 1 -W 1 10.0.0.2
$ for i in 1 2; do echo -n "h$i MAC "; ip -n h$i -br link show p$i | awk '{print $3}'; done
h1 MAC 4a:76:ab:d5:5e:23
h2 MAC 0a:a2:d7:12:d8:c7
$ bridge fdb show br br0 | grep -v permanent | grep -v "33:33\|01:00:5e"
4a:76:ab:d5:5e:23 dev v1 master br0
0a:a2:d7:12:d8:c7 dev v2 master br0
ca:a8:ea:e2:62:b4 dev v3 master br0
```

At the start the table is empty: a bridge **learns by listening**. When `h1` pinged `h2`, `h1` first sent an ARP request (a **broadcast**, which the bridge floods out of every port except the one it arrived on), so `h2` and `h3` both saw it. The bridge noted the **source MAC** of every frame together with the port it came in on: `h1` is behind `v1`, and when `h2` answered, `h2` is behind `v2`. The third entry is `h3`'s MAC: the host sent some traffic of its own (IPv6 neighbour discovery messages that every Linux host emits when its interface comes up), which the bridge also learned. (Filtered out above: the bridge's own permanent entries and multicast groups `33:33:…` and `01:00:5e:…`.) From now on frames for `h1` or `h2` go out of exactly one port instead of everywhere.

**2.**

```text
$ ip link set br0 type bridge vlan_filtering 1
$ bridge vlan add dev v1 vid 10 pvid untagged     # h1: VLAN 10
$ bridge vlan add dev v2 vid 20 pvid untagged     # h2: VLAN 20
$ bridge vlan add dev v3 vid 10 pvid untagged     # h3: VLAN 10
$ (the default VLAN 1 was removed from the three ports with: bridge vlan del dev vN vid 1)
$ bridge vlan show        # abbreviated
v1 VLAN 10 PVID Egress Untagged
v2 VLAN 20 PVID Egress Untagged
v3 VLAN 10 PVID Egress Untagged
h1 (VLAN 10) -> h3 (VLAN 10): reachable
h1 (VLAN 10) -> h2 (VLAN 20): unreachable
```

`pvid` sets the VLAN assigned to **untagged** frames arriving on the port, and `untagged` strips the tag when frames leave, which makes each port an **access port**: the host knows nothing about VLANs. Frames are only forwarded between ports of the same VLAN, so `h1` and `h3` talk and `h2` is cut off. Notice `h1` could not even **ARP** for `h2`: its broadcast stayed inside VLAN 10, so VLANs split the network into separate **broadcast domains**. For `h1` and `h2` to communicate, traffic has to be **routed** between the two VLANs (they would also need different IP subnets, since each VLAN is its own network): a router, or a Layer 3 switch, with an interface in each VLAN. Connecting two switches that both carry several VLANs needs a **trunk** port that carries tagged frames (`bridge vlan add dev X vid 10` without `untagged`).

**3.**

```text
$ ip link set br0 type bridge stp_state 1
$ bridge link show
v1@if8: ... state forwarding ...
v2@if10: ... state forwarding ...
v3@if12: ... state forwarding ...
$ ip -d link show br0 | grep -o "stp_state [0-9]\|forward_delay [0-9]*\|hello_time [0-9]*\|max_age [0-9]*"
forward_delay 1500 hello_time 200 max_age 2000 stp_state 1
```

(The `bridge link` lines are shortened.) The timers are printed in **hundredths of a second**: `hello_time 200` is 2 s (how often the root bridge sends configuration messages, BPDUs), `max_age 2000` is 20 s (how long a bridge trusts the last BPDU it heard before assuming the root is gone) and `forward_delay 1500` is 15 s (how long a port spends in each of the listening and learning states before forwarding). These are the classic IEEE 802.1D defaults, which is why classic STP needs 30 to 50 seconds to recover from a failure, and RSTP (802.1w) was invented to converge in about a second. In this lab all three ports are in the *forwarding* state because there is no loop to break. In a real network a cable between two switches that are already connected another way creates a loop, and with no STP a broadcast frame would circle forever, multiplying until the network collapses (a **broadcast storm**), because Ethernet frames have no hop count. STP blocks redundant ports to make a loop-free tree and unblocks them when a link fails, so you keep the redundancy without the storm. The bridge priority (default 32768) decides which bridge becomes the **root**; set it deliberately instead of letting the lowest MAC win.

**4.** (a) It **floods** the frame out of every port in the same VLAN except the ingress port ("unknown unicast flooding"); once the destination answers, its MAC is learned and later frames go to a single port. (b) Entries **age out** after the *ageing time*, 300 seconds by default (Linux prints `ageing_time 30000`, in hundredths of a second; change it with `ip link set br0 type bridge ageing_time 6000` for 60 s). When a host moves, its first frame from the new port **overwrites** the old entry, so the table corrects itself immediately; until then, frames toward it go to the old port. (c) A Linux bridge is both a switch and a host: the bridge device `br0` has its own MAC and can hold an IP address (used for management or as the default gateway of the VLAN). A **Layer 3 switch** adds an IP routing table and routes between VLANs in hardware at line rate; a pure Layer 2 switch only forwards by MAC address.

</details>

## Key takeaways

1. A bridge learns MAC addresses from frame *sources*, floods broadcasts and unknown destinations, and sends known destinations out of one port; entries age out after 300 s by default.
2. VLANs split one switch into separate broadcast domains, so hosts in different VLANs cannot even ARP for each other; traffic between VLANs must be routed.
3. Access ports use `pvid untagged`; trunk ports carry tagged frames for several VLANs.
4. STP's defaults (hello 2 s, max age 20 s, forward delay 15 s) explain slow convergence; redundancy without STP causes broadcast storms because Ethernet frames carry no TTL.
