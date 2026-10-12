#!/usr/bin/env python3
"""Print IPv6 packets with their Segment Routing Header: usage  sniff6.py IFACE COUNT [seconds]. Uses ring.Tap so captured bytes are a faithful copy."""
import socket, sys
from ring import Tap
iface, count = sys.argv[1], int(sys.argv[2]); wait = float(sys.argv[3]) if len(sys.argv) > 3 else 6
a = lambda b: socket.inet_ntop(socket.AF_INET6, b)
n = 0
for kind, p in Tap(iface).packets(wait):
    if len(p) < 54 or p[12:14] != b"\x86\xdd": continue
    nh, hl, src, dst = p[20], p[21], a(p[22:38]), a(p[38:54]); off = 54; arrow = "tx" if kind == 4 else "rx"
    line = f"{arrow} {src} > {dst} hlim={hl}"
    if nh == 43 and p[off + 2] == 4:                                           # routing header, type 4 = SRH
        left, last, nxt = p[off + 3], p[off + 4], p[off]
        segs = [a(p[off + 8 + 16 * i: off + 24 + 16 * i]) for i in range(last + 1)]
        line += f"\n      SRH: segments_left={left}  segment list (last SID first) {segs}"
        off += (p[off + 1] + 1) * 8; nh = nxt
    if nh == 41: line += f"\n      inner IPv6 {a(p[off + 8:off + 24])} > {a(p[off + 24:off + 40])}"
    elif nh == 58 and p[off] in (128, 129): line += f"  ICMPv6 {'echo request' if p[off] == 128 else 'echo reply'}"
    elif nh not in (41, 58) and not (nh == 43): continue
    elif nh == 58: continue
    print(line, flush=True); n += 1
    if n >= count: break
