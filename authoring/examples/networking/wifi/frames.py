#!/usr/bin/env python3
"""Build an IEEE 802.11 beacon frame byte by byte, parse it back, and derive a WPA2 pairwise master key."""
import hashlib, struct

def mac(s): return bytes(int(x, 16) for x in s.split(":"))
def ie(eid, body): return bytes([eid, len(body)]) + body

# ---- management frame header: frame control, duration, addr1 (receiver/destination), addr2 (transmitter), addr3 (BSSID), sequence control ----
fc = 0x0080                                    # version 0, type 0 (management), subtype 8 (beacon)
bssid = mac("02:00:00:aa:bb:01")
header = struct.pack("<HH", fc, 0) + mac("ff:ff:ff:ff:ff:ff") + bssid + bssid + struct.pack("<H", 42 << 4)
# ---- beacon body: timestamp (8), beacon interval in TUs of 1024 us (2), capability (2), then tagged information elements ----
rsn = struct.pack("<H", 1) + b"\x00\x0f\xac\x04" + struct.pack("<H", 1) + b"\x00\x0f\xac\x04" + struct.pack("<H", 1) + b"\x00\x0f\xac\x02" + struct.pack("<H", 0x000c)  # RSN: group CCMP, pairwise CCMP, AKM PSK, caps
body = struct.pack("<QHH", 123456789, 100, 0x0431) + ie(0, b"DemoNet") + ie(1, bytes([0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c])) + ie(3, bytes([6])) + ie(48, rsn)
frame = header + body
print(f"beacon frame: {len(frame)} bytes (+4 byte FCS on the air)")
print("hex:", frame.hex(" ")[:96], "...")

# ---- parse it back ----
f0, dur = struct.unpack("<HH", frame[:4]); ftype, subtype = (f0 >> 2) & 3, (f0 >> 4) & 15
print(f"\nframe control 0x{f0:04x}: version {f0 & 3}, type {ftype} ({['management', 'control', 'data'][ftype]}), subtype {subtype} (beacon), ToDS={f0 >> 8 & 1}, FromDS={f0 >> 9 & 1}, protected={f0 >> 14 & 1}")
print("addr1 (destination):", ":".join(f"{b:02x}" for b in frame[4:10]), "  = broadcast: every station in range")
print("addr2 (transmitter):", ":".join(f"{b:02x}" for b in frame[10:16]), " addr3 (BSSID):", ":".join(f"{b:02x}" for b in frame[16:22]))
ts, interval, cap = struct.unpack("<QHH", frame[24:36])
print(f"timestamp {ts} us, beacon interval {interval} TU = {interval * 1.024:.1f} ms, capability 0x{cap:04x} (ESS={cap & 1}, privacy={cap >> 4 & 1})")
off = 36; names = {0: "SSID", 1: "Supported rates", 3: "DS parameter set (channel)", 48: "RSN (WPA2/WPA3 security)"}
while off < len(frame):
    eid, ln = frame[off], frame[off + 1]; v = frame[off + 2:off + 2 + ln]
    shown = v.decode() if eid == 0 else (f"channel {v[0]}" if eid == 3 else ("[" + ", ".join(f"{(x & 0x7f) * 0.5:g}{'*' if x & 0x80 else ''}" for x in v) + "] Mbit/s (* = basic rate)" if eid == 1 else v.hex()))
    print(f"  IE {eid:>2} {names.get(eid, '?'):<28} len {ln:<2} {shown}"); off += 2 + ln
print("  RSN detail: group cipher 00-0f-ac:4 (CCMP/AES), pairwise CCMP, AKM 00-0f-ac:2 (WPA2-PSK)")

# ---- WPA2-Personal: the pairwise master key (PMK) is PBKDF2-HMAC-SHA1(passphrase, SSID, 4096 iterations, 256 bits) ----
pmk = hashlib.pbkdf2_hmac("sha1", b"password", b"IEEE", 4096, 32)
vector = "f42c6fc52df0ebef9ebb4b90b38a5f902e83fe1b135a70e23aed762e9710a12e"          # test vector from IEEE Std 802.11i-2004 Annex H.4
print("\nPMK for passphrase 'password', SSID 'IEEE':", pmk.hex(), "\nmatches the 802.11i test vector:", pmk.hex() == vector)
