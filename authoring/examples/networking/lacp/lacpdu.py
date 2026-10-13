#!/usr/bin/env python3
"""Capture one LACPDU leaving an interface and decode it: lacpdu.py IFACE SECONDS"""
import struct, sys
from ring import Tap

def state(b):
    names = ["Activity", "Timeout(short)", "Aggregation", "Synchronization", "Collecting", "Distributing", "Defaulted", "Expired"]
    return ",".join(n for i, n in enumerate(names) if b >> i & 1)

def info(tlv):
    prio, mac, key, pprio, port, st = struct.unpack("!H6sHHHB", tlv[:15])
    return f"system priority {prio}, key {key}, port priority {pprio}, port number {port}, state 0x{st:02x} [{state(st)}]"

for kind, pkt, ts in Tap(sys.argv[1]).packets(float(sys.argv[2])):
    dst, src, etype = pkt[:6], pkt[6:12], struct.unpack("!H", pkt[12:14])[0]
    if etype != 0x8809 or pkt[14] != 1: continue                                    # Slow Protocols, subtype 1 = LACP
    print("frame: dst", ":".join(f"{b:02x}" for b in dst), f"ethertype 0x{etype:04x}, subtype {pkt[14]}, version {pkt[15]}, {len(pkt)} bytes")
    p = 16
    while p < len(pkt) and pkt[p]:
        t, l = pkt[p], pkt[p + 1]; body = pkt[p + 2:p + l]
        if t == 1: print("  actor   :", info(body))
        elif t == 2: print("  partner :", info(body))
        elif t == 3: print("  collector max delay:", struct.unpack("!H", body[:2])[0])
        p += l
    break
else:
    print("no LACPDU seen")
