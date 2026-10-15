---
title: "What Is a Network?"
track: networking
module: Getting Started
moduleOrder: 0
order: 1
level: beginner
summary: "The ideas behind every network in plain language: devices, links, addresses, protocols and layers, how two computers on one cable talk (with a real virtual LAN you can reproduce), what a router and a default gateway add, how names become addresses (DNS), and what latency, bandwidth and throughput mean."
prerequisites:
  - Linux Command Line Basics
sources:
  - label: "Kurose & Ross — Computer Networking, A Top-Down Approach, Ch. 1 (Computer Networks and the Internet)"
  - label: "RFC 1122 — Requirements for Internet Hosts, Communication Layers"
    url: https://www.rfc-editor.org/rfc/rfc1122
  - label: "RFC 1034 — Domain Names, Concepts and Facilities"
    url: https://www.rfc-editor.org/rfc/rfc1034
  - label: "ip(8) and ip-netns(8) — Linux iproute2 manual pages"
    url: https://man7.org/linux/man-pages/man8/ip.8.html
---

A **network** is two or more computers connected so they can exchange data. The data travels as **packets**: small chunks, each labelled with who it is from and who it is for, sent one at a time and reassembled at the other end. Everything in this track (switches, routers, VLANs, BGP) exists to get those labelled packets to the right place reliably.

## The parts

- **Host**: any computer, phone or server that sends or receives data.
- **Link**: the connection between devices (an Ethernet cable, Wi-Fi, a fibre between cities).
- **Interface** (network card, NIC): a host's plug into a link. A computer with two interfaces is attached to two networks.
- **Switch**: joins many hosts into one **local network** (LAN) by passing frames between its ports.
- **Router**: joins *different* networks together and decides where to send each packet next.
- **Protocol**: an agreed set of rules and message formats so that devices from different vendors understand each other (Ethernet, IP, TCP, DNS...). Protocols are published as standards: the Internet's are **RFCs**.

A **LAN** (local area network) is the set of hosts that can reach each other directly through switches: a floor of a building, a home. A **WAN** (wide area network) links LANs across distance through routers: the Internet is a network of networks.

## Addresses: who is who

Two kinds of address work together, and keeping them apart is the key to networking:

| | **MAC address** | **IP address** |
|---|---|---|
| Identifies | one network interface | one interface *on a network* |
| Looks like | `aa:bb:cc:11:22:33` (48 bits) | `192.168.1.10` (32 bits, IPv4) |
| Works | only on the **local link** (Layer 2) | **end to end** across networks (Layer 3) |
| Assigned by | the hardware maker (can be changed in software) | the administrator or DHCP |

An IP address has two parts: a **network part** (which LAN) and a **host part** (which machine on it), split by the **prefix length**: `192.168.1.10/24` means the first 24 bits (`192.168.1`) name the network. Two hosts with the same network part are on the **same LAN** and talk directly; for anything else they send to a router. (How to work this out is the subject of *IPv4 Addressing, Subnetting, VLSM and CIDR*, and the binary background is in *Binary, Hexadecimal and Bits*.)

## Layers: dividing the work

Networking is split into **layers**, each doing one job and using the layer below it:

| Layer | Job | Examples |
|---|---|---|
| Application | what the program wants to say | HTTP (web), DNS, SSH |
| Transport | delivery between *programs* (ports), reliability | TCP, UDP |
| Network (Layer 3) | delivery between *hosts across networks* | IP, routing |
| Link (Layer 2) | delivery between *neighbouring devices* | Ethernet, Wi-Fi, switches |
| Physical | bits as signals | cable, light, radio |

As data goes down the stack on the sender, each layer **wraps** it with its own header (**encapsulation**); on the receiver each layer unwraps its own header. A web request becomes a TCP segment, inside an IP packet, inside an Ethernet frame. This track lives at Layers 2 and 3.

## Two computers on one cable: a real example

You can reproduce a LAN on your own computer with no hardware. The script below creates two "computers" (Linux network namespaces `pc1` and `pc2`), joins them with a virtual Ethernet cable, gives them addresses in the same network, and runs `ping`. It runs as an ordinary user inside a throwaway namespace, so it changes nothing on your real system.

```bash
#!/bin/sh
# Builds two "computers" (network namespaces) joined by a virtual cable, as an unprivileged user inside a throwaway user+network+mount namespace.
set -e
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
ip netns add pc1; ip netns add pc2
ip link add v1 type veth peer name v2                 # a virtual Ethernet cable with a plug at each end
ip link set v1 netns pc1; ip link set v2 netns pc2
ip -n pc1 addr add 192.168.1.10/24 dev v1; ip -n pc2 addr add 192.168.1.20/24 dev v2
ip -n pc1 link set v1 up; ip -n pc2 link set v2 up; ip -n pc1 link set lo up
echo '$ ip -br addr        (on pc1)';              ip -n pc1 -br addr | grep -E "^(lo|v1)"
echo '$ ip route           (on pc1)';              ip -n pc1 route
echo '$ ip neigh           (on pc1, before any traffic)'; ip -n pc1 neigh
echo '$ ping -c 3 192.168.1.20   (pc1 -> pc2)';    ip netns exec pc1 ping -c 3 192.168.1.20 | grep -E "bytes from|transmitted|rtt"
echo '$ ip neigh           (on pc1, after the ping)'; ip -n pc1 neigh | sed 's/\(..:..:..:..:..:..\)/<mac>/'
echo '$ ip route get 8.8.8.8   (pc1 has no route to the Internet)'; ip -n pc1 route get 8.8.8.8 2>&1 || true
ip -n pc1 route add default via 192.168.1.20
echo '$ ip route add default via 192.168.1.20 ; ip route get 8.8.8.8'; ip -n pc1 route get 8.8.8.8 | head -1 | sed 's/ uid [0-9]*//'
```

```text
$ unshare -Urnm sh lan.sh 2>&1
$ ip -br addr        (on pc1)
lo               UNKNOWN        127.0.0.1/8 ::1/128 
v1@if7           UP             192.168.1.10/24 fe80::d0eb:c3ff:fea3:f0f4/64 
$ ip route           (on pc1)
192.168.1.0/24 dev v1 proto kernel scope link src 192.168.1.10 
$ ip neigh           (on pc1, before any traffic)
$ ping -c 3 192.168.1.20   (pc1 -> pc2)
64 bytes from 192.168.1.20: icmp_seq=1 ttl=64 time=0.125 ms
64 bytes from 192.168.1.20: icmp_seq=2 ttl=64 time=0.050 ms
64 bytes from 192.168.1.20: icmp_seq=3 ttl=64 time=0.050 ms
3 packets transmitted, 3 received, 0% packet loss, time 2056ms
rtt min/avg/max/mdev = 0.050/0.075/0.125/0.035 ms
$ ip neigh           (on pc1, after the ping)
192.168.1.20 dev v1 lladdr <mac> REACHABLE 
$ ip route get 8.8.8.8   (pc1 has no route to the Internet)
RTNETLINK answers: Network is unreachable
$ ip route add default via 192.168.1.20 ; ip route get 8.8.8.8
8.8.8.8 via 192.168.1.20 dev v1 src 192.168.1.10 
```

Read the output step by step:

1. **`ip -br addr`**: pc1 has interface `v1` with address `192.168.1.10/24`. The `/24` says "the network is `192.168.1.0`".
2. **`ip route`**: one line: *to reach anything in 192.168.1.0/24, send it out `v1` directly*. That is the **routing table**: a list of "to reach this network, go this way". It was created automatically when the address was added.
3. **`ip neigh` (empty)**: pc1 does not yet know pc2's MAC address.
4. **`ping`**: pc1 sends an ICMP echo request to `192.168.1.20`; pc2 replies. To build the Ethernet frame, pc1 first needed pc2's MAC, so before the first ping packet it asked the LAN "who has 192.168.1.20?" with **ARP** (see *ARP and IPv4 Address Conflict Detection*) and pc2 answered. The times (about 0.05 ms) are tiny because the "cable" is virtual.
5. **`ip neigh` again**: pc1 now has pc2's MAC address in its neighbour (ARP) table (shown as `<mac>`; the real value is random on each run). This shows both address types working together: IP address chosen by the sender, MAC address *discovered* for the final delivery on this link.
6. **No route to 8.8.8.8**: pc1 knows only its own LAN, so a destination outside it is `Network is unreachable`.
7. **Default route**: adding `default via 192.168.1.20` says "for everything else, hand the packet to 192.168.1.20". That address is the **default gateway**: the router that leads out of the LAN. Now `route get 8.8.8.8` shows the next hop is the gateway. In a real network the gateway would be a router connected to other networks; here pc2 is playing that role (it would need forwarding turned on to actually pass traffic on).

## Router and gateway: leaving the LAN

When a host wants to send to an address outside its own network, it addresses the **Ethernet frame to the router's MAC** but keeps the **IP packet addressed to the final destination**. The router receives the frame, looks at the IP destination, chooses the next hop from its routing table, and builds a *new* frame for the next link. So **IP addresses stay the same end to end while MAC addresses change at every hop**: this is the most useful sentence in the whole track, and *A Packet's Journey* walks through it field by field.

## Names: DNS

People use names (`example.com`); packets need IP addresses. The **Domain Name System (DNS)** is the Internet's phone book: a distributed database that your computer queries (through a **resolver**, usually run by your router or ISP) to turn a name into one or more addresses. Your system also has a local file, `/etc/hosts`, checked first. On Linux, `getent hosts name` asks the system's resolver exactly the way programs do:

```text
$ getent hosts localhost
::1             localhost ip6-localhost ip6-loopback
```

DNS is why "the Internet is down" is often really "DNS is down": addresses still work but names do not. You can test with `ping 8.8.8.8` (an address) versus `ping example.com` (a name).

## Latency, bandwidth, throughput

- **Bandwidth**: how much data a link *can* carry per second (for example 1 Gbit/s). It is like the width of a pipe.
- **Latency**: how long one packet takes to get there (and back, for round-trip time, **RTT**, which `ping` reports). It is like the length of the pipe. Distance and queueing dominate it: light in fibre takes about 5 ms per 1,000 km.
- **Throughput**: how much useful data is *actually* delivered per second, always less than bandwidth after overhead, loss and congestion.

A fast pipe does not make a long pipe shorter: downloading a big file is limited by bandwidth, but loading a web page of many small requests is limited by latency.

## Pitfalls for beginners

- Mixing up MAC and IP addresses: the MAC changes every hop; the IP does not (apart from NAT).
- Believing a switch routes: switches forward within one LAN by MAC address; routers connect LANs by IP address.
- Using the wrong prefix length: two hosts that think they are on different networks will not talk directly.
- Forgetting the default gateway: hosts can reach their LAN but nothing else.
- Blaming the network when it is DNS (test with an IP address).

## Key takeaways

1. A network moves labelled packets between hosts; switches build LANs, routers join networks.
2. MAC addresses work on one link; IP addresses work end to end; the prefix length splits an IP address into network and host parts.
3. Protocols are layered: each layer wraps the data from the layer above (encapsulation).
4. A host sends local traffic directly and everything else to its default gateway, using its routing table; ARP finds the MAC address of the next hop.
5. DNS turns names into addresses; latency, bandwidth and throughput are three different things.

**Next:** *Network Models, Encapsulation and Device Roles*, then *A Packet's Journey*.
