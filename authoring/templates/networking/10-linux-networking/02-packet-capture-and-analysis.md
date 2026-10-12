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

{{include networking/capture/ring.py}}

{{include networking/capture/capture.py}}

{{include networking/capture/analyze.py}}

{{include networking/capture/lab.sh}}

{{run networking/capture :: unshare -Urnm sh lab.sh 2>&1}}

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
