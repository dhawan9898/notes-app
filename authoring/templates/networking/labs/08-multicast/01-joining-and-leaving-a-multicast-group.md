---
title: Joining and Leaving a Multicast Group
track: networking
module: Multicast
moduleOrder: 8
order: 1
level: intermediate
minutes: 40
env: browser
terminal:
  - ip link set lo up
  - ./multicast
summary: "Four exercises with a C program on the loopback interface: joining a multicast group from two sockets, sending one packet to several receivers, what leaving the group does (and the subtlety of interface-level membership and IP_MULTICAST_ALL), and mapping multicast IP addresses to Ethernet addresses and scopes."
prerequisites:
  - "IP Multicast Basics — Addressing, IGMP, MLD and Snooping"
  - "PIM Sparse Mode, SSM and Multicast Routing"
sources:
  - label: "RFC 1112 — Host Extensions for IP Multicasting"
    url: https://www.rfc-editor.org/rfc/rfc1112
  - label: "RFC 3376 — Internet Group Management Protocol, Version 3"
    url: https://www.rfc-editor.org/rfc/rfc3376
  - label: "RFC 5771 — IANA Guidelines for IPv4 Multicast Address Assignments"
    url: https://www.rfc-editor.org/rfc/rfc5771
  - label: "man 7 ip (IP_ADD_MEMBERSHIP, IP_MULTICAST_TTL), man 8 ip-maddress"
    url: https://man7.org/linux/man-pages/man7/ip.7.html
---

Multicast sends **one packet to many receivers** without the sender knowing who they are: receivers *join* a group address and the network delivers copies only where members exist. The program below uses the loopback interface so everything happens on your machine; read the output, then answer the questions.

{{include vm/multicast.c}}

{{vm :: ip link set lo up ;; ./multicast}}

## Exercises

**1. Membership.** Two sockets joined `239.1.2.3` on `lo`. Why does the kernel keep one membership for both of them, and what would `ip maddr show dev lo` and `/proc/net/igmp` list on a real machine?

**2. One send, two receivers.** The sender called `sendto` once. How many packets left the sender, and how many receivers got the message? What would unicast have needed?

**3. Leaving.** Receiver 2 leaves the group but still gets the second packet; receiver 1 then leaves too and the third packet reaches nobody. Explain both observations.

**4. Addresses.** For each group in the last block, explain the Ethernet address (`01:00:5e:...`) and the scope. How is the MAC computed, and what is the catch?

## Answers

Try before opening.

<details>
<summary>Show the explanations</summary>

**1.** Group membership is a property of the **interface** (the host tells the network "someone here wants this group"), not of each socket: the kernel counts the sockets that asked, so the second join only increments a reference count and `lo` gets exactly **one** entry for `239.1.2.3`. On a real machine `ip maddr show dev lo` and `cat /proc/net/igmp` show that table: `224.0.0.1` ("all hosts", every IP multicast host is always a member) and usually `224.0.0.251` (mDNS, joined by a system service), plus `239.1.2.3` with two users while the program runs. (The in-browser Linux kernel is built without IGMP, so its `/proc/net/igmp` does not exist and the program does not print the table: the delivery results show the same behaviour.) `/proc/net/igmp` is the kernel's table of the groups it reports through **IGMP** (the protocol hosts use to tell routers and switches about memberships). On a real Ethernet interface the host sends IGMP membership reports for each group when it joins and in answer to periodic queries from the router, which is how routers and snooping switches learn which ports have receivers.

**2.** **One packet left the sender** (one `sendto`, one datagram on the wire), and the network or the host's own stack replicates it for each member: both receivers got the message. With unicast the sender would need a separate `sendto` and a separate copy on the wire **per receiver**, so the cost grows with the audience (a video stream to 10,000 viewers would send 10,000 copies from the source), whereas multicast keeps the sender's cost constant and copies packets only where the delivery tree branches, which is why it is used for market-data feeds, IPTV, routing protocols (OSPF uses `224.0.0.5` and `224.0.0.6`) and service discovery. The price: **UDP only** (no connection, no retransmission), the sender gets no feedback, and the network must support multicast routing (PIM) and group management (IGMP), and it is generally **not routed across the public internet**. The sender set `IP_MULTICAST_TTL` to 1 so the packet cannot cross a router: for multicast, the TTL doubles as the scope limit.

**3.** Receiver 2 called `IP_DROP_MEMBERSHIP`, but it **still received the second packet**. Two things combine. Membership is counted per **interface**, and receiver 1 was still a member, so the interface kept receiving the group. And on Linux a socket receives every datagram for its port that arrives on the machine, including groups joined by *other* sockets, unless the socket option **`IP_MULTICAST_ALL`** is switched off (the default is on, for historical compatibility). Step 3b of the output confirms it: with three sockets on the same port, one a real member, one that joined and left with the default option, and one that joined and left with `IP_MULTICAST_ALL` set to 0, a packet to the group reached the member and the default socket but not the third.

So a socket should set `IP_MULTICAST_ALL` to 0 with `setsockopt` (the program defines the Linux option number 49 itself, because some libc headers do not name it) or `bind()` to the group address instead of `INADDR_ANY` if it must only see the groups it joined itself. After **both** program sockets had left, the third datagram to the group **arrived nowhere**: the host sent an IGMP *leave* (so routers and snooping switches stop forwarding the group to this interface), and the timeout shows no receiver got it. This is the essential multicast rule: **traffic flows only where there are members, membership is tracked per interface, and IGMP (IPv4) or MLD (IPv6) is the signalling.**

**4.**

| Group | Ethernet address | Meaning |
|---|---|---|
| `224.0.0.1` | `01:00:5e:00:00:01` | all hosts on the link |
| `224.0.0.2` | `01:00:5e:00:00:02` | all routers on the link |
| `224.0.0.251` | `01:00:5e:00:00:fb` | mDNS, link-local |
| `239.1.2.3` | `01:00:5e:01:02:03` | administratively scoped (private) range `239.0.0.0/8` |
| `232.1.1.1` | `01:00:5e:01:01:01` | source-specific multicast range `232.0.0.0/8` |

IPv4 multicast addresses are `224.0.0.0/4` (first octet 224 to 239). The Ethernet address is **computed**, not looked up (no ARP): the fixed prefix `01:00:5e`, a zero bit, then the **low 23 bits** of the group address. The catch is that an IPv4 group has 28 significant bits but only 23 fit, so **32 different groups map to the same MAC** (for example `224.1.1.1` and `225.129.1.1` share one), and a host's network card may receive frames for groups it did not join; the IP layer then discards them. Address ranges: `224.0.0.0/24` is **link-local** (never forwarded by routers: routing protocols, `224.0.0.5` OSPF, `224.0.0.18` VRRP), `232.0.0.0/8` is for **source-specific multicast** (receivers name both the group and the source, avoiding the need to discover sources), and `239.0.0.0/8` is **administratively scoped**, the multicast equivalent of private addresses, which you should use for your own applications. IPv6 uses `ff00::/8` with the group's low 32 bits copied into `33:33:xx:xx:xx:xx`.

</details>

## Key takeaways

1. Multicast sends one packet to a group address; the network copies it only where members exist, so the sender's cost does not grow with the audience.
2. Hosts announce membership per interface with IGMP (IPv4) or MLD (IPv6); the first join sends a report and the last leave sends a leave.
3. On Linux a socket sees group traffic that another socket's membership brought in unless `IP_MULTICAST_ALL` is 0 or the socket is bound to the group address.
4. Multicast MAC addresses are derived from the IP address (`01:00:5e` plus 23 bits), so 32 groups share each MAC; use `239.0.0.0/8` for private applications and TTL 1 to stay on the link.
