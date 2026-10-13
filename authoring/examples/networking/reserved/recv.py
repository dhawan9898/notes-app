#!/usr/bin/env python3
"""List the destination MAC of every frame that arrives on IFACE within SECONDS, naming the known reserved addresses; decode LLDP."""
import struct, sys
from ring import Tap
NAMES = {"01:80:c2:00:00:0e": "LLDP", "01:80:c2:00:00:00": "STP BPDU", "01:80:c2:00:00:02": "Slow protocols (LACP)", "01:80:c2:00:00:01": "PAUSE",
         "01:80:c2:00:00:03": "EAPOL group", "01:00:5e:00:00:fb": "ordinary multicast", "ff:ff:ff:ff:ff:ff": "broadcast"}
seen = []
for kind, pkt, ts in Tap(sys.argv[1]).packets(float(sys.argv[2])):
    if kind == 4: continue
    dst = ":".join(f"{b:02x}" for b in pkt[:6])
    if dst in NAMES:
        name = NAMES[dst]
        if dst.endswith("00") and pkt[17] != 0xee: name = "STP BPDU (sent by the bridge itself)"
        seen.append(name)
        if dst.endswith("0e"):                                                    # decode the LLDP TLVs
            p = 14
            while p + 2 <= len(pkt):
                h = struct.unpack("!H", pkt[p:p + 2])[0]; t, l = h >> 9, h & 0x1ff; v = pkt[p + 2:p + 2 + l]
                if t == 0: break
                label = {1: "Chassis ID", 2: "Port ID", 3: "TTL", 4: "Port Description", 5: "System Name"}.get(t, f"TLV {t}")
                shown = struct.unpack("!H", v)[0] if t == 3 else v[1:].decode(errors="replace") if t == 2 else v.decode(errors="replace") if t in (4, 5) else "subtype %d, %s" % (v[0], ":".join("%02x" % b for b in v[1:]))
                print(f"    {label:<17}= {shown}"); p += 2 + l
print("  frames that arrived:", ", ".join(seen) if seen else "(none)")
