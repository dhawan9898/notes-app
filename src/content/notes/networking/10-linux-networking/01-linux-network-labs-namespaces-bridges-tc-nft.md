---
title: "Linux Network Labs: Namespaces, veth, Bridges, VLANs, tc and nftables"
track: networking
module: Linux Networking Labs
moduleOrder: 10
order: 1
level: intermediate
summary: Build routers, switches, VLANs and impairments from Linux primitives in seconds, with scripts and the real output they produce, so you can reproduce every concept in this track on a laptop.
sources:
  - label: "Linux man-pages — ip-netns(8), ip-link(8), bridge(8), tc(8), nft(8)"
    url: https://man7.org/linux/man-pages/
  - label: "Linux kernel documentation — Networking (bridge, VLAN filtering, vrf, vxlan)"
    url: https://docs.kernel.org/networking/index.html
  - label: "Linux kernel documentation — ip-sysctl (ip_forward, rp_filter, arp_*, conf/*)"
    url: https://docs.kernel.org/networking/ip-sysctl.html
  - label: "nftables wiki"
    url: https://wiki.nftables.org/
  - label: "FRRouting user guide (OSPF, IS-IS, BGP, PIM, EVPN)"
    url: https://docs.frrouting.org/
---

The fastest way to *own* networking concepts is to build them. Linux gives you every device type in this track: **network namespaces** (hosts and routers with independent stacks), **veth pairs** (cables), **bridges** (switches, with VLAN filtering and STP), `vxlan`/`gre`/`vrf` devices, `tc` (QoS and impairments) and `nftables` (filtering, NAT). Everything below runs on one machine, needs only `iproute2` (plus `nftables` for the last lab), and leaves nothing behind when you delete the namespaces.

> All output in this note was produced by running the scripts exactly as written, inside an isolated user/network namespace (`unshare -Urnm`). On a normal system, run them as root; clean up with `ip netns del <name>` (and `ip link del br0`).

## The building blocks

| Real-world device | Linux object | Create |
|---|---|---|
| Host / router (an isolated stack with its own routes, ARP, sockets) | **network namespace** | `ip netns add h1` |
| Cable | **veth pair** | `ip link add a type veth peer name b` |
| Switch | **bridge** | `ip link add br0 type bridge` (+ `vlan_filtering 1` for 802.1Q) |
| NIC port | veth end / dummy / physical | `ip link set a netns h1` |
| Router function | `net.ipv4.ip_forward=1` in a namespace | `ip netns exec r1 sysctl -w ...` |
| VLAN trunk/sub-interface | `vlan` device | `ip link add link eth0 name eth0.10 type vlan id 10` |
| VRF | `vrf` device | `ip link add vrf-red type vrf table 10` |
| Link impairments | `netem` | `tc qdisc add dev eth0 root netem delay 40ms loss 1%` |
| Firewall / NAT | nftables in a namespace | `ip netns exec r1 nft ...` |

Run commands inside a namespace with `ip netns exec <ns> <cmd>` or `ip -n <ns> <ip-command>`.

## Lab 1: a router between two subnets

Reproduces *A Packet's Journey*: two hosts on different subnets, one router.

```text
h1 10.0.1.2/24 --- r1 (10.0.1.1 | 10.0.2.1) --- h2 10.0.2.2/24
```

```bash
set -e
ip netns add h1; ip netns add r1; ip netns add h2
ip link add h1-eth0 type veth peer name r1-eth0
ip link add h2-eth0 type veth peer name r1-eth1
ip link set h1-eth0 netns h1; ip link set r1-eth0 netns r1
ip link set h2-eth0 netns h2; ip link set r1-eth1 netns r1

ip -n h1 addr add 10.0.1.2/24 dev h1-eth0; ip -n h1 link set h1-eth0 up; ip -n h1 link set lo up
ip -n h2 addr add 10.0.2.2/24 dev h2-eth0; ip -n h2 link set h2-eth0 up; ip -n h2 link set lo up
ip -n r1 addr add 10.0.1.1/24 dev r1-eth0; ip -n r1 addr add 10.0.2.1/24 dev r1-eth1
ip -n r1 link set r1-eth0 up; ip -n r1 link set r1-eth1 up; ip -n r1 link set lo up

ip netns exec r1 sysctl -qw net.ipv4.ip_forward=1     # make r1 a router
ip -n h1 route add default via 10.0.1.1
ip -n h2 route add default via 10.0.2.1
```

**Observe the forwarding decision (local vs remote):**

```text
$ ip -n h1 route get 10.0.2.2
10.0.2.2 via 10.0.1.1 dev h1-eth0 src 10.0.1.2 uid 0
```

**TTL decrements at the router** (initial 64, one hop):

```text
$ ip netns exec h1 ping -c 2 10.0.2.2
64 bytes from 10.0.2.2: icmp_seq=1 ttl=63 time=0.089 ms
64 bytes from 10.0.2.2: icmp_seq=2 ttl=63 time=0.109 ms
```

**The ARP entry on h1 is for the *gateway*, not the destination:**

```text
$ ip -n h1 neigh show
10.0.1.1 dev h1-eth0 lladdr a2:ac:83:83:bf:72 REACHABLE
```

**ICMP Time Exceeded comes from the router** when TTL runs out:

```text
$ ip netns exec h1 ping -c 1 -t 1 10.0.2.2
From 10.0.1.1 icmp_seq=1 Time to live exceeded
```

Experiments to try:

1. `ip netns exec r1 sysctl -w net.ipv4.ip_forward=0` and watch ping fail with no reply (the router now treats forwarded packets as not-for-me).
2. Add a second router and a floating static route; remove it and see failover.
3. Run `tcpdump -eni h1-eth0` in `h1` while pinging, and confirm the Ethernet destination is the router's MAC (the destination IP is h2's).
4. Make the link MTU 1400 on r1-eth1 and ping with `-M do -s 1472` to trigger ICMP "Frag needed" (PMTUD).
5. Install FRR in each router namespace and run OSPF between several routers.

## Lab 2: a VLAN-aware switch, learning, and impairments

Reproduces *Ethernet Frames and MAC Learning* and *VLANs*: three hosts on a bridge, two in VLAN 10, one in VLAN 20.

```bash
ip link add br0 type bridge vlan_filtering 1 stp_state 0 ageing_time 3000   # ageing in centiseconds
ip link set br0 up
for h in a b c; do
  ip netns add $h
  ip link add $h-eth0 type veth peer name $h-br
  ip link set $h-eth0 netns $h
  ip link set $h-br master br0 ; ip link set $h-br up
  ip -n $h link set $h-eth0 up ; ip -n $h link set lo up
done
ip -n a addr add 10.10.0.1/24 dev a-eth0
ip -n b addr add 10.10.0.2/24 dev b-eth0
ip -n c addr add 10.10.0.3/24 dev c-eth0      # same subnet as a and b, but VLAN 20

bridge vlan del dev a-br vid 1; bridge vlan del dev b-br vid 1; bridge vlan del dev c-br vid 1
bridge vlan add dev a-br vid 10 pvid untagged    # access port, VLAN 10
bridge vlan add dev b-br vid 10 pvid untagged
bridge vlan add dev c-br vid 20 pvid untagged    # access port, VLAN 20
```

```text
$ bridge vlan show
port              vlan-id
a-br              10 PVID Egress Untagged
b-br              10 PVID Egress Untagged
c-br              20 PVID Egress Untagged
```

**Same VLAN works; a different VLAN is isolated at layer 2 even though the IPs share a subnet:**

```text
$ ping -c 1 10.10.0.2     (from a)     64 bytes from 10.10.0.2: icmp_seq=1 ttl=64 time=0.129 ms
$ ping -c 1 10.10.0.3     (from a)     1 packets transmitted, 0 received, 100% packet loss
```

**The FDB shows what the bridge learned, per VLAN:**

```text
$ bridge fdb show br br0
8a:a7:b1:1f:a8:63 dev a-br vlan 10 master br0
d6:e9:60:1f:01:d4 dev b-br vlan 10 master br0
32:ad:83:4b:ae:e8 dev c-br vlan 20 master br0
```

(Entries are learned from *source* MACs, keyed by VLAN; unknown destinations are flooded only within the VLAN. `ageing_time` is in centiseconds: 3000 = 30 s here; the 802.1Q default is 300 s.)

**Add latency with netem (a WAN simulator):**

```text
$ ip netns exec a tc qdisc add dev a-eth0 root netem delay 40ms
$ ip netns exec a ping -c 3 10.10.0.2
64 bytes from 10.10.0.2: icmp_seq=1 ttl=64 time=40.1 ms
64 bytes from 10.10.0.2: icmp_seq=2 ttl=64 time=40.3 ms
64 bytes from 10.10.0.2: icmp_seq=3 ttl=64 time=40.2 ms
```

netem also does jitter (`delay 50ms 10ms`), loss (`loss 1%`), duplication, reordering, corruption and rate limiting. Combine it with TCP (`iperf3`) to see how loss and RTT limit throughput (bandwidth-delay product).

**Count packets with nftables in the receiver:**

```bash
ip netns exec b nft add table inet t
ip netns exec b nft add chain inet t in '{ type filter hook input priority 0; }'
ip netns exec b nft add rule inet t in icmp type echo-request counter
ip netns exec a ping -c 3 -i 0.2 10.10.0.2
ip netns exec b nft list ruleset
```

```text
table inet t {
	chain in {
		type filter hook input priority filter; policy accept;
		icmp type echo-request counter packets 3 bytes 252
	}
}
```

(Each echo request is 84 bytes of IP: 3 × 84 = 252.)

Experiments to try:

1. Create a loop (connect two bridge ports with a veth pair) with `stp_state 0` and watch the CPU/packet counters explode, then enable `stp_state 1` and look at `bridge link` for blocking ports.
2. Add a trunk port carrying VLAN 10 and 20 (`bridge vlan add dev t-br vid 10`, `vid 20` tagged), attach a VLAN sub-interface in a namespace, and verify tagged frames with `tcpdump -e`.
3. Set a VLAN interface on `br0` (`bridge vlan add dev br0 vid 10 self` + `ip link add link br0 name br0.10 type vlan id 10`) to build an SVI and route between VLANs.
4. `ip link set a-br type bridge_slave isolated on` to emulate a private-VLAN isolated port.
5. Run `ip -d link show br0` to see the bridge parameters (`stp_state`, `ageing_time`, `vlan_filtering`).

## Lab 3 ideas (build these next)

| Topic | Recipe |
|---|---|
| NAT | Router namespace with `nft add rule ip nat postrouting oifname "r1-eth1" masquerade`; inspect with `conntrack -L` |
| Policy routing / VRF | `ip rule add from 10.0.1.0/24 table 100`; or two `vrf` devices with overlapping subnets |
| Link aggregation | two veth pairs between namespaces as `bond` members (`mode 802.3ad`) and read `/proc/net/bonding/bond0` |
| OSPF/BGP | install FRR; run one `frr` instance per namespace (`/etc/frr` per ns, or use containerlab) |
| VXLAN/EVPN | two namespaces as VTEPs, `ip link add vxlan100 type vxlan id 100 local ... dstport 4789`, add FRR for EVPN |
| IPsec / WireGuard | tunnel between namespaces; check `ip xfrm state`, MTU behavior with `ping -M do` |
| Multicast | `ip link set br0 type bridge mcast_snooping 1 mcast_querier 1`; send with `iperf -u -T 4 -c 239.1.1.1`; view `bridge mdb show` |

For larger topologies use **containerlab** (declarative YAML labs with FRR/vendor images), **GNS3**, **EVE-NG** or **Mininet**; the concepts are the same as these primitives.

## Debugging toolbox

| Tool | Use |
|---|---|
| `ip -s link`, `ip -d link` | counters, drops, device details |
| `ip route get`, `ip rule`, `ip -6 route` | the forwarding decision |
| `ip neigh`, `bridge fdb`, `bridge mdb`, `bridge vlan` | L2/L3 adjacency and switching state |
| `tcpdump -eni <if>` | frames with MACs/VLAN; `-w file.pcap` for Wireshark |
| `ss -tulpn`, `ss -ti` | sockets and TCP internals (RTT, cwnd, retransmits) |
| `nstat -az`, `/proc/net/snmp`, `/proc/net/softnet_stat` | protocol and driver counters |
| `ethtool -S`, `ethtool -k` | NIC statistics and offloads (note: offloads change what tcpdump sees) |
| `nft monitor trace`, `conntrack -E` | follow packets through rules and flows |
| `perf trace`, `bpftrace`, `dropwatch` | where the kernel drops packets |
| `mtr`, `ping -M do`, `tracepath`, `iperf3` | path, MTU, throughput |

## Key takeaways

- Namespaces + veth + bridges let you rebuild every L2/L3 concept on one machine in minutes; scripts are the repeatable "lab report".
- A bridge with `vlan_filtering` is an 802.1Q switch: per-port PVID/tagging, per-VLAN FDB.
- `ip route get`, `ip neigh`, and `bridge fdb` show you the exact state the forwarding decision used.
- `tc netem` and `nft` add impairments and policy, so you can reproduce real-world faults before they happen in production.
