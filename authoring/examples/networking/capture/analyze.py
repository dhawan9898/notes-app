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
