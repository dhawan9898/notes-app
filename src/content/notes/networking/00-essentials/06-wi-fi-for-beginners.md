---
title: "Wi-Fi for Beginners: Bands, Channels, Signal and Security"
track: networking
module: Networking Essentials
moduleOrder: 0.5
order: 6
level: beginner
summary: "How wireless networking differs from cable: access points, SSID and BSSID, the 2.4, 5 and 6 GHz bands, channels and interference, reading signal strength in dBm (with the arithmetic checked), why distance and walls matter, WPA2 and WPA3, roaming, and a practical troubleshooting order."
prerequisites:
  - What Is a Network?
  - "Home and Office Networks: Switch, Router, Wi-Fi, NAT and Firewall"
sources:
  - label: "IEEE 802.11 — Wireless LAN Medium Access Control and Physical Layer Specifications"
    url: https://standards.ieee.org/standard/802_11-2020.html
  - label: "Wi-Fi Alliance — Wi-Fi CERTIFIED WPA3 specification overview"
    url: https://www.wi-fi.org/discover-wi-fi/security
  - label: "Gast — 802.11 Wireless Networks: The Definitive Guide, 2nd ed., Ch. 1-4"
  - label: "ITU-R P.525 — Calculation of free-space attenuation"
    url: https://www.itu.int/rec/R-REC-P.525/en
---

Wi-Fi is Ethernet without the cable, but removing the cable changes almost everything about how reliable the network is. Radio waves fade, bounce, pass through some walls and not others, and are shared with every neighbour. This note explains what happens between your phone and the access point so you can fix a weak or flaky wireless network systematically. The detailed standard is in *Wi-Fi (IEEE 802.11): Medium Access, Frames, Security and Roaming*.

## The pieces and the names

- An **access point (AP)** connects wireless devices (**stations**) to the wired network. In a home it is built into the router box; in a company, many APs are managed together.
- The **SSID** is the network *name* you see ("HomeNet"). Several APs can share one SSID so you can move between them.
- The **BSSID** is the MAC address of one particular AP radio, the unique identity behind the name.
- Every Wi-Fi frame is a MAC-layer frame like Ethernet, but it carries up to **four addresses** (transmitter, receiver, source, destination) so the AP can relay between stations; the AP **bridges** wireless and wired traffic into one Layer 2 network, which is why a laptop on Wi-Fi gets an address from the same DHCP server as a cabled PC.

## Bands and channels

Wi-Fi uses unlicensed radio frequencies, called **bands**:

| Band | Typical channels | Range | Speed | Crowding |
|---|---|---|---|---|
| **2.4 GHz** | 1 to 13 (1, 6 and 11 do not overlap) | longest, passes walls best | lowest | very crowded (Bluetooth, microwaves, neighbours) |
| **5 GHz** | 36, 40, 44, 48, ... many | medium | high | much less crowded |
| **6 GHz** (Wi-Fi 6E and 7) | many wide channels | shortest | highest | newest, emptiest |

A **channel** is a slice of the band; each channel is centred on a frequency (2.4 GHz channel *n* is at 2407 + 5n MHz, so channels 1, 6 and 11 sit at 2412, 2437 and 2462 MHz; 5 GHz channel *n* is at 5000 + 5n MHz, so channel 36 is 5180 MHz). Channels in 2.4 GHz are 20 MHz wide but only 5 MHz apart, so **only 1, 6 and 11 are free of overlap**; picking channel 3 between two neighbours on 1 and 6 interferes with both. Two APs on the *same* channel do not collide destructively, they **share the air time** (they take turns, because Wi-Fi listens before it transmits: CSMA/CA), so a crowded channel feels slow even when the signal is strong. Wider channels (40, 80, 160 MHz) give more speed but fit fewer non-overlapping choices, which suits 5 and 6 GHz but is a poor idea in 2.4 GHz.

Marketing names map onto IEEE generations: Wi-Fi 4 (802.11n), Wi-Fi 5 (802.11ac, 5 GHz only), Wi-Fi 6 and 6E (802.11ax; 6E adds the 6 GHz band) and Wi-Fi 7 (802.11be). The headline speeds are best case: real throughput is typically half or less, shared among everyone on that AP.

## Signal strength: dBm in plain words

Signal strength is reported in **dBm**, decibels relative to one milliwatt. The scale is logarithmic and negative numbers are normal: *closer to 0 is stronger*. Each **+3 dB doubles** the power and each +10 dB multiplies it by ten:

| Reading | Power received | Meaning in practice |
|---|---|---|
| −30 dBm | 0.001 mW | next to the AP |
| −50 dBm | 0.00001 mW | excellent |
| −60 dBm | 0.000001 mW | very good |
| −67 dBm | 0.0000002 mW | the usual target for video calls and voice |
| −70 dBm | 0.0000001 mW | acceptable, speeds start to drop |
| −80 dBm | 0.00000001 mW | poor: slow and unreliable |
| −90 dBm | 0.000000001 mW | basically unusable |

(The powers are computed as 10^(dBm/10) mW.) A signal at −50 dBm is therefore 100 times stronger than one at −70 dBm, yet both show a nearly full bar on a phone, which is why bars mislead.

Equally important is **noise and interference**: what counts is the **SNR**, the gap between signal and background noise (a −60 dBm signal over a −95 dBm noise floor has an SNR of 35 dB, which is good; the same signal on a noisy channel with −70 dBm of interference is much worse).

**Distance and obstacles.** In open air the loss grows with the logarithm of distance and with frequency: the free-space path loss at 10 m is about 60 dB at 2.4 GHz, 67 dB at 5 GHz and 68 dB at 6 GHz. Walls add more: roughly 3 to 6 dB for a plasterboard wall, 10 to 15 dB for brick or concrete, and much more for metal, water (an aquarium, a person) and lifts. Rule of thumb: **5 and 6 GHz are faster but weaker through walls, 2.4 GHz travels further but is slow and crowded**; use 5 GHz close to the AP and keep 2.4 GHz for far corners and simple devices.

## Security: WPA2 and WPA3

Because radio reaches beyond your walls, Wi-Fi encrypts traffic:

- **Open** networks (no password) are unencrypted: anyone nearby can read what is not protected by TLS.
- **WPA2-Personal** (a shared passphrase, AES-CCMP) is acceptable if the passphrase is long and unique; the weakness is that an attacker who captures the handshake can try guesses offline.
- **WPA3-Personal** replaces the handshake with SAE, which makes offline guessing impractical and gives each client its own keys (forward secrecy).
- **WPA2/WPA3-Enterprise** (802.1X with a RADIUS server) gives every user their own credentials, so revoking one person does not mean changing a shared password (see *Layer 2 Security: Port Security, DHCP Snooping, DAI, 802.1X and MACsec*).
- Avoid **WEP** and **WPA/TKIP**, and turn off **WPS** (the push-button/PIN setup), which has known weaknesses. Hiding the SSID is not security.
- Use a separate **guest network** with client isolation so visitors cannot reach your printers and computers (a VLAN again).

## Roaming

When you walk between two APs of the same network, your device decides when to move to the stronger AP; the AP does not push it. Devices often stay attached to a distant AP ("sticky clients") until the signal is very poor. Fast-roaming features (802.11k/v/r) help: neighbour reports, steering suggestions and quick key exchange. For good roaming, place APs so their coverage overlaps at about −67 to −70 dBm, give them the same SSID and security settings, and put them on different non-overlapping channels.

## Troubleshooting order

1. **One device or all?** If only one device is slow, suspect that device (drivers, power saving, a distant corner).
2. **Signal.** Check the dBm value your device reports (on Linux, `iw dev wlan0 link` shows `signal: -62 dBm` and the connected BSSID; `nmcli dev wifi` lists nearby networks with channel and signal). Anything worse than −70 dBm means move closer or add an AP.
3. **Channel and band.** If many networks share your channel, change to a clearer one or move devices to 5 GHz.
4. **Interference.** Microwave ovens, cordless phones, baby monitors and Bluetooth hurt 2.4 GHz. Test with the suspect source off.
5. **The wired side.** The AP's own uplink may be the problem: check the cable, the link speed (a 100 Mb/s link throttles a fast AP) and PoE power.
6. **Upper layers.** If Wi-Fi is connected but "no internet", the fault is probably DHCP, DNS or the ISP, not radio: use *Troubleshooting a Network Problem Step by Step*.

## Pitfalls

- **Trusting signal bars**; use dBm and speed tests in the places you actually work.
- **Putting every AP on the same channel** or letting two on overlapping 2.4 GHz channels.
- **Using wide channels in 2.4 GHz**, which creates interference for everyone.
- **Placing the AP in a cupboard, on the floor or next to metal and water.**
- **Using WEP, WPS or a short passphrase.**
- **Judging speed from the "link rate"** shown by the device, which is the radio's best case, not the real throughput.

## Key takeaways

1. An access point bridges wireless devices onto the wired Layer 2 network; the SSID is the name and the BSSID identifies one radio.
2. 2.4 GHz reaches far but is slow and crowded (only channels 1, 6, 11 do not overlap); 5 and 6 GHz are faster but weaker through walls.
3. dBm is logarithmic: closer to zero is stronger, +3 dB doubles power, and about −67 dBm is the target for good service.
4. Use WPA3 (or WPA2 with a long passphrase), disable WEP and WPS, and isolate guests.
5. Troubleshoot from one device versus many, to signal, channel, interference, the wired uplink and only then upper layers.
