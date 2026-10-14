---
title: "Practice: Joining and Leaving a Multicast Group"
track: networking
module: Multicast
moduleOrder: 8
order: 3
level: intermediate
summary: "Four exercises with a Python program on the loopback interface: joining a multicast group and seeing it in ip maddr, sending one packet to several receivers, what leaving the group does (and the subtlety of interface-level membership), and mapping multicast IP addresses to Ethernet addresses and scopes."
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

```python
import socket, struct, subprocess, time

GROUP, PORT = "239.1.2.3", 5007

def make_receiver(name):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("", PORT))
    mreq = struct.pack("4s4s", socket.inet_aton(GROUP), socket.inet_aton("127.0.0.1"))   # join GROUP on the loopback interface
    s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    s.settimeout(1)
    return s

def maddr():
    out = subprocess.run(["ip", "maddr", "show", "dev", "lo"], capture_output=True, text=True).stdout
    return [l.split()[1] for l in out.splitlines() if "inet " in l]

print("1. multicast groups the loopback interface belongs to:", maddr())
r1, r2 = make_receiver("r1"), make_receiver("r2")
print("   after two sockets joined", GROUP + ":", maddr(), "(one interface membership, shared)")
def igmp_groups():                                     # /proc/net/igmp lists groups as little-endian hex addresses
    groups = []
    for line in open("/proc/net/igmp").read().splitlines()[1:]:
        if not line.startswith("\t"):
            if groups: break                                   # stop at the second interface: only "lo" is wanted
            continue
        h = line.split()[0]
        groups.append(".".join(str(int(h[i:i + 2], 16)) for i in (6, 4, 2, 0)))
    return groups
print("   /proc/net/igmp for lo (groups the kernel reports via IGMP):", igmp_groups())

tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
tx.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton("127.0.0.1"))
tx.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)
tx.sendto(b"one packet, many receivers", (GROUP, PORT))
print("2. receiver 1 got:", r1.recvfrom(100)[0].decode(), "| receiver 2 got:", r2.recvfrom(100)[0].decode())

r2.setsockopt(socket.IPPROTO_IP, socket.IP_DROP_MEMBERSHIP, struct.pack("4s4s", socket.inet_aton(GROUP), socket.inet_aton("127.0.0.1")))
tx.sendto(b"second packet", (GROUP, PORT))
print("3. after receiver 2 left the group: receiver 1 got:", r1.recvfrom(100)[0].decode())
try: print("   receiver 2, which left, ALSO still got:", r2.recvfrom(100)[0].decode(), "(the interface is still a member because receiver 1 is, and IP_MULTICAST_ALL defaults to on)")
except socket.timeout: print("   receiver 2 got nothing")
r1.setsockopt(socket.IPPROTO_IP, socket.IP_DROP_MEMBERSHIP, struct.pack("4s4s", socket.inet_aton(GROUP), socket.inet_aton("127.0.0.1")))
print("   after BOTH left, the interface's groups are:", maddr())
tx.sendto(b"third packet", (GROUP, PORT))
try: r1.recvfrom(100); print("   a packet arrived")
except socket.timeout: print("   a third packet sent to the group arrives nowhere: nobody on this interface is a member")

print("4. group address facts:")
for a in ["224.0.0.1", "224.0.0.2", "224.0.0.251", "239.1.2.3", "232.1.1.1"]:
    first = int(a.split(".")[0]); 
    mac = "01:00:5e:%02x:%02x:%02x" % (int(a.split(".")[1]) & 0x7f, int(a.split(".")[2]), int(a.split(".")[3]))
    scope = "all hosts" if a == "224.0.0.1" else "all routers" if a == "224.0.0.2" else "mDNS (link-local)" if a == "224.0.0.251" else "source-specific multicast range" if a.startswith("232.") else "administratively scoped (private)"
    print(f"   {a:<12} -> Ethernet {mac}   {scope}")
```

```text
$ python3 multicast.py
1. multicast groups the loopback interface belongs to: ['224.0.0.251', '224.0.0.1']
   after two sockets joined 239.1.2.3: ['239.1.2.3', '224.0.0.251', '224.0.0.1'] (one interface membership, shared)
   /proc/net/igmp for lo (groups the kernel reports via IGMP): ['239.1.2.3', '224.0.0.251', '224.0.0.1']
2. receiver 1 got: one packet, many receivers | receiver 2 got: one packet, many receivers
3. after receiver 2 left the group: receiver 1 got: second packet
   receiver 2, which left, ALSO still got: second packet (the interface is still a member because receiver 1 is, and IP_MULTICAST_ALL defaults to on)
   after BOTH left, the interface's groups are: ['224.0.0.251', '224.0.0.1']
   a third packet sent to the group arrives nowhere: nobody on this interface is a member
4. group address facts:
   224.0.0.1    -> Ethernet 01:00:5e:00:00:01   all hosts
   224.0.0.2    -> Ethernet 01:00:5e:00:00:02   all routers
   224.0.0.251  -> Ethernet 01:00:5e:00:00:fb   mDNS (link-local)
   239.1.2.3    -> Ethernet 01:00:5e:01:02:03   administratively scoped (private)
   232.1.1.1    -> Ethernet 01:00:5e:01:01:01   source-specific multicast range
```

## Exercises

**1. Membership.** Which multicast groups did `lo` belong to before any program joined, and which was added when two sockets joined `239.1.2.3`? Why did two sockets add only one entry? What does `/proc/net/igmp` show?

**2. One send, two receivers.** The sender called `sendto` once. How many packets left the sender, and how many receivers got the message? What would unicast have needed?

**3. Leaving.** Receiver 2 leaves the group but still gets the second packet; receiver 1 then leaves too and the third packet reaches nobody. Explain both observations.

**4. Addresses.** For each group in the last block, explain the Ethernet address (`01:00:5e:...`) and the scope. How is the MAC computed, and what is the catch?

## Answers

Try before opening.

<details>
<summary>Show the explanations</summary>

**1.** Before the program joined anything, `lo` was a member of `224.0.0.1` ("all hosts", every IP multicast host on a link is always a member) and `224.0.0.251` (mDNS, joined by a system service on this machine). After the two sockets joined `239.1.2.3` the list gained exactly **one** entry: group membership is a property of the **interface** (the host tells the network "someone here wants this group"), not of each socket, and the kernel counts the sockets that asked, so the second join only increments a reference count. `/proc/net/igmp` is the kernel's table of the groups it reports through **IGMP** (the protocol hosts use to tell routers and switches about memberships); the output shows the same three groups for `lo`, in the file's own little-endian hex which the program decodes. On a real Ethernet interface the host sends IGMP membership reports for each group when it joins and in answer to periodic queries from the router, which is how routers and snooping switches learn which ports have receivers.

**2.** **One packet left the sender** (one `sendto`, one datagram on the wire), and the network or the host's own stack replicates it for each member: both receivers got the message. With unicast the sender would need a separate `sendto` and a separate copy on the wire **per receiver**, so the cost grows with the audience (a video stream to 10,000 viewers would send 10,000 copies from the source), whereas multicast keeps the sender's cost constant and copies packets only where the delivery tree branches, which is why it is used for market-data feeds, IPTV, routing protocols (OSPF uses `224.0.0.5` and `224.0.0.6`) and service discovery. The price: **UDP only** (no connection, no retransmission), the sender gets no feedback, and the network must support multicast routing (PIM) and group management (IGMP), and it is generally **not routed across the public internet**. The sender set `IP_MULTICAST_TTL` to 1 so the packet cannot cross a router: for multicast, the TTL doubles as the scope limit.

**3.** Receiver 2 called `IP_DROP_MEMBERSHIP`, but it **still received the second packet**. Two things combine. Membership is counted per **interface**, and receiver 1 was still a member, so the interface kept receiving the group. And on Linux a socket receives every datagram for its port that arrives on the machine, including groups joined by *other* sockets, unless the socket option **`IP_MULTICAST_ALL`** is switched off (the default is on, for historical compatibility). A small check confirms it: with three sockets on the same port, one a real member, one that joined and left with the default option, and one that joined and left with `IP_MULTICAST_ALL` set to 0, a packet to the group gave:

```text
member                                 received
left, IP_MULTICAST_ALL=1 (default)     received
left, IP_MULTICAST_ALL=0               nothing
```

So a socket should set `IP_MULTICAST_ALL` to 0 (option number 49 in Python, which has no named constant for it) or bind to the group address (`s.bind((GROUP, PORT))`) if it must only see the groups it joined itself. After **both** program sockets had left, the interface's groups returned to the original two and the third packet to the group **arrived nowhere**: the host sent an IGMP *leave* (so routers and snooping switches stop forwarding the group to this interface), and the timeout shows no receiver got it. This is the essential multicast rule: **traffic flows only where there are members, membership is tracked per interface, and IGMP (IPv4) or MLD (IPv6) is the signalling.**

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
