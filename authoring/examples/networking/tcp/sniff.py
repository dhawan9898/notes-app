#!/usr/bin/env python3
"""Tiny TCP packet printer (AF_PACKET raw socket): shows each segment's flags, relative seq/ack, payload length, window and options."""
import socket, struct, sys, time

iface, port, maxn = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
s = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.ntohs(3))      # ETH_P_ALL
s.bind((iface, 0)); s.settimeout(8)
isn = {}; t0 = None; n = 0
FLAGS = [("F", 0x01), ("S", 0x02), ("R", 0x04), ("P", 0x08), ("A", 0x10)]
def opts(raw):
    out, i = [], 0
    while i < len(raw):
        k = raw[i]
        if k == 0: break
        if k == 1: i += 1; continue
        ln = raw[i + 1]; body = raw[i + 2:i + ln]
        if k == 2: out.append(f"mss={struct.unpack('!H', body)[0]}")
        elif k == 3: out.append(f"wscale={body[0]}")
        elif k == 4: out.append("sackOK")
        elif k == 5: out.append(f"SACK[{len(body)//8}]")
        elif k == 8: out.append("ts")
        i += ln
    return ",".join(out)
try:
    while n < maxn:
        pkt, _ = s.recvfrom(65535)
        if len(pkt) < 54 or pkt[12:14] != b"\x08\x00" or pkt[23] != 6: continue
        ihl = (pkt[14] & 15) * 4; ip = pkt[14:14 + ihl]; tcp = pkt[14 + ihl:]
        sp, dp, seq, ack, off, fl, win = struct.unpack("!HHIIBBH", tcp[:16]); hl = (off >> 4) * 4
        if port not in (sp, dp): continue
        src, dst = socket.inet_ntoa(ip[12:16]), socket.inet_ntoa(ip[16:20])
        plen = struct.unpack("!H", ip[2:4])[0] - ihl - hl
        key = (src, sp)
        if fl & 2 and key not in isn: isn[key] = seq
        rseq = (seq - isn.get(key, seq)) & 0xFFFFFFFF
        peer = isn.get((dst, dp)); rack = (ack - peer) & 0xFFFFFFFF if (fl & 0x10 and peer is not None) else None
        t = time.time(); t0 = t0 or t
        flags = "".join(c for c, b in FLAGS if fl & b)
        print(f"{(t - t0) * 1000:8.1f} ms  {src}:{sp:<5} > {dst}:{dp:<5} [{flags:<3}] seq={rseq:<6} ack={'-' if rack is None else rack:<6} len={plen:<5} win={win:<6} {opts(tcp[20:hl])}", flush=True)
        n += 1
except socket.timeout:
    pass
