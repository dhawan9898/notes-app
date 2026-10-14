---
title: "Start Here: How to Use the Networking Track"
track: networking
module: Start Here
moduleOrder: 0
order: 1
level: beginner
summary: "Who the Networking track is for, what to know first, three reading paths from first ping to BGP and EVPN, and how to practise safely with virtual labs on your own computer."
sources:
  - label: "Kurose & Ross — Computer Networking, A Top-Down Approach"
  - label: "Tanenbaum & Wetherall — Computer Networks, 5th ed."
  - label: "IETF — RFC Editor (the Internet's standards)"
    url: https://www.rfc-editor.org/
---

Networking is how computers talk: how a laptop reaches a web server across a building, a city and an ocean. This track covers the **lower layers** that carry everything else: Layer 2 (Ethernet, switches, VLANs, spanning tree) and Layer 3 (IP addressing, routing protocols, IPv6, multicast, tunnels, VPNs, QoS), plus the Linux tools to build and observe all of it yourself. It is grounded in the standards (IEEE 802 and IETF RFCs) so you can check any claim at the source.

## What you need first

Comfort with the command line (see **Linux Command Line Basics** in the Linux track) and with binary and hexadecimal numbers (see **Binary, Hexadecimal and Bits** in the C track; ten minutes there makes subnetting easy). No networking experience is assumed for *What Is a Network?*.

## Reading paths

| You are… | Read in this order |
|---|---|
| **New to networking** | *What Is a Network?* → the seven **Networking Essentials** notes (*How the Internet Works*, *IP Addresses, Subnets and Ports in Plain Language*, *Home and Office Networks*, *Troubleshooting a Network Problem Step by Step*, *Practice: Networking Exercises*, *Wi-Fi for Beginners*, *Planning a Small Office Network*) → *Network Models, Encapsulation and Device Roles* → *A Packet's Journey* → *Ethernet Frames and MAC Learning* → *IPv4 Addressing, Subnetting, VLSM and CIDR* → *ARP*, *ICMP, Ping, Traceroute* → *Forwarding* → *Routing Fundamentals* |
| **Sysadmin / developer who needs the networking layer** | *A Packet's Journey* → modules 2 and 3 (Layer 2 and IPv4) → *NAT* → *DHCP* → *Linux Network Labs* → *IPv6* |
| **Network engineer going deeper** | modules 5 to 8 in order (Routing, Resilience, Overlays and VPN, Multicast), then *QoS* and *Switch Internals* |

## How to practise

You do not need hardware. Every lab in this track runs on a Linux computer using **network namespaces** (each one behaves like a separate computer) and **virtual Ethernet cables**. *What Is a Network?* shows the smallest example; *Linux Network Labs* builds bridges, VLANs, firewalls and traffic control. Because these run inside a throwaway user namespace, you do not need root and cannot damage your real network configuration.

Two habits that make you good at networking quickly:

1. **Predict, then observe.** Before you run `ping` or `ip route`, say what you expect to happen and which devices handle the packet. Then capture the traffic (`tcpdump`, Wireshark) and compare.
2. **Always ask "what changes at each hop?"** Which addresses stay the same end to end (IP), and which are rewritten at each link (MAC). Most confusion disappears with this question (see *A Packet's Journey*).

## How the notes are organised

Each note starts from the standard's mechanism, explains why it is designed that way, shows how to observe it on Linux, lists failure modes, and ends with **Key takeaways** and the exact RFC or IEEE sections used. Unknown terms are in the [Glossary](../../../glossary/). Command output shown in notes was produced on a Linux machine with throwaway namespaces; where a value is machine-specific (MAC addresses, timings) the note says so.

## Key takeaways

1. The Layer 2 / Layer 3 split organises the whole track: local delivery by MAC address, end-to-end delivery by IP address.
2. You can build and test realistic networks with namespaces on your own computer.
3. Predict, capture, compare.
