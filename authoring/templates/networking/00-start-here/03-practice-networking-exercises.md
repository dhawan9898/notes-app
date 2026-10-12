---
title: "Practice: Networking Exercises"
track: networking
module: Start Here
moduleOrder: 0
order: 3
level: beginner
summary: "Eight graded exercises: binary addresses, network and broadcast addresses, same-network tests, sizing subnets for host counts, splitting a block, classifying special addresses, summarising routes, and a hands-on lab, with answers verified by a Python script."
prerequisites:
  - "What Is a Network?"
  - IPv4 Addressing, Subnetting, VLSM and CIDR
  - Binary, Hexadecimal and Bits
sources:
  - label: RFC 4632 — Classless Inter-domain Routing (CIDR)
    url: https://www.rfc-editor.org/rfc/rfc4632
  - label: RFC 1918 — Address Allocation for Private Internets
    url: https://www.rfc-editor.org/rfc/rfc1918
  - label: Python ipaddress module documentation
    url: https://docs.python.org/3/library/ipaddress.html
---

Subnetting is a skill: it becomes fast only through practice. Work each exercise on paper first (use the "block size = 256 − mask octet" shortcut from *IPv4 Addressing, Subnetting, VLSM and CIDR*), then check yourself against the answers at the end.

## Exercises

**1. Binary.** Write `192.168.10.77` as four 8-bit binary groups.

**2. Network, mask and size.** For `172.20.5.9/20`: the network address, the subnet mask in dotted form, and the number of usable host addresses.

**3. Range.** For `10.20.77.130/21`: network address, broadcast address, and the first and last usable host.

**4. Same network?** Are these pairs on the same network (so they could talk without a router)?
- `192.168.1.10/24` and `192.168.1.200/24`
- `10.0.0.5/25` and `10.0.0.130/25`
- `172.16.4.1/22` and `172.16.7.250/22`

**5. Sizing.** What is the smallest prefix (largest subnet mask) that gives room for 25 hosts? For 100 hosts? For 500? Remember the network and broadcast addresses.

**6. Splitting.** Split `192.168.5.0/24` into four equal subnets. Write each.

**7. Classify.** Say whether each address is private, link-local, shared (carrier-grade NAT) or public: `10.1.2.3`, `172.32.0.1`, `192.168.255.1`, `169.254.7.7`, `100.64.0.9`, `8.8.8.8`.

**8. Summarise.** Can `10.1.0.0/24`, `10.1.1.0/24`, `10.1.2.0/24` and `10.1.3.0/24` be advertised as one route? Which?

**Lab (hands-on).** Extend the virtual LAN from *What Is a Network?*: add a third namespace `pc3` with address `192.168.1.30/24`, connected to `pc2` by a *second* veth pair (so `pc3` is on a different cable from `pc1`). Predict whether `pc1` can ping `192.168.1.30`, then try it and explain the result. Then build the fix: connect all three hosts to one Linux bridge (a virtual switch; see *Linux Network Labs*) and ping again.

## Answers

<details>
<summary>Show answers and the checking script</summary>

1. `192.168.10.77` = `11000000.10101000.00001010.01001101`.
2. `/20` mask is `255.255.240.0`; the third octet block size is 16, and 5 rounds down to 0 → network **172.20.0.0/20**; hosts = 2^12 − 2 = **4094**.
3. `/21`: block size 8 in the third octet; 77 → 72, so network **10.20.72.0**, broadcast **10.20.79.255**, hosts **10.20.72.1 – 10.20.79.254**.
4. Yes (both in `192.168.1.0/24`); **No** (`10.0.0.0/25` and `10.0.0.128/25` are different networks: they need a router); Yes (both in `172.16.4.0/22`, which covers 4.0 to 7.255).
5. 25 hosts + 2 = 27 addresses → the next power of two is 32 = 2^5 → **/27** (30 usable); 100 hosts → 128 → **/25** (126 usable); 500 hosts → 512 → **/23** (510 usable).
6. Four equal parts of a /24 are /26 blocks of 64 addresses: `192.168.5.0/26`, `.64/26`, `.128/26`, `.192/26`.
7. `10.1.2.3` private; `172.32.0.1` **public** (the private range is 172.16.0.0 to 172.31.255.255 only); `192.168.255.1` private; `169.254.7.7` link-local (a host that failed to get an address from DHCP); `100.64.0.9` shared address space (CGNAT); `8.8.8.8` public.
8. Yes: four consecutive /24s starting on a multiple of 4 (`10.1.0.0`) are exactly **10.1.0.0/22**.
Lab: `pc1` believes `192.168.1.30` is on its own LAN (same /24), so it sends an ARP request out of its only cable. `pc2` is not a switch and does not forward frames between its two cables, so nobody answers: the ping fails (100 % packet loss, typically reported as *Destination Host Unreachable*). A bridge connecting all three interfaces forwards frames between them, so all three hosts become one LAN and the ARP request reaches `pc3`.

The script that produced and checked these answers:

{{include networking/beginner/answers.py}}

{{run networking/beginner :: python3 answers.py}}

</details>

## Key takeaways

1. Subnetting is binary arithmetic with a mask; the block-size shortcut gives the answer in seconds.
2. Same network means same prefix bits; different networks need a router.
3. Size a subnet for hosts + 2, rounded up to a power of two.
4. Know the special ranges so you can recognise them in a capture or log.

**Next:** *ARP and IPv4 Address Conflict Detection*.
