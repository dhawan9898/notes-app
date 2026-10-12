---
title: "Packet Capture and Analysis"
track: networking
module: Linux Networking Labs
moduleOrder: 10
order: 2
level: intermediate
summary: "Where and how to capture, capture versus display filters, the pcap file format, a method for reading a trace, and a worked analysis: capture a lossy TCP transfer at the sender, write a pcap, and extract handshake RTT, retransmissions, duplicate ACKs and goodput with a script, including the segmentation-offload effect."
prerequisites:
  - "TCP and UDP: The Transport Layer"
  - "Linux Network Labs: Namespaces, veth, Bridges, VLANs, tc and nftables"
sources:
  - label: pcap-savefile(5) — libpcap capture file format
    url: https://www.tcpdump.org/manpages/pcap-savefile.5.html
  - label: pcap-filter(7) — packet filter syntax (BPF)
    url: https://www.tcpdump.org/manpages/pcap-filter.7.html
  - label: Wireshark User's Guide — display filters and the capture file formats
    url: https://www.wireshark.org/docs/wsug_html_chunked/
  - label: tcpdump(8) and tshark(1) — manual pages
    url: https://www.tcpdump.org/manpages/tcpdump.1.html
  - label: Linux kernel documentation — Packet mmap (PACKET_RX_RING, TPACKET)
    url: https://www.kernel.org/doc/html/latest/networking/packet_mmap.html
  - label: Sanders — Practical Packet Analysis, 3rd ed.
---

When a network problem resists explanation from counters and logs, the packets themselves are the ground truth: what was actually sent, in what order, at what time, and what the other side answered. **Packet capture** records packets; **analysis** turns a trace into a diagnosis. The skill is mostly choosing *where* to capture, filtering sensibly, and reading a trace methodically.

## Where to capture

| Capture point | How | Sees | Caveats |
|---|---|---|---|
| **On the host** | `tcpdump -i eth0` | all traffic of that host's interface (plus loopback and virtual interfaces) | offloads change what you see (below); the capture itself costs CPU |
| **On a switch or router port (SPAN / port mirror)** | the device copies traffic from ports/VLANs to a monitor port | traffic of other hosts | oversubscribed mirror ports drop packets; rewritten VLAN tags; not a perfect copy |
| **A network TAP** | passive hardware in the cable path | every packet, both directions, with accurate timestamps | needs physical access |
| **Remote** (ERSPAN/GRE-encapsulated mirror, `tcpdump -w -` over SSH) | encapsulated copy to a collector | remote switch ports | bandwidth and MTU overhead |
| **Inside containers/namespaces** | `ip netns exec NS tcpdump ...` or `nsenter` | traffic of that namespace | needs the right namespace |

Capturing at **both ends** of a path and comparing is the standard method for proving where a packet disappeared: seen at the sender but not at the receiver means the network ate it; not seen at the sender means the host never sent it.

## Capture filters versus display filters

These are two different languages, and confusing them is the most common beginner mistake.

- A **capture filter** (BPF, "Berkeley Packet Filter" syntax) is compiled into the kernel and decides which packets are *recorded at all*. It is fast and keeps files small, but whatever it drops is gone for ever.
- A **display filter** (Wireshark/tshark) is applied afterwards to a trace you already have, with a rich protocol-aware language.

| Goal | Capture filter (`tcpdump`, BPF) | Display filter (Wireshark) |
|---|---|---|
| One host | `host 10.0.0.5` | `ip.addr == 10.0.0.5` |
| One port | `port 443` | `tcp.port == 443` |
| Source only | `src host 10.0.0.5` | `ip.src == 10.0.0.5` |
| TCP SYNs | `tcp[tcpflags] & tcp-syn != 0` | `tcp.flags.syn == 1` |
| SYN without ACK | `tcp[tcpflags] & (tcp-syn\|tcp-ack) == tcp-syn` | `tcp.flags.syn==1 && tcp.flags.ack==0` |
| ICMP | `icmp or icmp6` | `icmp \|\| icmpv6` |
| DNS | `udp port 53` | `dns` |
| ARP | `arp` | `arp` |
| A subnet | `net 10.0.0.0/24` | `ip.addr == 10.0.0.0/24` |
| VLAN | `vlan 20` | `vlan.id == 20` |
| TCP problems | (not expressible) | `tcp.analysis.flags` (retransmissions, duplicate ACKs, zero windows...) |
| Slow responses | (not expressible) | `http.time > 1` or `tcp.time_delta > 1` |

Typical command lines (reference: `tcpdump` and `tshark` are not installed in the environment that produced the experiment below, so these were not run for this note):

```bash
tcpdump -nn -i eth0 -s 0 -w trace.pcap 'host 10.0.0.5 and tcp port 443'   # -nn: no name/port lookups; -s 0: whole packets; -w: write a file
tcpdump -nn -r trace.pcap -c 20                                           # read a file, first 20 packets
tcpdump -nn -i eth0 -C 100 -W 10 -w ring.pcap                             # ring buffer: ten files of 100 MB, the oldest overwritten
tshark -r trace.pcap -Y 'tcp.analysis.retransmission' -T fields -e frame.time_relative -e ip.src -e tcp.seq
tshark -r trace.pcap -q -z conv,tcp                                       # conversation table
tshark -r trace.pcap -q -z io,stat,1,"COUNT(tcp.analysis.retransmission)" # per-second statistics
```

Always use `-nn` on a busy host (reverse DNS lookups during capture distort timing and generate their own traffic), capture whole packets (`-s 0` or a large snap length) unless privacy requires truncation, and write to a file (`-w`) so you can analyse repeatedly instead of reading scrolling text.

## The pcap file format

A classic capture file is trivially simple, which is why every tool reads it: a **24-byte global header** (magic number `a1b2c3d4`, version 2.4, time zone, snap length, link-layer type, 1 = Ethernet) followed by records of a **16-byte record header** (timestamp seconds and microseconds, captured length, original length) and the raw packet bytes. The modern **pcapng** format adds interface descriptions, per-packet comments and nanosecond timestamps. The lab below writes a real pcap file with 30 lines of Python and analyses it with 80, which is the fastest way to understand what tools like Wireshark do.

## Offloads: what a capture on the host actually shows

Modern network cards and the kernel offload work to hardware: **TSO/GSO** (transmit segmentation: the TCP stack hands the card one huge "segment" of up to 64 KB, and the card cuts it into MTU-sized packets), **GRO/LRO** (receive aggregation), and checksum offload. A capture on the *host* sees the packet *before* the card splits it, so you may see "TCP segments" of 14,480 or 65,000 bytes on a 1500-byte network, and checksums that appear wrong (not yet computed). Analysis programs flag these as errors; they are not. For an exact view of what is on the wire, turn the offloads off for the capture (`ethtool -K eth0 tso off gso off gro off`), or capture on the switch side. The lab shows the effect.

## A worked analysis

The lab creates a client and a server in separate network namespaces joined by a virtual cable with `tc netem` adding 10 ms of delay in each direction (RTT about 20 ms), 1 % random loss and a 50 Mbit/s rate limit. The server sends for five seconds; a capture program (Python, using the kernel's memory-mapped capture ring so every packet is copied at the moment it passes) records everything on the **sender's** interface into a pcap file, and the analysis script then reads that file back and prints what an engineer would look for.

```python
"""Packet capture through a memory-mapped RX ring (TPACKET_V2): the kernel COPIES each packet into the ring at the moment it passes the tap.
A plain AF_PACKET socket only queues a clone that shares the packet's data; if the next hop rewrites the header in place (as an SRv6 End does),
a late recvfrom() would show the rewritten bytes. The ring avoids that."""
import mmap, socket, struct, time

SOL_PACKET, PACKET_RX_RING, PACKET_VERSION, TPACKET_V2 = 263, 5, 10, 1
BLOCK, FRAMES, FRAME = 1 << 16, 64, 1 << 11                                 # 64 frames of 2 KiB in one 128 KiB-ish ring

class Tap:
    def __init__(self, iface):
        self.s = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.ntohs(3))
        self.s.setsockopt(SOL_PACKET, PACKET_VERSION, TPACKET_V2)
        nblocks = FRAMES * FRAME // BLOCK
        self.s.setsockopt(SOL_PACKET, PACKET_RX_RING, struct.pack("IIII", BLOCK, nblocks, FRAME, FRAMES))
        self.ring = mmap.mmap(self.s.fileno(), BLOCK * nblocks); self.s.bind((iface, 0)); self.i = 0

    def packets(self, timeout):
        """Yield (pkttype, raw_bytes, timestamp); pkttype 0 = received from the wire, 4 = sent by this host."""
        end = time.time() + timeout
        while time.time() < end:
            off = self.i * FRAME
            status = struct.unpack_from("I", self.ring, off)[0]
            if status & 1:                                                  # TP_STATUS_USER: the kernel filled this slot
                snaplen, mac = struct.unpack_from("I", self.ring, off + 8)[0], struct.unpack_from("H", self.ring, off + 12)[0]
                pkttype = self.ring[off + 42]; sec, nsec = struct.unpack_from("II", self.ring, off + 16)
                yield pkttype, bytes(self.ring[off + mac: off + mac + snaplen]), sec + nsec / 1e9
                struct.pack_into("I", self.ring, off, 0); self.i = (self.i + 1) % FRAMES        # give the slot back to the kernel
            else:
                time.sleep(0.002)
```

```python
#!/usr/bin/env python3
"""Write a classic libpcap file (readable by tcpdump and Wireshark): capture.py IFACE OUTFILE SECONDS"""
import struct, sys
from ring import Tap
iface, out, secs = sys.argv[1], sys.argv[2], float(sys.argv[3]); n = 0
with open(out, "wb") as f:
    f.write(struct.pack("<IHHiIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1))                 # global header: magic, version 2.4, tz, sigfigs, snaplen, linktype 1 = Ethernet
    for kind, pkt, ts in Tap(iface).packets(secs):
        f.write(struct.pack("<IIII", int(ts), int((ts % 1) * 1e6), len(pkt), len(pkt)) + pkt); n += 1
print(f"{n} packets written to {out}")
```

```python
#!/usr/bin/env python3
"""Analyse a pcap of TCP traffic captured at the SENDER: handshake RTT, retransmissions, duplicate ACKs, window, goodput per interval."""
import socket, struct, sys
from collections import defaultdict

def read_pcap(path):
    with open(path, "rb") as f:
        magic, = struct.unpack("<I", f.read(24)[:4]); assert magic == 0xA1B2C3D4
        while True:
            h = f.read(16)
            if len(h) < 16: return
            sec, usec, incl, orig = struct.unpack("<IIII", h); yield sec + usec / 1e6, f.read(incl)

def tcp(pkt):
    if len(pkt) < 54 or pkt[12:14] != b"\x08\x00" or pkt[23] != 6: return None
    ihl = (pkt[14] & 15) * 4; t = pkt[14 + ihl:]; sp, dp, seq, ack, off, fl, win = struct.unpack("!HHIIBBH", t[:16]); hl = (off >> 4) * 4
    plen = struct.unpack("!H", pkt[16:18])[0] - ihl - hl
    return socket.inet_ntoa(pkt[26:30]), sp, socket.inet_ntoa(pkt[30:34]), dp, seq, ack, fl, win, plen

path = sys.argv[1]; pkts = [(t, tcp(p), len(p)) for t, p in read_pcap(path)]; pkts = [(t, x, n) for t, x, n in pkts if x]
t0 = pkts[0][0]
flows = defaultdict(list)
for t, x, n in pkts: flows[tuple(sorted([(x[0], x[1]), (x[2], x[3])]))].append((t - t0, x, n))
print(f"{len(pkts)} TCP packets, {len(flows)} connection(s), capture lasted {pkts[-1][0] - t0:.2f} s\n")
for key, seg in flows.items():
    (a, pa), (b, pb) = key
    syn = [s for s in seg if s[1][6] & 0x12 == 0x02]; synack = [s for s in seg if s[1][6] & 0x12 == 0x12]
    data_from = max(((x[0], x[1]) for _, x, _ in seg), key=lambda k: sum(x[8] for _, x, _ in seg if (x[0], x[1]) == k))     # the side that sent the most payload = the sender
    sender = [s for s in seg if (s[1][0], s[1][1]) == data_from]; recv = [s for s in seg if (s[1][0], s[1][1]) != data_from]
    print(f"connection {a}:{pa} <-> {b}:{pb}; data flows from {data_from[0]}:{data_from[1]}")
    if syn and synack: print(f"  handshake: SYN at {syn[0][0] * 1e3:.1f} ms, SYN+ACK at {synack[0][0] * 1e3:.1f} ms -> round-trip time seen at the capture point = {(synack[0][0] - syn[0][0]) * 1e3:.1f} ms")
    highest_end, retrans, retrans_bytes, total_bytes, biggest = None, 0, 0, 0, 0
    for _, x, _ in sender:
        if x[8] == 0: continue
        total_bytes += x[8]; biggest = max(biggest, x[8]); end = x[4] + x[8]
        if highest_end is not None and ((x[4] - highest_end) & 0xFFFFFFFF) > 0x7FFFFFFF:      # starts BELOW data already sent: a retransmission (or reordering)
            retrans += 1; retrans_bytes += x[8]
        if highest_end is None or ((end - highest_end) & 0xFFFFFFFF) < 0x7FFFFFFF: highest_end = end
    last_ack, dup, dups = None, 0, 0
    for _, x, _ in recv:
        if x[8] == 0 and x[6] & 0x12 == 0x10:
            if x[5] == last_ack: dup += 1
            else: last_ack = x[5]
    ndata = sum(1 for _, x, _ in sender if x[8] > 0)
    print(f"  sender: {ndata} data segments, {total_bytes / 1e6:.2f} MB sent (including retransmissions); largest segment {biggest} bytes" + (" (bigger than the 1448-byte MSS: segmentation offload, the tap sees the packet before the NIC splits it)" if biggest > 1448 else ""))
    print(f"  retransmitted segments: {retrans} ({100 * retrans / max(1, ndata):.1f}% of data segments), {retrans_bytes / 1e3:.0f} kB")
    print(f"  duplicate ACKs received: {dup}; minimum/maximum advertised window seen: {min(x[7] for _, x, _ in recv)} / {max(x[7] for _, x, _ in recv)} (raw field, scaled by the window-scale option)")
    acked = [(t, x[5]) for t, x, _ in recv if x[6] & 0x10]; base = acked[0][1]
    print("  goodput (bytes acknowledged per 0.5 s interval):")
    for i in range(int(seg[-1][0] / 0.5) + 1):
        lo, hi = i * 0.5, (i + 1) * 0.5; a0 = max([v for t, v in acked if t < lo] or [base]); a1 = max([v for t, v in acked if t < hi] or [a0])
        print(f"    {lo:4.1f}-{hi:4.1f} s  {((a1 - a0) & 0xFFFFFFFF) * 8 / 0.5 / 1e6:6.1f} Mbit/s " + "#" * int(((a1 - a0) & 0xFFFFFFFF) * 8 / 0.5 / 1e6 / 2))
```

```bash
#!/bin/sh
# Capture a TCP transfer at the sender over a lossy 20 ms path, write a pcap, then analyse it with a script.
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
ip netns add client; ip netns add server; ip link add vc type veth peer name vs; ip link set vc netns client; ip link set vs netns server
ip -n client addr add 10.0.0.1/24 dev vc; ip -n server addr add 10.0.0.2/24 dev vs
for x in "client vc" "server vs"; do set -- $x; ip -n $1 link set $2 up; ip -n $1 link set lo up; ip netns exec $1 tc qdisc add dev $2 root netem delay 10ms loss 1% rate 50mbit limit 1000; done
ip netns exec server python3 -c '
import socket, time
l = socket.socket(); l.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); l.bind(("10.0.0.2", 5001)); l.listen(1)
c, _ = l.accept(); end = time.time() + 5
while time.time() < end: c.sendall(b"x" * 65536)
c.close()' &
sleep 0.5
ip netns exec server python3 capture.py vs capture.pcap 7 &
sleep 0.5
ip netns exec client python3 -c '
import socket
s = socket.create_connection(("10.0.0.2", 5001)); n = 0
while True:
    d = s.recv(1 << 20)
    if not d: break
    n += len(d)
print("client received", n, "bytes")'
wait
python3 analyze.py capture.pcap
echo; echo "file: $(ls -l capture.pcap | awk '{print $5}') bytes; first 4 bytes (magic number, little-endian a1b2c3d4): $(head -c 4 capture.pcap | od -An -tx1)"
```

```text
$ unshare -Urnm sh lab.sh 2>&1
client received 7536640 bytes
4818 packets written to capture.pcap
4807 TCP packets, 1 connection(s), capture lasted 5.83 s

connection 10.0.0.1:49730 <-> 10.0.0.2:5001; data flows from 10.0.0.2:5001
  handshake: SYN at 0.0 ms, SYN+ACK at 10.1 ms -> round-trip time seen at the capture point = 10.1 ms
  sender: 2608 data segments, 7.54 MB sent (including retransmissions); largest segment 14480 bytes (bigger than the 1448-byte MSS: segmentation offload, the tap sees the packet before the NIC splits it)
  retransmitted segments: 34 (1.3% of data segments), 74 kB
  duplicate ACKs received: 181; minimum/maximum advertised window seen: 502 / 64240 (raw field, scaled by the window-scale option)
  goodput (bytes acknowledged per 0.5 s interval):
     0.0- 0.5 s    26.1 Mbit/s #############
     0.5- 1.0 s    15.8 Mbit/s #######
     1.0- 1.5 s     6.4 Mbit/s ###
     1.5- 2.0 s     6.5 Mbit/s ###
     2.0- 2.5 s     6.8 Mbit/s ###
     2.5- 3.0 s     6.6 Mbit/s ###
     3.0- 3.5 s     8.9 Mbit/s ####
     3.5- 4.0 s     6.3 Mbit/s ###
     4.0- 4.5 s    10.7 Mbit/s #####
     4.5- 5.0 s     9.9 Mbit/s ####
     5.0- 5.5 s     7.9 Mbit/s ###
     5.5- 6.0 s     8.6 Mbit/s ####

file: 5212296 bytes; first 4 bytes (magic number, little-endian a1b2c3d4):  d4 c3 b2 a1
```

What the analysis shows, and how to read each line:

- **Handshake timing**: the time between the `SYN` (timestamped when it *arrives* at the capture point) and the `SYN+ACK` (timestamped when it *leaves*) is 10 ms here. At a capture taken on the server that interval is the server-side part of the path only: the emulated 10 ms delay of the server's own interface; the client-to-server leg happened *before* the SYN reached the tap. To measure the full round-trip time, capture at the *client* (SYN leaving to SYN+ACK arriving) or use the first data ACK exchange. It is the first number to check in a slow-connection investigation: a slow handshake means the path, not the application.
- **Largest segment** far above the 1448-byte MSS: **segmentation offload**. The tap sees the 14,480-byte super-segment (ten MSS-sized packets) before the card would split it.
- **Retransmitted segments**: a data segment whose sequence number is *below* data already sent was sent again. About 1 % of segments here, matching the 1 % loss configured. Notice a subtlety: the original lost packets never appear in this capture, because the loss is applied (by the emulation) *after* the capture point's tap when the packet leaves the host; a capture at the receiver would show gaps instead. **Always say where a capture was taken.**
- **Duplicate ACKs**: the receiver's repeated acknowledgments for the same byte, which tell the sender "something earlier is missing, but I keep receiving later data": the trigger for fast retransmit. Their count is the receiver's view of the losses.
- **Window values** are raw fields. The scaling factor from the SYN options (`wscale=7`, multiply by 128) is needed to interpret them; Wireshark does this automatically when it has seen the handshake, which is why *captures that start mid-connection show wrong windows*.
- **Goodput per interval** (bytes acknowledged per half second): the throughput the application actually got: high at the start of the connection while the window opens, then low and ragged as each loss halves the congestion window, which is the saw-tooth behavior explained in the TCP note.
- The file itself begins with the bytes `d4 c3 b2 a1`: the magic number `a1b2c3d4` stored little-endian, which tells readers the byte order of the rest.

## A method for reading a trace

1. **State the question.** "Why is this transfer slow?" "Why does the connection fail?" Without one you will wander.
2. **Record where and when** it was captured, from which host, with what filter, with what offloads and what clock accuracy.
3. **Find the conversation** (conversation table: who talked to whom, how much).
4. **Check the handshake**: did it complete? How long did it take? Which options were negotiated (MSS, window scale, SACK, timestamps)?
5. **Look for the problem signatures** (table below), then follow the single worst conversation packet by packet.
6. **Compare both ends** if you can: the difference between "sent" and "received" is the network's contribution.
7. **Conclude with evidence**: a statement such as "20 ms RTT, 1.1 % retransmissions, window never exceeded 64 KB" is a diagnosis; "the network is slow" is not.

| What you see | What it usually means |
|---|---|
| `SYN` repeated, no reply | firewall dropping, wrong address/route, host down |
| `SYN` answered by `RST` | nothing listening on that port |
| Handshake fine, then a long gap before data | application delay (server slow), not network |
| Retransmissions with duplicate ACKs | packet loss on the path; fast retransmit |
| Retransmissions with exponentially growing gaps | timeouts (RTO backoff): heavy loss or dead path |
| Window field of zero (`TCP ZeroWindow`) | receiver cannot keep up (slow application or full buffer) |
| Large packets never acknowledged, small ones fine | path MTU black hole (ICMP "fragmentation needed" blocked) |
| `RST` in the middle of a connection | a firewall/middlebox or the peer aborted; check who sent it (TTL/MAC differs) |
| ARP requests without replies | host down or wrong VLAN/subnet |
| Many DNS queries with `NXDOMAIN` | typos, search-domain expansion, misconfiguration |
| TLS `Alert` after `ClientHello` | version/cipher/SNI mismatch |
| Duplicate packets | mirroring loop or both a switch mirror and a host capture |

## Encrypted traffic and privacy

TLS hides the payload, but you can still see addresses, ports, timing, sizes, the unencrypted `ClientHello` (including the SNI hostname) and TCP behavior, which is enough for most network diagnosis. To read the application data of your *own* connections, make the client write TLS session secrets to a file (`SSLKEYLOGFILE`) and load that into Wireshark. A capture contains everything that crossed the wire, including passwords on unencrypted protocols and personal data: capture only what you are authorised to capture, restrict file permissions, and delete files you no longer need.

## Capturing in Linux networking labs

Because namespaces and virtual interfaces are ordinary network devices, you can capture on any of them (`ip netns exec NS tcpdump -i veth0`), on bridges (every port's traffic) and on tunnel interfaces (`vxlan0` shows the *inner* packets, the underlay interface shows the encapsulated ones). **Kernel capture drops** occur if the reader is too slow for the capture ring: `tcpdump` reports "packets dropped by kernel" at the end, which means the trace is incomplete. Remember also that a **host capture is not a perfect wire view**: outgoing packets are tapped when they are handed to the driver (before the card, after the qdisc), and `tc netem` effects are applied around that point, exactly as in the lab above.

## Pitfalls

- Using a **capture filter** that excludes the packets you later need (re-capture is impossible for a past event). When in doubt, capture everything for the host and filter later.
- Reading checksum "errors" and oversized segments as faults when they are offload artifacts.
- Forgetting **where** the capture was taken when interpreting loss and retransmissions.
- Mis-reading windows, sequence numbers, or RTT without the handshake in the trace.
- Clock differences between two capture hosts: compare relative times within each trace, or synchronise clocks (NTP/PTP).
- Capturing for hours into one file; use ring buffers.
- Privacy and legal exposure.

## Key takeaways

1. Capture where the question can be answered, record where and how you captured, and ideally capture at both ends.
2. Capture filters (BPF) decide what is stored; display filters decide what you look at; do not mix the syntaxes.
3. A pcap file is a 24-byte header plus 16-byte record headers and raw packets; a script can analyse it.
4. Host captures are affected by offloads (large segments, bad checksums); the lab's 14,480-byte "segments" are an example.
5. The handshake, retransmission and duplicate-ACK counts, the window values and the goodput over time give a quantitative diagnosis of a slow transfer.
6. Follow a method: question, conversation, handshake, signature, comparison, evidence.
