---
title: VLANs, Trunking, Private VLANs and QinQ
track: networking
module: Layer 2
moduleOrder: 2
order: 3
level: intermediate
summary: Design and operate VLANs — access and trunk ports, native VLAN, inter-VLAN routing, PVLANs, provider bridging with QinQ, and VLAN-based attacks.
sources:
  - label: "IEEE 802.1Q-2022 — Bridges and Bridged Networks (VLAN-aware bridges, ingress/egress rules, Clause 5, 8, 9)"
  - label: "IEEE 802.1ad (now in 802.1Q) — Provider Bridges, S-VLAN"
  - label: "RFC 5517 — Cisco Systems' Private VLANs (Informational)"
    url: https://www.rfc-editor.org/rfc/rfc5517
  - label: "Linux kernel docs — bridge VLAN filtering, ip-link(8), bridge(8)"
    url: https://docs.kernel.org/networking/bridge.html
---

A **VLAN** is a logical broadcast domain carved out of a physical switch fabric. It gives you broadcast containment, security segmentation, and the ability to place hosts in the same subnet without being on the same switch. The tag format was covered in *Ethernet Frames*; this note covers the **behavior**.

## The VLAN-aware bridge model (802.1Q)

Every port of a VLAN-aware bridge has:

- a **PVID** (port VLAN ID): the VLAN assigned to *untagged* frames arriving on the port;
- a **VLAN membership list** (the member set), saying which VLANs the port belongs to;
- for each member VLAN, whether frames leave **tagged** or **untagged**.

Rules, per 802.1Q:

1. **Ingress**: an untagged (or priority-tagged) frame is classified to the port's PVID; a tagged frame keeps its VID. If the port is not a member of that VLAN, the frame is dropped.
2. **Forwarding**: the bridge looks up *(VID, destination MAC)* in the FDB, which is learned *per VLAN* ("Independent VLAN Learning", the standard default) and floods only within the VLAN's member ports.
3. **Egress**: for each outgoing port, the frame is sent tagged or untagged according to that port's egress rule for that VID.

Vendor terms map onto this model:

| Vendor term | 802.1Q meaning |
|---|---|
| Access port | PVID = v; member of v only; egress untagged |
| Trunk port | member of many VLANs, egress tagged; one *native* VLAN (the PVID) egress untagged |
| Hybrid port | arbitrary mix of tagged and untagged VLANs |
| Allowed VLAN list | the member set |
| Native VLAN | the port's PVID that is sent/accepted untagged |

> VLAN IDs: `0` = priority-tagged only, `1` = default VLAN (conventionally the default PVID), `1–4094` usable, `4095` reserved. Extended ranges and "VTP" (a Cisco proprietary VLAN database sync protocol) are vendor details. VTP is dangerous: a switch with a higher revision number can overwrite the whole VLAN database. Use VTP transparent/off.

## Trunks and the native VLAN

Both ends of a trunk must agree on the **tag set and the native VLAN**. Mismatches have two effects:

- Untagged traffic sent by side A in its native VLAN 10 arrives on side B's native VLAN 1, *leaking between VLANs*. Cisco's CDP/PVST+ will log a native-VLAN mismatch; standards-based bridges will not.
- Allowing a VLAN on one side but not the other black-holes it.

Best practice: use an unused VLAN as native (or tag everything on trunks — "native VLAN tagging"), prune the allowed list to what is needed, and never run user traffic on VLAN 1.

## Inter-VLAN routing

Hosts in different VLANs are in different subnets, and traffic between them must be routed:

| Method | How | Notes |
|---|---|---|
| **Router-on-a-stick** | One router interface trunked to the switch, with a sub-interface per VLAN (`eth0.10`, `eth0.20`) | Simple; all inter-VLAN traffic crosses one link twice |
| **SVI** (switch virtual interface) on an L3 switch | A routed interface for each VLAN on the switch itself | Hardware-routed at line rate; standard in campus and data-center designs |
| **External router/firewall per VLAN** | Dedicated links | When policy enforcement is needed on all inter-VLAN traffic |

## Linux examples

Two different mechanisms: classic **VLAN sub-interfaces** (host or router) and **VLAN filtering on a bridge** (a switch).

```bash
# host or router: tagged sub-interface
ip link add link eth0 name eth0.100 type vlan id 100
ip addr add 10.100.0.1/24 dev eth0.100
ip link set eth0.100 up

# bridge acting as a VLAN-aware switch
ip link add br0 type bridge vlan_filtering 1
ip link set eth1 master br0 ; ip link set eth2 master br0
bridge vlan add dev eth1 vid 100 pvid untagged   # access port in VLAN 100
bridge vlan add dev eth2 vid 100 vid 200         # trunk: tagged VLANs 100 and 200
bridge vlan add dev br0  vid 100 self            # let the bridge itself see VLAN 100 (for an SVI)
bridge vlan show
```

Without `vlan_filtering 1` a Linux bridge ignores tags (it forwards the tagged frames, treating the tag as payload), so it works as a single flat domain.

## Private VLANs (PVLAN)

A **private VLAN** subdivides one subnet's L2 domain so that hosts can't talk to each other directly while sharing an IP subnet and gateway. It uses a **primary** VLAN and one or more **secondary** VLANs:

| Port type | Can talk to |
|---|---|
| **Promiscuous** (gateway, firewall) | all ports |
| **Isolated** (secondary) | promiscuous ports only |
| **Community** (secondary) | promiscuous ports and other ports of the *same* community |

Typical use: hosting provider or hotel Wi-Fi where tenants share a /24 but must not see each other, and to stop lateral movement. Note that a PVLAN still allows hosts to talk to each other *via the router* unless it filters same-subnet forwarding (Proxy ARP / local-proxy-arp issues). PVLAN is vendor-implemented (RFC 5517 is informational).

## QinQ and provider bridging (802.1ad)

A service provider wants to carry **customers' VLAN-tagged traffic** without knowing or colliding with their VLAN IDs. 802.1ad stacks a second tag:

```text
| Dst | Src | S-TAG (0x88A8, S-VID) | C-TAG (0x8100, C-VID) | EtherType | Payload |
```

- The provider assigns each customer an **S-VLAN**; the customer's own **C-VLAN** tags ride unchanged inside.
- The customer-facing port is a **Customer Network Port**; the transit ports are **Provider Network Ports**.
- Many implementations use TPID `0x8100` for both tags ("QinQ" informally, 802.1ad strictly uses `0x88A8` for the S-TAG), so check what your counterpart expects.
- Also commonly supported: **VLAN translation** (map a customer VID to another at the ingress) and **802.1ah PBB (MAC-in-MAC)** for scaling the number of MAC addresses in a provider core.
- Customer L2 control protocols (STP, LLDP, CDP) can be *tunneled* by rewriting their multicast destination address, so that the provider does not process them.

```bash
ip link add link eth0 name eth0.100 type vlan proto 802.1ad id 100   # S-tag
ip link add link eth0.100 name eth0.100.10 type vlan proto 802.1q id 10  # C-tag inside
```

Limits: 12-bit VID gives 4094 VLANs; with stacked tags 4094 × 4094, but also an MTU increase of 4 bytes per tag.

## VLAN security issues

- **Switch spoofing**: an attacker's host negotiates a trunk using DTP (Cisco) to gain all VLANs. Fix: set access ports to `switchport mode access`, disable DTP.
- **Double tagging**: the attacker, on the *native* VLAN, sends a frame with two tags. The first switch strips the outer tag (as native) and forwards the frame with the inner tag onto the trunk, where it reaches another VLAN. It is one-way (no return), and is prevented by not using the native VLAN for users, or by tagging native frames.
- **STP and VLAN 1** exposure: keep management in a separate VLAN with ACLs.
- VLANs are **segmentation**, not strong security; a misconfigured trunk defeats them. Use ACLs and firewalls between VLANs.

## Key takeaways

- A VLAN-aware bridge classifies ingress by tag or PVID, learns and floods per VLAN, and tags or untags at egress.
- Trunks require identical native VLAN and allowed-list configuration on both ends.
- Inter-VLAN traffic must be routed (SVI, router-on-a-stick, or external firewall).
- PVLANs isolate hosts inside a subnet; QinQ (802.1ad) lets a provider carry customer VLANs transparently.
- Disable dynamic trunking, avoid VLAN 1 and the native VLAN for user traffic, and prune trunks.
