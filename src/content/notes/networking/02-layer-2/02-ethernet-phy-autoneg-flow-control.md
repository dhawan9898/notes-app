---
title: Ethernet PHY, Autonegotiation, Duplex, Flow Control and MTU
track: networking
module: Layer 2
moduleOrder: 2
order: 2
level: intermediate
summary: What happens below the MAC — link speeds and media, autonegotiation, duplex mismatches, PAUSE and PFC, jumbo frames and the error counters that reveal physical problems.
sources:
  - label: "IEEE 802.3-2022, Clause 28 (autonegotiation for twisted pair), Clause 37 (1000BASE-X), Clause 40 (1000BASE-T), Annex 31B (MAC Control PAUSE)"
  - label: "IEEE 802.1Q-2022, Clause 36 (Priority-based Flow Control, formerly 802.1Qbb)"
  - label: "IEEE 802.3-2022, Clause 74/91/108 (Forward Error Correction)"
  - label: "Linux kernel docs — ethtool(8), Documentation/networking/ethtool-netlink.rst"
    url: https://docs.kernel.org/networking/ethtool-netlink.html
---

## The PHY and the MAC

Ethernet splits into the **MAC** (framing, addressing, access control; the previous note) and the **PHY** (encoding bits onto a medium). The boundary is a family of interfaces: MII, GMII, RGMII, XGMII… on the chip side. The PHY itself has sublayers: the **PCS** (line coding, e.g. 8b/10b or 64b/66b), **PMA** (serialization), and **PMD** (the optical or electrical driver), plus optional **FEC**.

Each speed/medium combination is a separate clause of 802.3 and is named `<speed><modulation><medium>`: `1000BASE-T` (1 Gb/s, baseband, twisted pair), `10GBASE-SR` (short-reach multimode fiber), `25GBASE-CR` (copper twinax cable).

| Name | Medium | Reach (typ.) | Notes |
|---|---|---|---|
| 10BASE-T | Cat3+ UTP, 2 pairs | 100 m | Manchester coding |
| 100BASE-TX | Cat5 UTP, 2 pairs | 100 m | 4B5B + MLT-3 |
| 1000BASE-T | Cat5e UTP, **4 pairs, simultaneous both directions** | 100 m | PAM-5; echo cancellation |
| 1000BASE-SX / LX | MMF / SMF | 550 m / 5 km | 8b/10b |
| 10GBASE-T | Cat6A | 100 m | PAM-16, high latency and power |
| 10GBASE-SR / LR | MMF / SMF | 300–400 m / 10 km | 64b/66b |
| 25/40/100GBASE-* | SFP28 / QSFP+ / QSFP28 | DAC 3–5 m, SR 70–100 m, LR 10 km | 40G/100G use multiple parallel or wavelength "lanes" |

At speeds ≥ 25 Gb/s the signal is so marginal that **Reed–Solomon FEC** (RS(528,514), "KR4/KP4") is required on many media. FEC corrects errors at the cost of some latency (on the order of 100 ns) and means the two ends **must agree** on FEC mode, a common cause of "link up but flapping" with third-party optics.

## Autonegotiation

Autonegotiation (Clause 28) lets two ends of a **twisted-pair** link discover their common capabilities and pick the best: speed, duplex, flow-control support, and (for 1000BASE-T) master/slave clock roles. It is implemented with **Fast Link Pulses (FLP)**, bursts of link pulses encoding a 16-bit *Link Code Word* of capability bits, exchanged in the first moments of link bring-up.

Resolution follows a fixed **priority**: 10GBASE-T > 1000BASE-T full > 1000BASE-T half > 100BASE-T2 > 100BASE-TX full > … > 10BASE-T half.

Rules that surprise people:

- **1000BASE-T and 10GBASE-T require autonegotiation**. You cannot "hard-code" 1 Gb/s on copper: you can only *advertise* only that speed. The standard needs the exchange to choose master/slave.
- If one side disables autonegotiation and fixes 100/full, the other side sees no FLPs and falls back to **parallel detection**: it can detect the *speed* from the signal but not the duplex, so it assumes **half duplex**. → **Duplex mismatch.**
- Fiber links have no FLP-based negotiation (1000BASE-X has its own, simpler Clause 37 negotiation; optical 10G and above generally has none, while backplane and twinax PHYs use Clause 73), so speed is mostly a function of the transceiver.
- Auto-MDIX (not part of autoneg itself but standardized with 1000BASE-T) removes the need for crossover cables.

### Duplex mismatch: symptoms

One end full duplex (transmits whenever it wants), the other half duplex (applies CSMA/CD and backs off on perceived collisions). Light traffic works; under load:

- The half-duplex side counts **collisions and late collisions**; the full side counts **FCS/CRC errors, runts, and alignment errors**.
- TCP throughput collapses (retransmissions) while ping looks fine.

```bash
ethtool eth0                  # Speed, Duplex, Auto-negotiation, Link detected, advertised/partner modes
ethtool -S eth0 | egrep -i 'crc|align|coll|runt|fcs|drop'
ethtool -s eth0 autoneg on advertise 0x020   # advertise only 1000baseT/Full (value is a bitmask)
```

Modern guidance: leave autonegotiation **on** at both ends; if one end *must* be fixed, fix both ends and verify with `ethtool`.

## Flow control

Two different mechanisms called "flow control", both link-local:

### 802.3x PAUSE

The receiver sends a **MAC Control frame** (EtherType `0x8808`, opcode `0x0001`, destination `01-80-C2-00-00-01`) with a *pause_time* in units of 512 bit times. The sender stops *all* transmission for that time. PAUSE is **per-link** and blunt: it stops every flow, including unrelated ones (**head-of-line blocking**), and can spread congestion upstream ("congestion spreading"). It is usually disabled on switch uplinks and NICs unless a lossless need exists.

### 802.1Qbb Priority-based Flow Control (PFC)

PFC pauses **one of eight priority classes** (using a PCP-based priority-vector in the MAC control frame, opcode `0x0101`) instead of the whole link. It creates a **lossless class** for traffic that cannot tolerate drops: RoCEv2 RDMA, Fibre Channel over Ethernet. It is part of Data Center Bridging (DCB), along with ETS (802.1Qaz bandwidth sharing) and DCBX. PFC needs careful buffer headroom tuning and can lead to **PFC deadlocks** and *pause storms*; many operators add a watchdog.

Neither mechanism is congestion control: that is the job of TCP, or ECN (RFC 3168) / DCQCN for RDMA.

## MTU, jumbo frames and giants

The standard Ethernet payload is 1500 bytes; "jumbo frames" (commonly 9000, up to ~9216 payload) are **not in the 802.3 standard** but are widely implemented. An MTU only works if **every device in the L2 domain agrees**:

- A switch with a smaller frame-size limit **drops** the larger frames silently, counting them as *giants* or *oversized*. A router with a smaller interface MTU fragments or sends ICMP "fragmentation needed".
- All hosts in a subnet must use the same MTU: IP has no way to learn a peer's MTU on the same link (IPv6 can advertise one in Router Advertisements).
- The MTU configured on a Linux interface excludes the Ethernet header and FCS; the *switch's* frame-size limit includes them (VLAN tag as well), so "MTU 9000" requires a switch frame-size limit of at least 9018 (9022 with a VLAN tag).

```bash
ip link set dev eth0 mtu 9000
ping -M do -s 8972 10.0.0.2   # -M do sets DF; 8972 + 8 ICMP + 20 IP = 9000
```

If the ping with DF fails but a smaller one works, the path MTU is smaller than you think. Tunnel overheads (VXLAN 50 B, GRE 24 B, IPsec ~50–70 B) follow the same logic.

## Physical-layer diagnostics

| Counter / symptom | Likely cause |
|---|---|
| CRC / FCS errors increasing | bad cable, dirty fiber, EMI, duplex mismatch, failing optic |
| Runts (< 64 B) | collisions on half duplex, duplex mismatch |
| Giants / oversized | MTU mismatch or jumbo on one side |
| Late collisions | duplex mismatch or over-long cable (half duplex only) |
| Input drops with no errors | congestion / queue exhaustion |
| Link flaps | bad cable/optic, autoneg trouble, FEC mismatch, power-saving (EEE) interaction |
| Optical power (DOM) low/high | `ethtool -m eth0` reads the transceiver's DOM: Tx/Rx power, temperature |

```bash
ethtool -m eth0               # SFP/QSFP module EEPROM and digital diagnostics
ethtool --show-fec eth0       # FEC mode (None/BaseR/RS/Auto)
```

## Key takeaways

- The MAC is common to all Ethernet; the PHY changes per speed and medium (an 802.3 clause per PHY).
- Autonegotiation is mandatory at 1G/10G copper; disable it on one side only and you get a duplex mismatch.
- Duplex mismatch produces CRC errors on the full-duplex side and collisions on the half-duplex side.
- PAUSE stops the whole link; PFC stops one priority class and is the basis for lossless Ethernet (RoCE).
- MTU must be consistent across the L2 domain; compute the extra bytes (tag, tunnel) and test with DF pings.
