---
title: "IP Addresses, Subnets and Ports in Plain Language"
track: networking
module: Networking Essentials
moduleOrder: 0.5
order: 2
level: beginner
summary: "What an IPv4 address and a subnet mask really are, how to tell whether two hosts are on the same network, the network and broadcast addresses, the block-size shortcut for subnetting, private and special ranges, how ports complete an address, and a verified Python script to check your answers."
prerequisites:
  - What Is a Network?
  - Binary, Hexadecimal and Bits
sources:
  - label: "RFC 791 — Internet Protocol"
    url: https://www.rfc-editor.org/rfc/rfc791
  - label: "RFC 4632 — Classless Inter-domain Routing (CIDR)"
    url: https://www.rfc-editor.org/rfc/rfc4632
  - label: "RFC 1918 — Address Allocation for Private Internets"
    url: https://www.rfc-editor.org/rfc/rfc1918
  - label: "RFC 6890 — Special-Purpose IP Address Registries"
    url: https://www.rfc-editor.org/rfc/rfc6890
  - label: "Python ipaddress module documentation"
    url: https://docs.python.org/3/library/ipaddress.html
---

Every device on an IP network needs an **address**, and the network needs a way to know which addresses are "close by" (reachable directly) and which are "far away" (reachable through a router). That second job is what the **subnet mask** does. This note explains both in plain language, step by step, and gives you a short script to check your answers. It is the gentle foundation for *IPv4 Addressing, Subnetting, VLSM and CIDR*, and it prepares you for the *Networking Exercises* lab.

## An address is just 32 bits

An IPv4 address is a 32-bit number. For people it is written as four **octets** (bytes) in decimal, separated by dots: `192.168.10.77`. Each octet is 8 bits, so each runs from 0 to 255. If you need a reminder of binary, see *Binary, Hexadecimal and Bits*; the only skill needed here is converting a number from 0 to 255 to and from 8 binary digits (128, 64, 32, 16, 8, 4, 2, 1).

## Network part and host part

An address has two parts: the **network part** (identifies the network, like a street name) and the **host part** (identifies the device on that network, like a house number). Where the split falls is written as a **prefix length** after a slash, or as a **subnet mask**:

- `/26` means "the first 26 bits are the network part, the remaining 6 bits identify the host".
- The same thing as a mask is 26 ones followed by 6 zeros: `11111111.11111111.11111111.11000000`, which is `255.255.255.192` in decimal.

To find the network an address belongs to, **AND** the address with the mask bit by bit (keep a bit where the mask has a 1, zero it where the mask has a 0):

```python
import ipaddress

def show(cidr):
    i = ipaddress.ip_interface(cidr)
    n = i.network
    hosts = list(n.hosts())
    print(f"{cidr:<18} network {str(n.network_address):<15} mask {str(n.netmask):<15} broadcast {str(n.broadcast_address):<15} "
          f"first {str(hosts[0]):<15} last {str(hosts[-1]):<15} usable {len(hosts)}")

for c in ["192.168.1.130/24", "192.168.10.77/26", "172.20.5.9/20", "10.1.2.3/30"]:
    show(c)

print()
a = ipaddress.ip_address("192.168.10.77")
print("binary:", ".".join(f"{o:08b}" for o in a.packed))
m = ipaddress.ip_network("0.0.0.0/26").netmask
print("mask  :", ".".join(f"{o:08b}" for o in m.packed))
print("AND   :", ".".join(f"{x & y:08b}" for x, y in zip(a.packed, m.packed)), "=", ipaddress.ip_interface("192.168.10.77/26").network.network_address)

print()
net = ipaddress.ip_network("192.168.10.0/24")
print("splitting", net, "into /26 pieces:")
for sub in net.subnets(new_prefix=26):
    print("  ", sub, "hosts", sub[1], "-", sub[-2])

print()
for t in ["192.168.10.77", "10.0.0.1", "172.16.5.4", "172.32.0.1", "169.254.12.9", "127.0.0.1", "8.8.8.8", "100.64.1.1"]:
    ip = ipaddress.ip_address(t)
    kind = "private (RFC 1918)" if ip.is_private and not ip.is_link_local and not ip.is_loopback and t != "100.64.1.1" else \
           "link-local (APIPA)" if ip.is_link_local else "loopback" if ip.is_loopback else \
           "carrier-grade NAT (100.64/10)" if t == "100.64.1.1" else "public"
    print(f"  {t:<15} {kind}")

print()
same = lambda a_, b_, p: ipaddress.ip_interface(f"{a_}/{p}").network == ipaddress.ip_interface(f"{b_}/{p}").network
print("192.168.10.77 and 192.168.10.100 on the same /26?", same("192.168.10.77", "192.168.10.100", 26))
print("192.168.10.77 and 192.168.10.130 on the same /26?", same("192.168.10.77", "192.168.10.130", 26))
```

Output of the script:

```text
$ python3 subnet.py
192.168.1.130/24   network 192.168.1.0     mask 255.255.255.0   broadcast 192.168.1.255   first 192.168.1.1     last 192.168.1.254   usable 254
192.168.10.77/26   network 192.168.10.64   mask 255.255.255.192 broadcast 192.168.10.127  first 192.168.10.65   last 192.168.10.126  usable 62
172.20.5.9/20      network 172.20.0.0      mask 255.255.240.0   broadcast 172.20.15.255   first 172.20.0.1      last 172.20.15.254   usable 4094
10.1.2.3/30        network 10.1.2.0        mask 255.255.255.252 broadcast 10.1.2.3        first 10.1.2.1        last 10.1.2.2        usable 2

binary: 11000000.10101000.00001010.01001101
mask  : 11111111.11111111.11111111.11000000
AND   : 11000000.10101000.00001010.01000000 = 192.168.10.64

splitting 192.168.10.0/24 into /26 pieces:
   192.168.10.0/26 hosts 192.168.10.1 - 192.168.10.62
   192.168.10.64/26 hosts 192.168.10.65 - 192.168.10.126
   192.168.10.128/26 hosts 192.168.10.129 - 192.168.10.190
   192.168.10.192/26 hosts 192.168.10.193 - 192.168.10.254

  192.168.10.77   private (RFC 1918)
  10.0.0.1        private (RFC 1918)
  172.16.5.4      private (RFC 1918)
  172.32.0.1      public
  169.254.12.9    link-local (APIPA)
  127.0.0.1       loopback
  8.8.8.8         public
  100.64.1.1      carrier-grade NAT (100.64/10)

192.168.10.77 and 192.168.10.100 on the same /26? True
192.168.10.77 and 192.168.10.130 on the same /26? False
```

Read the first block. For `192.168.10.77/26` the script prints network `192.168.10.64`, broadcast `192.168.10.127`, usable hosts `.65` to `.126`, and 62 usable addresses. The second block shows why: the address in binary ends `01001101`, the mask ends `11000000`, and ANDing them leaves `01000000`, which is 64.

## The special addresses in every subnet

In each subnet two addresses are not available for hosts:

- the **network address** (host bits all 0): names the network itself (`192.168.10.64`);
- the **broadcast address** (host bits all 1): "everybody on this network" (`192.168.10.127`).

So a subnet with *h* host bits has 2^h addresses and **2^h − 2 usable** ones. `/24` gives 254, `/26` gives 62, `/30` gives 2 (handy for a link between two routers). `/31` and `/32` are special cases for point-to-point links and single hosts.

## The shortcut: block size

Calculating in binary is slow. For masks that cut inside the last interesting octet, use the **block size** shortcut: it is `256 − (mask octet)`.

- `/26` has mask octet 192, so the block size is 256 − 192 = **64**. Subnets start at multiples of 64: `.0`, `.64`, `.128`, `.192` (the script's third block lists exactly those four).
- To place a host, find the largest multiple of 64 that does not exceed its last octet: for `.77` that is `.64`. The network address is `.64`, the broadcast is the next block's start minus 1 (`.127`), and the usable range is `.65` to `.126`.

For `172.20.5.9/20` the interesting octet is the third one: mask `255.255.240.0`, block size 256 − 240 = **16** in the third octet, so the network is `172.20.0.0` (the largest multiple of 16 not above 5 is 0), the broadcast is `172.20.15.255`, and there are 4,094 usable hosts, matching the script's output.

## Are two hosts on the same network?

Two hosts can talk **directly** only if, using the same mask, their network addresses are equal. Otherwise the sender must hand the packet to its **default gateway** (a router):

- `192.168.10.77` and `192.168.10.100` are on the same `/26` (`.64` to `.127`): **True**.
- `192.168.10.77` and `192.168.10.130` are *not* (`.130` is in the `.128` block): **False**, so traffic goes through a router.

This decision, repeated by every host for every packet, is what makes routing work (*Forwarding: Routing Table, Longest-Prefix Match, Policy Routing and ECMP*). A wrong mask is one of the most common configuration mistakes: a host with a mask that is too long thinks its neighbours are "far away" and sends their traffic to the router, or one with a mask too short tries to reach faraway hosts directly and fails.

## Private, public and special ranges

Not every address is routable on the public internet. Reserved ranges you will meet constantly:

| Range | Name | Meaning |
|---|---|---|
| `10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16` | **private** (RFC 1918) | for internal networks; not routed on the internet; reused by millions of homes and offices; internet access uses NAT (*NAT — Static, NAPT, Port Forwarding, Hairpinning and CGNAT*) |
| `127.0.0.0/8` | loopback | this machine itself (`127.0.0.1`) |
| `169.254.0.0/16` | link-local | self-assigned when DHCP fails: a sign something is wrong |
| `100.64.0.0/10` | carrier-grade NAT | shared address space used by ISPs |
| `224.0.0.0/4` | multicast | one-to-many delivery |
| everything else (e.g. `8.8.8.8`) | public | routed on the internet |

The script's last classification block checks real examples. Note that `172.32.0.1` is *public*: the private block is only `172.16.0.0` to `172.31.255.255`, a classic trap.

## Ports: completing the address

An IP address picks the **machine**. A **port** (a 16-bit number, 0 to 65535) picks the **program** on it. A connection is identified by four values: source address, source port, destination address and destination port. Well-known destination ports:

| Port | Protocol | Port | Protocol |
|---|---|---|---|
| 22 | SSH | 80 | HTTP |
| 53 | DNS | 443 | HTTPS |
| 25 / 587 | email submission | 3389 | remote desktop |
| 67/68 | DHCP | 123 | NTP |

Clients use a random high **ephemeral** source port (typically 32768 to 60999 on Linux), so one machine can keep thousands of simultaneous conversations with the same server. The pair `address:port` (for example `192.168.10.77:51734`) is called a **socket address**; *TCP and UDP: The Transport Layer* explains how TCP and UDP use ports.

## IPv6 in one paragraph

IPv4 has only about 4.3 billion addresses, which ran out. IPv6 uses 128-bit addresses (written in hexadecimal groups, like `2001:db8::1`) and a prefix length with the same idea: network part plus host part. The ideas on this page, prefixes, "same network or via the router", ports and private versus public ranges, all carry over. Details are in *IPv6 Header, Extension Headers and Addressing*.

## Pitfalls

- **Forgetting that two addresses per subnet are unusable** (network and broadcast).
- **Counting from the wrong octet** when the mask boundary is not at the last octet: find which octet is "interesting" first.
- **Assuming `172.x` is private**: only `172.16.0.0/12` is.
- **Ignoring the mask** when diagnosing: the same two addresses can be neighbours with one mask and strangers with another.
- **Overlapping subnets**: two networks that share addresses cannot be routed correctly.
- **Doing arithmetic by gut feeling**: use the block-size method or the script and check.

## Key takeaways

1. An IPv4 address is 32 bits; the mask or prefix length splits it into a network part and a host part.
2. ANDing the address with the mask gives the network address; host bits all 0 is the network, all 1 is the broadcast, and usable hosts = 2^h − 2.
3. The block-size shortcut (256 − mask octet) finds subnet boundaries quickly.
4. Hosts talk directly only within the same network; otherwise they use the default gateway.
5. Know the private, loopback, link-local and public ranges, and remember that ports identify the program on a machine.
