# Networking track — topic map (L2 & L3 and related technologies)

Status: [x] written, [ ] planned. Module order = `moduleOrder`.

## 1. Fundamentals
- [x] Network models, encapsulation and device roles
- [x] A packet's journey: host → gateway → host
- [x] Reading RFCs and IEEE standards efficiently

## 2. Layer 2
- [x] Ethernet frames and MAC learning
- [x] Ethernet PHY, autonegotiation, duplex, flow control, MTU
- [x] VLANs, trunking, private VLANs, QinQ
- [x] Spanning Tree: STP, RSTP, MSTP
- [x] Link aggregation (802.1AX / LACP) and hashing
- [x] ARP and IPv4 address conflict detection
- [x] Bridge-reserved addresses and LLDP
- [x] Layer 2 security: port security, DHCP snooping, DAI, 802.1X, MACsec
- [x] Switch internals: CAM, TCAM, buffering, oversubscription

## 3. IPv4
- [x] IPv4 header, fragmentation and MTU
- [x] Addressing, subnetting, VLSM, CIDR, summarization
- [x] ICMP, ping, traceroute, Path MTU Discovery
- [x] Forwarding: routing table, longest-prefix match, ECMP
- [x] DHCP
- [x] NAT: NAPT, CGNAT, hairpinning

## 4. IPv6
- [x] IPv6 header, addressing and extension headers
- [x] ICMPv6, Neighbor Discovery, SLAAC, DAD
- [x] DHCPv6, transition mechanisms (dual-stack, NAT64/DNS64, 464XLAT)

## 5. Routing
- [x] Routing fundamentals: static routes, administrative distance, recursion
- [x] Distance-vector vs link-state; RIP
- [x] OSPFv2: neighbors, DR/BDR, LSAs, SPF
- [x] OSPF areas, LSA types, stub/NSSA, route summarization
- [x] IS-IS
- [x] BGP-4 fundamentals: FSM, messages, path attributes, best-path selection
- [x] BGP at scale: iBGP, route reflectors, communities, policy, convergence
- [x] Route redistribution and policy

## 6. Resilience
- [x] First-hop redundancy: VRRP, HSRP
- [x] BFD
- [x] ACLs and stateless vs stateful filtering

## 7. Overlays and VPNs
- [x] GRE and IP-in-IP
- [x] IPsec: ESP, AH, IKEv2
- [x] MPLS and LDP; L3VPN
- [x] VXLAN and EVPN

## 8. Multicast
- [x] IGMP/MLD and snooping
- [x] PIM sparse mode

## 9. QoS
- [x] DiffServ, classification, queuing, policing and shaping

## 10. Linux networking labs
- [x] ip, bridge, namespaces, veth, tc, nftables

## Networking Essentials (moduleOrder 0.5, beginner)

Added after the continuity review (beginner bridge between *Start Here* and module 1).

- [x] How the Internet Works: From a Browser Click to a Web Page
- [x] IP Addresses, Subnets and Ports in Plain Language
- [x] Home and Office Networks: Switch, Router, Wi-Fi, NAT and Firewall
- [x] Troubleshooting a Network Problem Step by Step
- [x] Practice: Networking Exercises (moved here from Start Here so it follows the subnetting primer)

### Bridging and practice notes (added in the exercises pass)

- [x] Wi-Fi for Beginners: Bands, Channels, Signal and Security (Essentials)
- [x] Planning a Small Office Network: VLANs and an Addressing Plan (Essentials)
- [x] Practice: A Layer 2 Lab with Bridges, VLANs and STP (module 2)
- [x] Practice: VLSM, Summarisation, Longest-Prefix Match and Fragmentation (module 3)
- [x] Practice: IPv6 Addresses and Neighbour Discovery (module 4)
- [x] Practice: A Static Routing Lab (module 5)

### Remaining practice notes (second exercises batch)

- [x] Practice: Building and Parsing a Packet by Hand (Fundamentals)
- [x] Practice: Watching TCP and UDP Behave (Transport and Services)
- [x] Practice: Redundant Paths, ECMP and Failure Detection (Resilience)
- [x] Practice: Building GRE and VXLAN Tunnels and Counting the MTU Cost (Overlays and VPN)
- [x] Practice: Joining and Leaving a Multicast Group (Multicast)
- [x] Practice: Shaping, Queuing and Priority with tc (QoS)
- [x] Practice: Counters, Time and Syslog Arithmetic (Operations and Automation)
