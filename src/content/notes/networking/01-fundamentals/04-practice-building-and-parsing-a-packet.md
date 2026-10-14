---
title: "Practice: Building and Parsing a Packet by Hand"
track: networking
module: Fundamentals
moduleOrder: 1
order: 4
level: intermediate
summary: "A hands-on encapsulation exercise: build an Ethernet frame carrying IPv4 and UDP in Python with correct lengths and checksums, parse it back, capture the real frame the kernel sends for the same datagram, and compare the two byte by byte, with the full program and its output."
prerequisites:
  - "Network Models, Encapsulation and Device Roles"
  - "A Packet's Journey: Host to Gateway to Host"
  - "Reading RFCs and IEEE Standards Efficiently"
sources:
  - label: "RFC 791 — Internet Protocol (header format)"
    url: https://www.rfc-editor.org/rfc/rfc791
  - label: "RFC 768 — User Datagram Protocol (header and pseudo-header)"
    url: https://www.rfc-editor.org/rfc/rfc768
  - label: "RFC 1071 — Computing the Internet Checksum"
    url: https://www.rfc-editor.org/rfc/rfc1071
  - label: "IEEE 802.3 — Ethernet frame format (overview)"
    url: https://standards.ieee.org/ieee/802.3/7071/
---

"Encapsulation" is easy to say and easy to forget. Building a frame from the RFC diagrams, byte by byte, and then comparing it with what the kernel actually sends is the most reliable way to make the layers concrete. The program below needs only Python. Part 1 builds the frame and parses it; part 2 captures the kernel's real frame for the same datagram on the loopback interface (it opens a raw `AF_PACKET` socket inside a throwaway network namespace created with `unshare -Urn`, so no root is needed).

```python
import socket, struct

def checksum(data: bytes) -> int:                              # ones' complement sum of 16-bit words (RFC 1071)
    if len(data) % 2: data += b"\0"
    s = sum(struct.unpack("!%dH" % (len(data) // 2), data))
    while s >> 16: s = (s & 0xFFFF) + (s >> 16)
    return ~s & 0xFFFF

def build_udp_frame(src_mac, dst_mac, src_ip, dst_ip, sport, dport, payload, ident=0x1234):
    udp_len = 8 + len(payload)
    pseudo = socket.inet_aton(src_ip) + socket.inet_aton(dst_ip) + struct.pack("!BBH", 0, 17, udp_len)
    udp0 = struct.pack("!HHHH", sport, dport, udp_len, 0) + payload
    udp = struct.pack("!HHHH", sport, dport, udp_len, checksum(pseudo + udp0) or 0xFFFF) + payload
    ip0 = struct.pack("!BBHHHBBH4s4s", 0x45, 0, 20 + udp_len, ident, 0x4000, 64, 17, 0, socket.inet_aton(src_ip), socket.inet_aton(dst_ip))
    ip = ip0[:10] + struct.pack("!H", checksum(ip0)) + ip0[12:]
    eth = bytes.fromhex(dst_mac.replace(":", "")) + bytes.fromhex(src_mac.replace(":", "")) + struct.pack("!H", 0x0800)
    return eth + ip + udp

def parse(frame: bytes):
    dst, src, etype = frame[0:6].hex(":"), frame[6:12].hex(":"), struct.unpack("!H", frame[12:14])[0]
    ihl = (frame[14] & 0x0F) * 4
    ver, tos, total, ident, flags, ttl, proto, csum = struct.unpack("!BBHHHBBH", frame[14:14 + 12])[0:1] + struct.unpack("!BBHHHBBH", frame[14:14 + 12])[1:]
    sip, dip = socket.inet_ntoa(frame[26:30]), socket.inet_ntoa(frame[30:34])
    sport, dport, ulen, ucsum = struct.unpack("!HHHH", frame[14 + ihl:14 + ihl + 8])
    return dict(eth_dst=dst, eth_src=src, ethertype=hex(etype), ip_total=total, ttl=ttl, proto=proto, ip_checksum_ok=checksum(frame[14:14 + ihl]) == 0,
                src=f"{sip}:{sport}", dst=f"{dip}:{dport}", udp_len=ulen, payload=frame[14 + ihl + 8:14 + ihl + ulen])

mine = build_udp_frame("00:00:00:00:00:00", "00:00:00:00:00:00", "127.0.0.1", "127.0.0.1", 40000, 9999, b"hello")
print("1. a frame built by hand:", len(mine), "bytes =", 14, "Ethernet +", 20, "IPv4 +", 8, "UDP +", len(b"hello"), "payload")
print("   hex:", mine.hex(" ", 1)[:60] + " ...")
for k, v in parse(mine).items(): print(f"   {k:<14} {v}")

# 2. send a real datagram over loopback, capture it with an AF_PACKET socket, and compare with the hand-built frame
cap = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(0x0003)); cap.bind(("lo", 0)); cap.settimeout(2)
tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); tx.bind(("127.0.0.1", 40000)); tx.sendto(b"hello", ("127.0.0.1", 9999))
while True:
    real = cap.recv(2048)
    if len(real) == len(mine) and real[23] == 17: break
same = [i for i in range(len(mine)) if mine[i] != real[i]]
print("\n2. captured the kernel's own frame:", len(real), "bytes; bytes that differ from mine:", len(same), "at offsets", same)
print(f"   IP identification: mine {mine[18:20].hex()}, kernel {real[18:20].hex()}; IP checksum: mine {mine[24:26].hex()}, kernel {real[24:26].hex()} (differs only because the identification differs)")
print(f"   UDP checksum: mine {mine[40:42].hex()}, kernel {real[40:42].hex()} (the kernel left only the pseudo-header sum, fe20, for a network card to complete: checksum offload)")
print("   kernel frame parses to the same fields:", {k: v for k, v in parse(real).items() if k in ("src", "dst", "ttl", "ip_checksum_ok", "payload")})
```

```text
$ unshare -Urn bash -c 'ip link set lo up; python3 packet.py'
1. a frame built by hand: 47 bytes = 14 Ethernet + 20 IPv4 + 8 UDP + 5 payload
   hex: 00 00 00 00 00 00 00 00 00 00 00 00 08 00 45 00 00 21 12 34  ...
   eth_dst        00:00:00:00:00:00
   eth_src        00:00:00:00:00:00
   ethertype      0x800
   ip_total       33
   ttl            64
   proto          17
   ip_checksum_ok True
   src            127.0.0.1:40000
   dst            127.0.0.1:9999
   udp_len        13
   payload        b'hello'

2. captured the kernel's own frame: 47 bytes; bytes that differ from mine: 6 at offsets [18, 19, 24, 25, 40, 41]
   IP identification: mine 1234, kernel bef1; IP checksum: mine 2a96, kernel 7dd8 (differs only because the identification differs)
   UDP checksum: mine faaf, kernel fe20 (the kernel left only the pseudo-header sum, fe20, for a network card to complete: checksum offload)
   kernel frame parses to the same fields: {'ttl': 64, 'ip_checksum_ok': True, 'src': '127.0.0.1:40000', 'dst': '127.0.0.1:9999', 'payload': b'hello'}
```

## Exercises

**1. Count the layers.** The frame is 47 bytes. Break it into headers and payload, and say which layer each belongs to and what addressing each uses.

**2. Read the fields.** In the hand-built hex, locate the EtherType, the IP version/length byte, the TTL, the protocol number and the two ports. What do the values `0800`, `45`, `40`, `11` mean?

**3. The checksum.** How is the Internet checksum computed, and how does a receiver verify it? Why does the UDP checksum include a *pseudo-header* with the IP addresses?

**4. Compare with the kernel.** Six bytes differed between the hand-built frame and the captured one. Which fields, and why? What does that tell you about which parts of a packet are fixed by the standard and which are a sender's choice?

## Answers

Try before opening.

<details>
<summary>Show the explanations</summary>

**1.** 47 = **14** (Ethernet header) + **20** (IPv4 header, no options) + **8** (UDP header) + **5** (payload `hello`).

| Bytes | Layer | Contains | Addressing |
|---|---|---|---|
| 0-13 | Layer 2, Ethernet | destination MAC (6), source MAC (6), EtherType (2) | MAC addresses: the next hop on this link |
| 14-33 | Layer 3, IPv4 | version/header length, TOS, total length, identification, flags/fragment, TTL, protocol, checksum, source and destination address | IP addresses: end to end |
| 34-41 | Layer 4, UDP | source port, destination port, length, checksum | ports: which program |
| 42-46 | application | `hello` | none |

(An Ethernet frame also has a 4-byte frame check sequence at the end that the network card adds and strips; software never sees it, so it is not in the capture.) Each layer treats everything inside as opaque payload: Ethernet does not look at IP, and IP does not look at UDP; that separation is the whole point of the model in *Network Models, Encapsulation and Device Roles*. Here both MACs are all zeros because the loopback device has no hardware address.

**2.** In the hex the Ethernet header ends with **`08 00`**: the **EtherType** `0x0800` means "the payload is IPv4" (`0x86dd` would be IPv6, `0x0806` ARP). The next byte **`45`** packs two 4-bit fields: version **4** and header length **5** words of 4 bytes = 20 bytes. After the TOS byte, the total length (`00 21` = 33 = 20 + 8 + 5) and the identification (`12 34`), the flags/fragment field `40 00` (the **Don't Fragment** bit set), then the TTL **`40`** = 64 hops, and the protocol **`11`** = 17 = **UDP** (6 would be TCP, 1 ICMP). The UDP header carries the ports 40000 (`9c 40`) and 9999 (`27 0f`) and its own length 13 = 8 + 5. The parser prints the same values, and `ip_checksum_ok True` shows the header verifies.

**3.** The **Internet checksum** (RFC 1071) is the **ones' complement of the ones' complement sum of all 16-bit words**: add the words, fold any carry out of bit 16 back into the low bits, then invert. The sender computes it with the checksum field set to zero and stores the result. A receiver **sums everything including the checksum field**; a correct, undamaged header gives all ones, whose complement is `0`, which is what `checksum(header) == 0` tests. It catches most random bit errors but is weak (it cannot detect reordered words, for instance), so higher layers and link-layer CRCs add stronger checks, and TLS provides real integrity. The IPv4 checksum covers **only the header** (which is why routers must recompute it after decrementing the TTL). UDP's checksum covers the **payload plus a pseudo-header** made of the source and destination IP addresses, the protocol number and the UDP length: those fields are not in the UDP header itself, but a datagram delivered to the *wrong host* (because of a corrupted IP address) or in the wrong protocol must be detected, so they are included in the sum. In IPv4 the UDP checksum is optional (a zero value means "not computed"); in IPv6 it is mandatory, since IPv6 has no header checksum of its own. Our program turns a computed `0` into `0xFFFF` to avoid ambiguity, as RFC 768 requires.

**4.** The two frames differ in six bytes, in three fields: the **IP identification** (the kernel's `11c4` against my chosen `1234`; the standard only says it should help reassembly of fragments, so each sender picks its own values), the **IP header checksum** (`2b06` against `2a96`: it changes whenever any header byte changes, so it differs *because* the identification differs, and both versions verify correctly) and the **UDP checksum** (`fe20` against my `faaf`): on a modern system the kernel leaves checksum calculation to the network card (**checksum offload**) and stores only the pseudo-header sum (`fe20`: the sum of the address, protocol and length words) for the hardware to complete; on the loopback interface no card ever does, and the receiving stack knows to accept it. Everything else is identical: addresses, ports, TTL 64, the DF bit and the length fields. So most of a packet is **dictated by the protocol** (field positions, lengths, valid values, how the checksum is computed), while a few fields are the **sender's choice** (identification, TTL, TOS/DSCP, and options), and the checksum is **derived** from the others. Whenever you read an RFC for a new protocol, this is the split to look for: *what is fixed, what is chosen, and what is computed*. That matches the advice in *Reading RFCs and IEEE Standards Efficiently*, and it is also how packet generators and firewalls' parsers are written.

</details>

## Key takeaways

1. A frame is nested headers: Ethernet (MAC addresses, EtherType), IPv4 (addresses, TTL, protocol number, header checksum), UDP (ports, length, checksum), then the payload; each layer treats the next as opaque data.
2. The Internet checksum is the ones' complement of the ones' complement sum of 16-bit words; verifying means summing everything and getting zero.
3. The UDP checksum includes a pseudo-header with the IP addresses so a misdelivered datagram is detected; IPv4 header checksums are recomputed at every router because the TTL changes.
4. In any protocol some fields are fixed by the standard, some chosen by the sender, and some computed from the rest; building a packet by hand and comparing it with a capture is the fastest way to learn the format.
