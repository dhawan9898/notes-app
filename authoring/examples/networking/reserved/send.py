#!/usr/bin/env python3
"""Send one Ethernet frame to each of several destination MACs: send.py IFACE"""
import socket, struct, sys
iface = sys.argv[1]
s = socket.socket(socket.AF_PACKET, socket.SOCK_RAW); s.bind((iface, 0))
src = s.getsockname()[4]

def tlv(t, v): return struct.pack("!H", t << 9 | len(v)) + v
lldpdu = (tlv(1, b"\x04" + src) + tlv(2, b"\x05" + b"port-1") + tlv(3, struct.pack("!H", 120)) +
          tlv(5, b"switch-a") + tlv(4, b"uplink to the core") + tlv(0, b""))

cases = [  # (label, destination MAC, ethertype, payload)
    ("LLDP (nearest bridge)",    "01:80:c2:00:00:0e", 0x88CC, lldpdu),
    ("STP BPDU group address",   "01:80:c2:00:00:00", 0x0042, b"\x42\x42\x03" + b"\xee" * 35),   # 802.3 length-style frame with an LLC header; the 0xEE filler marks it as ours
    ("Slow protocols (LACP)",    "01:80:c2:00:00:02", 0x8809, b"\x01" + bytes(100)),
    ("802.3x PAUSE",             "01:80:c2:00:00:01", 0x8808, struct.pack("!HH", 1, 0xffff) + bytes(42)),
    ("802.1X EAPOL group",       "01:80:c2:00:00:03", 0x888E, bytes(46)),
    ("ordinary multicast",       "01:00:5e:00:00:fb", 0x88B5, bytes(46)),   # local experimental ethertype, so no IP snooping logic applies
    ("broadcast",                "ff:ff:ff:ff:ff:ff", 0x0806, bytes(46)),
]
for label, dst, etype, payload in cases:
    if etype == 0x0042:                                           # 802.3 length field instead of an ethertype
        etype = len(payload)
    frame = bytes.fromhex(dst.replace(":", "")) + src + struct.pack("!H", etype) + payload
    s.send(frame.ljust(60, b"\0"))
