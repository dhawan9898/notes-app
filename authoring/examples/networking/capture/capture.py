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
