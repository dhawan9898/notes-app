---
title: "Home and Office Networks: Switch, Router, Wi-Fi, NAT and Firewall"
track: networking
module: Networking Essentials
moduleOrder: 0.5
order: 3
level: beginner
summary: "What each box in a typical home or small-office network actually does: the switch, the router, the wireless access point, DHCP and DNS helpers, NAT and the firewall, how a laptop's packet to the internet crosses them, and how this simple picture grows into the campus and data-centre designs in the main track."
prerequisites:
  - What Is a Network?
  - "How the Internet Works: From a Browser Click to a Web Page"
sources:
  - label: "Kurose & Ross — Computer Networking: A Top-Down Approach, Ch. 1 and 4 (NAT, DHCP)"
  - label: "RFC 1918 — Address Allocation for Private Internets"
    url: https://www.rfc-editor.org/rfc/rfc1918
  - label: "RFC 3022 — Traditional IP Network Address Translator (Traditional NAT)"
    url: https://www.rfc-editor.org/rfc/rfc3022
  - label: "RFC 2131 — Dynamic Host Configuration Protocol"
    url: https://www.rfc-editor.org/rfc/rfc2131
  - label: "IEEE 802.11 and IEEE 802.3 standards overview"
    url: https://www.ieee802.org/
---

The "router" in your home is really **several devices in one box**: a switch, a wireless access point, a router, a DHCP server, a DNS forwarder, a NAT gateway and a firewall. Companies sell them separately and in larger versions, but the jobs are the same. Understanding each job separately is the key to reading every later note in this track, because a big network is mostly these same pieces, multiplied and made redundant.

## The picture

```text
 Laptop ---(Wi-Fi)---+
                     |                      +----------------------------+
 Phone  ---(Wi-Fi)---+--[ access point ]----|  home "router" box         |-----> ISP ----> Internet
                     |                      |  switch + router + NAT +   |      (public address
 PC     ---(cable)---+--[ switch ports ]----|  firewall + DHCP + DNS     |       203.0.113.45)
                                            +----------------------------+
   private addresses: 192.168.1.x                                  public address on this side
```

Everything inside uses **private addresses** (`192.168.1.x`); only one **public address** faces the ISP.

## The switch: connects devices on one network

A **switch** connects devices in the same local network (a **LAN**). It learns which **MAC address** is behind which port, then forwards each Ethernet frame only to the port that needs it (*Ethernet Frames and MAC Learning*). Before switches there were **hubs**, which repeated every frame to every port; they are obsolete. Switches work at **Layer 2** and do not look at IP addresses. If you have ever plugged cables into the numbered sockets on the back of your router, you used its built-in switch. Larger networks add VLANs (virtual separate LANs on the same switches, see *VLANs, Trunking, Private VLANs and QinQ*).

## The access point: Wi-Fi is just a wireless port

A **wireless access point (AP)** lets Wi-Fi devices join the same LAN as the cabled ones. It converts between radio and Ethernet; to the rest of the network, a Wi-Fi laptop looks like a cabled one. Wi-Fi differs from cable in that the medium is shared and noisy (devices take turns, signals fade and collide), and it needs encryption and authentication, such as WPA2 or WPA3 (*Wi-Fi (IEEE 802.11): Medium Access, Frames, Security and Roaming*). Several APs sharing one network name give the "roaming" you experience when walking through a building.

## The router: connects networks to each other

A **router** connects *different* networks and forwards packets between them using **IP addresses** (Layer 3). Your home router joins exactly two: your private LAN (`192.168.1.0/24`) and the ISP's network. Every device is configured with the router's LAN address as its **default gateway**: "if the destination is not on my network, send it here". The router looks up the destination in its routing table and forwards the packet onward (*Forwarding: Routing Table, Longest-Prefix Match, Policy Routing and ECMP*); in a home it has essentially one rule, "everything else goes to the ISP".

**The difference to remember: a switch moves frames *within* a network using MAC addresses; a router moves packets *between* networks using IP addresses.**

## DHCP: how devices get their settings

A device joining the network does not know its address. It broadcasts a request, and a **DHCP server** (in the home, the router box) replies with a **lease**: an IP address from a pool (for example `192.168.1.20` to `192.168.1.200`), the subnet mask, the default gateway, the DNS server and how long the lease lasts (*DHCP — Messages, Leases, Relay and Options*). If DHCP fails, many systems give themselves a `169.254.x.x` address, the telltale sign of "the device could not get an address". Fixed addresses for servers and printers are done with a **DHCP reservation** (the same address always goes to the same MAC) or by configuring a static address outside the pool.

## DNS: the helper that translates names

Devices are told a DNS server by DHCP. Home routers usually act as a **forwarder**: they accept the query and pass it to the ISP's resolver (or a public one such as `1.1.1.1`), caching answers (*DNS: Names, Resolvers and Records*). This is why changing the DNS setting on the router changes it for the whole house.

## NAT: many devices, one public address

There are not enough public IPv4 addresses for every device, and the ISP gives a home just one. **NAT** (more precisely **NAPT**, network address *and port* translation) lets all the private devices share it. When your laptop (`192.168.1.20`, source port 51734) connects to a web server, the router rewrites the packet's source to its own public address and a unique port, remembers the mapping in a **translation table**, and reverses it for the reply:

| Inside (private) | Outside (as seen by the internet) |
|---|---|
| `192.168.1.20:51734` | `203.0.113.45:40001` |
| `192.168.1.31:51734` | `203.0.113.45:40002` |
| `192.168.1.20:60112` | `203.0.113.45:40003` |

Two consequences that surprise people:

- **Connections started from the inside work automatically; connections started from the outside do not**, because there is no table entry yet. That is a side effect that looks like security but is not a firewall. To host a service at home you configure **port forwarding** ("send incoming port 443 to `192.168.1.50`").
- Some protocols embed addresses in their data or want direct peer-to-peer paths (voice calls, games), and they need helper tricks. IPv6 removes the need for NAT by giving every device a public address, which makes a real firewall essential.

More in *NAT — Static, NAPT, Port Forwarding, Hairpinning and CGNAT*. (Some ISPs add a second NAT layer, called CGNAT, where even your "public" address is shared: you will see `100.64.x.x` on your router's WAN side.)

## The firewall: deciding what is allowed

A **firewall** is a set of rules that permit or block traffic. Home routers ship with a **stateful** firewall: it remembers the connections you opened from the inside and automatically allows the matching replies, while dropping unsolicited traffic from the outside. Larger networks add rules between internal zones (guests versus staff versus servers) and filter by address, port and direction (*ACLs, Stateless vs Stateful Filtering and Infrastructure Protection*). On a host, tools such as `nftables` or `ufw` do the same job per machine.

## Following one packet

Your laptop (`192.168.1.20`) opens `https://example.com`:

1. **DHCP** earlier gave it its address, gateway `192.168.1.1` and DNS server `192.168.1.1`.
2. **DNS**: it asks `192.168.1.1` for `example.com`, receives an address such as `104.20.23.154`.
3. The destination is outside `192.168.1.0/24`, so the laptop sends the frame to the **gateway's MAC** (found with ARP), through the Wi-Fi access point and the internal switch.
4. The **router** receives it, the **NAT** rewrites the source to `203.0.113.45:40001`, the **firewall** records the connection, and the packet goes to the ISP, which routes it towards the server.
5. The reply returns to `203.0.113.45:40001`; NAT maps it back to `192.168.1.20:51734` and the firewall lets it through because it belongs to a known connection.

## Growing up: the same pieces, bigger

| At home | In a company |
|---|---|
| one switch inside the router box | many switches, VLANs, spanning tree (*Spanning Tree: STP, RSTP and MSTP*) |
| one access point | controller-managed access points, 802.1X login |
| one router with one default route | many routers running OSPF/BGP (*OSPFv2 Operation*, *BGP-4 Fundamentals*) |
| one NAT address | pools of addresses, IPv6, firewalls in clusters |
| one ISP link | redundant links (*First-Hop Redundancy*, *BFD*) |

## Pitfalls

- **Treating "the router" as one thing** when troubleshooting: decide whether the fault is Wi-Fi (radio), switching, addressing (DHCP), naming (DNS) or the ISP.
- **Calling NAT a firewall.** It blocks unsolicited inbound traffic by accident, not by design.
- **Port-forwarding to a device that gets a different DHCP address tomorrow**; reserve its address first.
- **Two routers handing out DHCP on the same LAN**, causing duplicate or wrong gateways.
- **Leaving default admin passwords** and outdated firmware on the router.
- **Overlapping private ranges** when joining a home network to an office VPN (both using `192.168.1.0/24`).

## Key takeaways

1. A home "router" is a switch, an access point, a router, DHCP, DNS forwarding, NAT and a firewall in one box.
2. Switches forward frames inside a network by MAC address; routers forward packets between networks by IP address.
3. DHCP hands out addresses, gateway and DNS; link-local `169.254.x.x` means DHCP failed.
4. NAT lets many private devices share one public address by rewriting addresses and ports, but it is not a firewall.
5. Larger networks are these same functions replicated and made redundant.
