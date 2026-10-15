---
title: "Planning a Small Office Network: VLANs and an Addressing Plan"
track: networking
module: Networking Essentials
moduleOrder: 0.5
order: 7
level: beginner
summary: "A worked design exercise that joins the beginner notes together: choosing VLANs for a small office, sizing and aligning subnets with growth room, picking gateways and address ranges, isolating guests, checking the plan for overlaps with a script, and the documentation habits that make a network maintainable."
prerequisites:
  - "IP Addresses, Subnets and Ports in Plain Language"
  - "Home and Office Networks: Switch, Router, Wi-Fi, NAT and Firewall"
  - "Troubleshooting a Network Problem Step by Step"
sources:
  - label: "RFC 1918 — Address Allocation for Private Internets"
    url: https://www.rfc-editor.org/rfc/rfc1918
  - label: "RFC 4632 — Classless Inter-domain Routing (CIDR)"
    url: https://www.rfc-editor.org/rfc/rfc4632
  - label: "IEEE 802.1Q — Virtual Bridged Local Area Networks"
    url: https://standards.ieee.org/ieee/802.1Q/10323/
  - label: "Limoncelli, Hogan, Chalup — The Practice of System and Network Administration, Ch. 24 (Network Architecture)"
---

Knowing what a subnet is and being able to *design* with subnets are different skills. This note walks through a realistic small-office design from requirements to a checked plan, using only ideas from the earlier beginner notes. The method, not the particular numbers, is what to take away; the later notes (*VLANs, Trunking, Private VLANs and QinQ*, *IPv4 Addressing, Subnetting, VLSM and CIDR*, *ACLs, Stateless vs Stateful Filtering and Infrastructure Protection*) supply the depth.

## The requirements

A small office has these groups of devices:

| Group | Devices today | Notes |
|---|---|---|
| Staff | 60 laptops and desktops | the main user group |
| Servers | 12 | file server, printers' print server, internal tools |
| Guests | 40 | visitors' phones and laptops: untrusted |
| Phones | 45 | desk telephones (voice traffic needs predictable quality) |
| Printers and IoT | 14 | printers, cameras, door controllers: cannot be hardened |
| Management | 8 | switch and access point management addresses |

## Step 1: one VLAN per trust and traffic type

Put devices into **separate VLANs** when they differ in *who can be trusted* or *what the traffic needs*: staff, servers, guests, voice phones, IoT and management each get their own VLAN, and therefore their own subnet (one VLAN = one subnet = one broadcast domain). The benefits are security (a compromised printer cannot talk directly to the file server), smaller broadcast domains, and the ability to apply a different policy to each group at the router or firewall. Pick VLAN numbers once and keep them consistent: the plan below uses 10, 20, 30, 40, 50 and 99. Avoid VLAN 1 (the default everywhere) for anything important.

## Step 2: size each subnet with growth

For each group, take today's count, multiply by a **growth factor** (1.5 for groups that grow slowly, 2 for ones that tend to double), and add three addresses (network, broadcast and the gateway). Round up to the next power of two. Growth matters because renumbering a network is painful; running out of addresses in the Staff VLAN next year is a design failure. Then allocate **largest first, aligned on block boundaries** (see *IP Addresses, Subnets and Ports in Plain Language*) from one private block: here `10.20.0.0/22` (1,024 addresses).

## Step 3: let a script do the arithmetic and check it

Doing this by hand invites mistakes, so the plan is computed and *verified* (no overlaps, everything inside the block):

```python
import ipaddress, math, itertools

# Requirements: (name, VLAN id, hosts needed today, growth factor)
needs = [("Staff", 10, 60, 1.5), ("Servers", 20, 12, 2.0), ("Guests", 30, 40, 1.5), ("Phones", 40, 45, 1.5), ("Printers+IoT", 50, 14, 2.0), ("Management", 99, 8, 2.0)]
supernet = ipaddress.ip_network("10.20.0.0/22")

print(f"address space: {supernet} ({supernet.num_addresses} addresses)\n")
print(f"{'VLAN':<5} {'name':<13} {'today':>5} {'plan for':>8} {'subnet':<16} {'gateway':<11} {'host range (after .1)':<24} {'usable':>6}")
free = int(supernet.network_address)
plan = []
for name, vlan, hosts, growth in sorted(needs, key=lambda x: -x[2] * x[3]):
    want = math.ceil(hosts * growth)
    bits = math.ceil(math.log2(want + 3))                    # +2 network/broadcast, +1 gateway
    size = 2 ** bits
    free = (free + size - 1) // size * size
    net = ipaddress.ip_network((free, 32 - bits))
    assert net.subnet_of(supernet)
    gw = net[1]
    pool = f"{net[10]} - {net[-2]}" if net.num_addresses > 32 else f"{net[2]} - {net[-2]}"
    plan.append((vlan, name, hosts, want, net, gw, pool))
    free += size
for vlan, name, hosts, want, net, gw, pool in sorted(plan):
    print(f"{vlan:<5} {name:<13} {hosts:>5} {want:>8} {str(net):<16} {str(gw):<11} {pool:<24} {net.num_addresses - 2:>6}")

nets = [p[4] for p in plan]
overlap = [(a, b) for a, b in itertools.combinations(nets, 2) if a.overlaps(b)]
used = sum(n.num_addresses for n in nets)
print(f"\noverlapping subnets: {len(overlap)}; addresses allocated {used} of {supernet.num_addresses}; next free block starts at {ipaddress.ip_address(free)}")
print("summary route for the whole office:", supernet, "(one route on the upstream router)")
print("guest VLAN is isolated: firewall rule 'deny", [str(p[4]) for p in plan if p[1] == 'Guests'][0], "-> all other office subnets, allow -> internet'")
```

```text
$ python3 office_plan.py
address space: 10.20.0.0/22 (1024 addresses)

VLAN  name          today plan for subnet           gateway     host range (after .1)    usable
10    Staff            60       90 10.20.0.0/25     10.20.0.1   10.20.0.10 - 10.20.0.126    126
20    Servers          12       24 10.20.1.96/27    10.20.1.97  10.20.1.98 - 10.20.1.126     30
30    Guests           40       60 10.20.1.0/26     10.20.1.1   10.20.1.10 - 10.20.1.62      62
40    Phones           45       68 10.20.0.128/25   10.20.0.129 10.20.0.138 - 10.20.0.254    126
50    Printers+IoT     14       28 10.20.1.64/27    10.20.1.65  10.20.1.66 - 10.20.1.94      30
99    Management        8       16 10.20.1.128/27   10.20.1.129 10.20.1.130 - 10.20.1.158     30

overlapping subnets: 0; addresses allocated 416 of 1024; next free block starts at 10.20.1.160
summary route for the whole office: 10.20.0.0/22 (one route on the upstream router)
guest VLAN is isolated: firewall rule 'deny 10.20.1.0/26 -> all other office subnets, allow -> internet'
```

Reading the plan:

- **Staff** needs 90 addresses with growth, so a `/25` (126 usable). **Phones** need 68, also a `/25`. **Servers** (24) and **Printers and IoT** (28) each fit a `/27` (30 usable). **Guests** need 60, so a `/26` (62 usable). **Management** needs 16, so a `/27`.
- The two `/25`, one `/26` and three `/27` blocks pack together with **no overlap**, and the whole design uses 416 of the 1,024 addresses, leaving the block starting at `10.20.1.160` free for new VLANs. That spare room is deliberate.
- Every subnet's **gateway is its first usable address** (`.1`, `.65`, ...): the convention means anyone can guess it and a mistake is easy to spot. The router (or Layer 3 switch) has one interface or VLAN interface per subnet with that address.
- The whole office summarises to **one route**, `10.20.0.0/22`, which is all the upstream router or VPN peer needs (see the *VLSM, Summarisation, Longest-Prefix Match and Fragmentation* lab).
- Within each subnet, keep addresses **static** for servers, printers and infrastructure (outside any DHCP pool) and use **DHCP** for client VLANs such as Staff, Guests and Phones. Reserve the first few addresses for gateways and redundancy protocols such as VRRP, and use DHCP reservations for devices that must keep an address but are configured automatically.

## Step 4: decide who may talk to whom

VLANs separate devices; **firewall rules at the router** decide what crosses between them. A sensible starting policy (default deny, then allow what is needed):

| From | To | Policy |
|---|---|---|
| Staff | Servers | allow the needed services (file sharing, printing, internal web) |
| Staff | Printers/IoT | allow printing only |
| Staff | Internet | allow |
| Phones | voice servers/Internet | allow, and give voice traffic priority (QoS) |
| **Guests** | **internet only**: deny `10.20.1.0/26` to every other office subnet | allow DNS and web; block everything internal; isolate guests from each other too |
| Printers/IoT | anything except replies | deny new connections *from* IoT to staff and servers |
| Management | managed by a small group only | allow from an admin workstation or jump host |

Remember that the router also needs **NAT** to the internet (*NAT — Static, NAPT, Port Forwarding, Hairpinning and CGNAT*) and that the access points broadcast **one SSID per VLAN** (Staff, Guests; see *Wi-Fi for Beginners*), mapped to the right VLAN at the AP.

## Step 5: build the physical picture

- **Access ports** connect end devices; each carries one VLAN untagged (a staff desk port is in VLAN 10).
- **Trunk ports** carry several VLANs, tagged, between the switches, the access points and the router. Keep the list of VLANs allowed on each trunk explicit.
- A **single link** from the switch to the router carrying all the VLANs ("router on a stick") is simple and fine for small offices, but it becomes a bottleneck and a single point of failure; larger designs use a Layer 3 switch for inter-VLAN routing.
- Plan **spanning tree** (set the root bridge deliberately) and **redundant uplinks** where an outage would hurt (*Spanning Tree: STP, RSTP and MSTP*, *Link Aggregation (802.1AX / LACP) and Load Balancing*).

## Step 6: document it, because the plan is a deliverable

A network you cannot read about is a network nobody dares change. Keep, in version control or a wiki: the address plan table above; a diagram (devices, links, VLANs on each trunk); the firewall rules with the *reason* for each; the DHCP ranges and reservations; passwords and device inventory (in a password manager, not in the document); and a change log. When you test the design, use the troubleshooting order from *Troubleshooting a Network Problem Step by Step*: address, gateway, routing, name, port, application.

## Pitfalls

- **Sizing for today's count only**, then renumbering a year later.
- **One big flat network**: every device can attack every other and broadcasts reach everyone.
- **Overlapping subnets** (a typo in a prefix length); always check with a script.
- **Using `192.168.1.0/24` everywhere**: it collides with home networks the moment people use VPN from home. Prefer less common private ranges such as `10.20.0.0/22`.
- **Leaving VLAN 1 as the management network** and unused ports enabled.
- **Forgetting that VLANs are not security without firewall rules** between them, and that guest Wi-Fi without client isolation is a shared network.
- **No documentation** and no spare address space.

## Key takeaways

1. Create a VLAN (and so a subnet) per trust level and traffic type; apply policy between them at the router or firewall.
2. Size each subnet from today's need times a growth factor plus three, round up to a power of two, and allocate largest first on aligned boundaries.
3. Verify plans with a script, keep one summarisable block for the whole site, and leave unallocated space for growth.
4. Use the first usable address as the gateway, static addresses for infrastructure and DHCP for clients.
5. Isolate guests and IoT by rule, document everything, and test with a layered troubleshooting order.
