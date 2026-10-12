#!/usr/bin/env python3
"""A minimal SNMP poller: reads interface counters from an agent twice, with traffic in between, and computes the rate the way monitoring systems do."""
import socket, subprocess, time

AGENT = "10.0.0.2"
def get(oid, v3=False):
    cmd = ["snmpget", "-Oqv", "-On"] + (["-v3", "-l", "authPriv", "-u", "nmsuser", "-a", "SHA", "-A", "authpass123", "-x", "AES", "-X", "privpass123"] if v3 else ["-v2c", "-c", "demo"]) + [AGENT, oid]
    return subprocess.run(cmd, capture_output=True, text=True).stdout.strip()
def walk(oid):
    out = subprocess.run(["snmpwalk", "-Oqn", "-v2c", "-c", "demo", AGENT, oid], capture_output=True, text=True).stdout.strip().splitlines()
    return [(l.split()[0], l.split(None, 1)[1].strip('"')) for l in out]

print("sysDescr.0  =", get("1.3.6.1.2.1.1.1.0")); print("sysName.0   =", get("1.3.6.1.2.1.1.5.0")); print("sysUpTime.0 =", get("1.3.6.1.2.1.1.3.0"))
idx = [o.rsplit(".", 1)[1] for o, v in walk("1.3.6.1.2.1.2.2.1.2") if v == "va"][0]
print(f"interface 'va' has ifIndex {idx}")
IN32, IN64 = f"1.3.6.1.2.1.2.2.1.10.{idx}", f"1.3.6.1.2.1.31.1.1.1.6.{idx}"          # ifInOctets (Counter32), ifHCInOctets (Counter64)
a32, a64, t0 = int(get(IN32)), int(get(IN64)), time.time()
s = socket.create_connection((AGENT, 9000)); sent = 0
while sent < 40_000_000: s.sendall(b"x" * 65536); sent += 65536                          # push about 40 MB toward the agent host as fast as possible
s.close()
early = int(get(IN64))                                                                    # poll immediately: the agent has not refreshed its interface cache yet
time.sleep(7)                                                                             # net-snmp refreshes its interface table every few seconds
b32, b64, t1 = int(get(IN32)), int(get(IN64)), time.time()
print(f"\nthe poller pushed {sent} payload bytes to the agent host in well under a second (a burst of several Gbit/s on this virtual cable)")
print(f"a poll immediately after the burst returned a delta of {early - a64} bytes: the agent serves counters from a cache that is only refreshed every few seconds")
print(f"ifHCInOctets delta = {b64 - a64} bytes over the {t1 - t0:.1f} s polling interval  ->  {(b64 - a64) * 8 / (t1 - t0) / 1e6:.0f} Mbit/s AVERAGE (the counter includes Ethernet and IP headers, so a little more than the payload)")
print(f"ifInOctets   delta = {(b32 - a32) % 2 ** 32} bytes (32-bit counter, taken modulo 2^32)")
print("\n32-bit counter wrap: a Counter32 wraps after 2^32 bytes = 4.29 GB; at 1 Gbit/s that takes", round(2 ** 32 * 8 / 1e9, 1), "s; at 10 Gbit/s", round(2 ** 32 * 8 / 10e9, 1), "s.")
print("Poll more slowly than that and a delta is ambiguous (it may have wrapped several times): use the 64-bit ifHC* counters.")
print("\nSNMPv3 authPriv read of the same 64-bit counter:", get(IN64, v3=True))
