---
title: Reading RFCs and IEEE Standards Efficiently
track: networking
module: Fundamentals
moduleOrder: 1
order: 3
level: advanced
summary: How the documents are organized, the vocabulary (MUST/SHOULD), how to find the normative parts fast, and the specs worth knowing by number.
sources:
  - label: "RFC 2119 / RFC 8174 — Key words for use in RFCs to indicate requirement levels"
    url: https://www.rfc-editor.org/rfc/rfc8174
  - label: "RFC 2026 / BCP 9 — The Internet Standards Process"
    url: https://www.rfc-editor.org/rfc/rfc2026
  - label: "RFC 7322 — RFC Style Guide"
    url: https://www.rfc-editor.org/rfc/rfc7322
  - label: "RFC 7841 — RFC Streams, Headers, and Boilerplates"
    url: https://www.rfc-editor.org/rfc/rfc7841
  - label: "IEEE SA — Get IEEE 802 Program (free access to published 802 standards)"
---

Senior engineers eventually settle disputes by reading the spec. The goal of this note is to make that fast.

## IETF RFCs

### Document streams and status

An RFC can come from the IETF, IAB, IRTF or independent stream, and is marked with a **status**:

| Status | Meaning |
|---|---|
| Internet Standard (STD n) | Mature, with proven interoperability (e.g. STD 5 = IP, ICMP; STD 7 = TCP is RFC 9293) |
| Proposed Standard | The normal state of most widely deployed protocols (OSPFv3, BGP updates). *Proposed does not mean experimental.* |
| Best Current Practice (BCP) | Operational guidance (BCP 38 = ingress filtering; BCP 14 = the MUST/SHOULD keywords) |
| Informational / Experimental | Descriptive or trial protocols; Informational RFCs also document vendor protocols (HSRP RFC 2281, a few others) |
| Historic | Obsolete; don't implement |

RFC numbers never change and are never edited. Corrections arrive as new RFCs that **Update** or **Obsolete** older ones. Always check the header of the document, or `rfc-editor.org/info/rfcNNNN`, for "Obsoleted by". Example: TCP is now RFC 9293, which obsoletes RFC 793; IPv6 core is RFC 8200, which obsoletes RFC 2460; the BGP specification RFC 4271 obsoleted 1771; and RFC 9293 consolidates a dozen TCP updates.

### The requirement vocabulary

RFC 2119 / RFC 8174 give meaning to capitalized words:

- **MUST / REQUIRED / SHALL**: absolute requirement for compliance.
- **SHOULD / RECOMMENDED**: valid reasons may exist to ignore, but you must understand the implications.
- **MAY / OPTIONAL**: truly optional.
- Lowercase "must/should" has no special force.

When two vendors disagree, find the sentence containing a capitalized keyword and check whether it covers the behavior; if the text says "implementation-specific" or has no keyword, either behavior is legal.

### Anatomy and a fast reading order

1. **Abstract and Introduction**: the problem and scope.
2. **Terminology**: defines terms precisely; read it before the rest.
3. **Packet formats and field semantics**: the tables and ASCII diagrams. Bits are numbered left to right, and 32-bit words are the layout unit.
4. **State machines and procedures**: the core. Pseudocode in RFCs is normative unless stated otherwise.
5. **Security Considerations** (mandatory in every RFC since RFC 3552): known attacks.
6. **IANA Considerations**: which registries (types, codes, port numbers) it creates or uses. Often the best summary of what is extensible.
7. **Appendices**: examples, rationale. Informative.

The **Normative References** list tells you which other RFCs you must read to understand this one; the Informative list is background.

### Working with RFCs

- Search by number: `https://www.rfc-editor.org/rfc/rfc4271`. The datatracker (`datatracker.ietf.org/doc/rfc4271/`) shows relationships, errata and the working-group history.
- **Errata** are not the standard but are often right: check the errata page ("Verified" ones are accepted; "Reported" are not authoritative).
- Companion documents matter: OSPF is RFC 2328 plus RFC 3101 (NSSA), RFC 5340 (OSPFv3), RFC 6860 (hidden prefixes) and so on. A "complete" spec is often 5–10 RFCs; the working group's page lists them.
- Draft documents (`draft-ietf-...`) are works in progress and expire after 6 months; do not treat them as standards.

## IEEE 802 standards

IEEE standards are more compact but densely written, with **clauses** (numbered sections) and **annexes**, and rely on formal state-machine diagrams (SDL-like) and tables of managed objects.

- 802.3 is huge (≈ 7000 pages) and organized as **clauses per PHY** (Clause 25 = 100BASE-TX, 40 = 1000BASE-T, 49 = 10GBASE-R PCS…). Clause 4 is the MAC procedure and Clause 3 the frame.
- 802.1Q (2022) *consolidates* many amendments: 802.1D (bridging), 802.1s (MSTP), 802.1w (RSTP), 802.1ad (provider bridges), 802.1ak (MRP), 802.1Qbb (PFC), 802.1Qaz (ETS/DCBX). Old documents like 802.1D-2004 remain valid for plain STP/RSTP only.
- Amendment letters (802.3ab, 802.3ae, 802.1ad) denote *project names*; after approval, they are merged into the base standard, so the lowercase letter is a historic label but is still how engineers refer to features.
- **Access**: the *Get IEEE 802* program provides free PDFs of 802 standards six months after publication. The IEEE Registration Authority gives the OUI/EtherType registries (free).
- Normative language uses "shall/should/may" (IEEE style guide) rather than RFC capitals; the same hierarchy applies.

## Spec map: the ones to remember

| Technology | Authoritative document |
|---|---|
| Ethernet MAC/PHY | IEEE 802.3 |
| Bridging / VLAN / STP | IEEE 802.1Q (802.1D legacy) |
| LACP | IEEE 802.1AX |
| LLDP | IEEE 802.1AB |
| Port-based authentication | IEEE 802.1X (+ EAP: RFC 3748, RADIUS: RFC 2865) |
| MACsec | IEEE 802.1AE |
| ARP | RFC 826 (+ RFC 5227 conflict detection) |
| IPv4 / ICMP | RFC 791 / RFC 792 |
| IPv6 / NDP / SLAAC | RFC 8200 / RFC 4861 / RFC 4862 |
| IP addressing architecture | RFC 4291 (IPv6), RFC 4632 (CIDR), RFC 1918 (private), RFC 6890 (special-purpose) |
| DHCP / DHCPv6 | RFC 2131 / RFC 8415 |
| NAT | RFC 3022, RFC 4787 (UDP behavior), RFC 6888 (CGNAT) |
| OSPF | RFC 2328 (v2), RFC 5340 (v3) |
| IS-IS | ISO 10589, RFC 1195 |
| BGP | RFC 4271 (+ 4456 RR, 1997 communities, 7606 error handling) |
| BFD | RFC 5880 |
| VRRP | RFC 5798 |
| IPsec / IKEv2 | RFC 4301 / RFC 7296 |
| MPLS | RFC 3031, 3032 |
| VXLAN / EVPN | RFC 7348 / RFC 7432, RFC 8365 |
| IGMP / MLD / PIM | RFC 3376 / RFC 3810 / RFC 7761 |
| DiffServ | RFC 2474, RFC 2475 |

## How I use these in the notes

Every note ends with its sources; the constants (field widths, timers, defaults) are quoted from those documents. Where behavior is *vendor-specific* (VTP, PVST+, HSRP internals, vPC) the note says so, because the RFC will not tell you.

## Key takeaways

- RFC numbers are immutable: check "Obsoleted by/Updates" before trusting any RFC.
- Capitalized MUST/SHOULD/MAY define conformance; "Proposed Standard" is usually production-grade.
- Read order: terminology → formats → procedures → security → IANA.
- For Ethernet-and-below use IEEE 802 (802.3 per clause, 802.1Q as the consolidated bridging document); above that use IETF.
- Errata and companion RFCs are part of the real specification.
