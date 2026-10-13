---
title: Bridge-Reserved Addresses, Slow Protocols and LLDP
track: networking
module: Layer 2
moduleOrder: 2
order: 7
level: intermediate
summary: The reserved multicast MACs that carry link-local control protocols, how bridges treat them, and the LLDP discovery protocol in detail.
prerequisites:
  - Ethernet Frames and MAC Learning
sources:
  - label: "IEEE 802.1Q-2022 Table 8-1 — Reserved MAC addresses; Clause 8.6 (bridge relay of reserved addresses)"
  - label: "IEEE 802.1AB-2016 — Station and Media Access Control Connectivity Discovery (LLDP)"
  - label: "IEEE 802.1AB / ANSI/TIA-1057 — LLDP-MED"
  - label: "IEEE 802.3-2022 Annex 57A — Slow Protocols and OAM (Clause 57)"
  - label: "RFC 7042 — IANA Considerations and IETF Protocol and Documentation Usage for IEEE 802 Parameters"
    url: https://www.rfc-editor.org/rfc/rfc7042
---

## The `01-80-C2-00-00-0x` block

Bridges implement the *relay function* (forward frames between ports), but some protocols are meant to be **link-local**: they should reach only the immediate neighbor. IEEE reserved 16 multicast addresses `01-80-C2-00-00-00` to `…-0F` for this. A conforming 802.1D/Q bridge **never forwards** frames with these destinations (it filters them); depending on the address, the bridge may *process* them itself.

| Address | Used by |
|---|---|
| `01-80-C2-00-00-00` | Bridge Group Address: STP/RSTP/MSTP BPDUs |
| `01-80-C2-00-00-01` | MAC Control: 802.3x PAUSE |
| `01-80-C2-00-00-02` | Slow Protocols: LACP, Marker, OAM (802.3ah), ESMC |
| `01-80-C2-00-00-03` | 802.1X EAPOL (Nearest non-TPMR bridge group address) |
| `01-80-C2-00-00-08` | Provider Bridge Group Address (802.1ad) |
| `01-80-C2-00-00-0E` | LLDP — the "nearest bridge" address |
| `01-80-C2-00-00-20`–`2F` | Other 802.1 uses: MMRP, MVRP (`…-21`), GARP |

Important operational facts:

- **Many switches pass or tunnel some of these**: e.g. a "dumb" unmanaged switch or a L2 VPN service might forward `…-0E` or `…-02` between customer ports. A customer may thus see LLDP from the *far* end of a provider network. Conversely, some bridges **drop** 802.1X frames (`…-03`), breaking 802.1X through a switch ("EAPOL pass-through" must be enabled).
- Linux: the bridge driver filters the whole block by default. `group_fwd_mask` (per bridge, and `group_fwd_mask` per port on newer kernels) selects which of the 16 addresses may be forwarded; bits for `…-00` (STP), `…-01` (PAUSE) and `…-02` (LACP) can never be set because those frames are always consumed. Example: `echo 0x4008 > /sys/class/net/br0/bridge/group_fwd_mask` lets `…-03` (bit 3, 802.1X) and `…-0E` (bit 14, LLDP) pass through.
- **Provider tunneling** of customer control protocols (L2PT) rewrites these destination MACs (to a vendor address like `01-00-0C-CD-CD-D0`) at ingress and back at egress, so that the provider core treats them as ordinary multicast.

## Slow Protocols

EtherType `0x8809` followed by a **subtype** byte: 1 = LACP, 2 = Marker, 3 = OAM (802.3ah, "Ethernet in the First Mile"), 10 = OSSP (organization-specific). They are called "slow" because the standard limits them to **at most 10 frames per second per port**, so hardware can pass them to the CPU safely.

## LLDP (IEEE 802.1AB)

LLDP is the vendor-neutral replacement for proprietary discovery protocols (Cisco CDP, Foundry FDP, Extreme EDP, Nortel SONMP). It advertises "who am I and what is this port" to the neighbor, so tools can draw topology maps and verify cabling.

### Transport and timers

- EtherType **`0x88CC`**, destination one of three multicast addresses (nearest bridge `01-80-C2-00-00-0E` is the default and is never forwarded; `…-00` customer bridge and `…-03` non-TPMR have wider scope for provider setups).
- Each LLDPDU is **one frame** with a sequence of TLVs (type 7 bits, length 9 bits), ending with an End-of-LLDPDU TLV.
- `msgTxInterval` **30 s** (default); `msgTxHold` **4**, so the **TTL = 4 × 30 + 1 = 121 s** (neighbors discard stale info after TTL). When something changes, a frame is sent immediately (rate limited), and *fast start* repeats it a few times at 1 s when a new neighbor appears. Sending a TTL of 0 withdraws the information (shutdown LLDPDU).
- It is **unidirectional**: each side transmits independently, and neither acknowledges.

### TLVs

Three **mandatory** TLVs must come first, in order:

| TLV | Type | Content |
|---|---|---|
| Chassis ID | 1 | subtype + value (MAC, IP, hostname, ...) of the device |
| Port ID | 2 | subtype + value (interface name, MAC, ...) |
| Time To Live | 3 | seconds |

Optional basic TLVs (types 4–8): Port Description, System Name, System Description, **System Capabilities** (bridge, router, WLAN AP, telephone, ...), Management Address. **Organizationally Specific TLVs** (type 127, with OUI + subtype) extend the protocol:

- **802.1**: Port VLAN ID, Port & Protocol VLAN ID, VLAN Name, Link Aggregation, DCBX (ETS/PFC/application priority).
- **802.3**: MAC/PHY configuration/status (autoneg and duplex!), power via MDI (PoE), maximum frame size, EEE.
- **TIA LLDP-MED** (OUI `00-12-BB`): network policy (voice VLAN, DSCP), PoE power management, location (emergency call info). This is how an IP phone learns which VLAN is its voice VLAN.

### Operational uses and pitfalls

- **Cabling verification**: compare the advertised port ID/chassis with the intended topology.
- **Detect inconsistent MAC/PHY or VLAN settings** across a link: the 802.3 MAC/PHY TLV reveals autoneg/duplex mismatches, 802.1 TLVs reveal native VLAN mismatches.
- **Security**: LLDP advertises software versions, IPs and VLANs in clear text. Disable it on untrusted edge ports (or limit to LLDP-MED on phone ports) and filter at the customer boundary.
- Never rely on LLDP as the sole source of truth; it is an advertisement, not a validated fact.

### Linux

```bash
lldpcli show neighbors detail          # lldpd: neighbors with all TLVs
lldpcli configure system description 'edge-01'
tcpdump -eni eth0 ether proto 0x88cc -vv
```

NICs can intercept LLDP in firmware (e.g. when used for DCB/FCoE), so if a Linux host never sees a neighbor, check `ethtool --show-priv-flags` / vendor docs for an LLDP offload that consumes the frames.

## Lab: what a Linux bridge relays

Host `h1` and host `h2` connect through a Linux bridge (`br0` in namespace `sw`). `h1` sends one frame to each of seven destination MACs: the LLDP, STP, Slow Protocols, PAUSE and EAPOL reserved addresses, an ordinary multicast address, and broadcast. `h2` lists what actually arrives. The LLDP frame is a valid LLDPDU built by hand (Chassis ID, Port ID, TTL, System Name, Port Description, End), and the receiver decodes it.

{{include networking/reserved/send.py}}

{{include networking/reserved/recv.py}}

{{include networking/reserved/lab.sh}}

{{run networking/reserved :: unshare -Urnm sh lab.sh 2>&1}}

Reading the four rounds:

1. **Default**: the broadcast and the ordinary multicast (flooded, as for any group address without snooping state) arrive. The LLDP, Slow Protocols, PAUSE and EAPOL-group frames are **filtered**: the bridge neither forwards nor passes them on, which is exactly what makes them link-local. The STP frame did arrive, but only because this bridge has spanning tree switched off (`stp_state 0`): Linux relays the Bridge Group Address when STP is not running. This is a classic way for a "dumb" Linux bridge to leak BPDUs between two switches and create a loop that neither of their spanning trees can see.
2. **`group_fwd_mask 0x4000`**: bit *n* of the mask means "forward `01-80-C2-00-00-0n`". Bit 14 (0x4000) is the LLDP address, so the LLDP frame now crosses the bridge, and the decoded TLVs show what a neighbor would learn: a chassis ID (subtype 4 = MAC address), a port ID (subtype 5 = locally assigned string), TTL 120 s, system name and port description. Use this on a transparent bridge or hypervisor bridge that should be invisible to LLDP.
3. **Forwarding the protected addresses is refused**: the kernel returns `Invalid argument` when bits 0 (STP), 1 (PAUSE) or 2 (LACP) are set, because forwarding those would break the protocols they belong to; with STP switched off the bridge relays the STP address anyway, as round 1 showed. (The kernel's `BR_GROUPFWD_RESTRICTED` constant is 0x0007: those three bits cannot be set, whatever the STP state.)
4. **STP enabled**: now the crafted BPDU is consumed by the bridge's STP logic and never reaches `h2`; the BPDU that does arrive is the one the bridge itself generates on its own port. The bridge participates in spanning tree rather than relaying it.

## Key takeaways

- `01-80-C2-00-00-0x` frames are link-local; compliant bridges do not relay them, though some devices pass or tunnel them.
- Slow Protocols (`0x8809`) are rate-limited to 10 fps/port and include LACP and OAM.
- LLDP (`0x88CC`): three mandatory TLVs, 30 s interval, TTL 121 s, rich org-specific TLVs (VLANs, PoE, DCBX, LLDP-MED).
- Use LLDP to verify cabling and settings, but restrict it on untrusted ports.
