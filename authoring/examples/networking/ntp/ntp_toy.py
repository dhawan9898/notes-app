#!/usr/bin/env python3
"""A toy NTP exchange (RFC 5905 timestamps and formulas) over real UDP. The server answers with its real clock; the client's clock is
deliberately wrong by CLIENT_ERROR seconds. Usage: ntp_toy.py server | ntp_toy.py client [samples]"""
import socket, struct, sys, time

CLIENT_ERROR = 0.250                       # the client's clock runs 250 ms AHEAD of the server's
PORT = 12300
def now(skew=0.0): return time.time() + skew
def pack(t): return struct.pack("!d", t)

if sys.argv[1] == "server":
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.bind(("10.0.0.2", PORT))
    while True:
        data, peer = s.recvfrom(64); t2 = now()                      # T2: request received
        t3 = now(); s.sendto(data + pack(t2) + pack(t3), peer)        # T3: reply sent (echo T1, add T2 and T3)
else:
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.settimeout(2); results = []
    s.sendto(pack(0), ("10.0.0.2", PORT)); s.recvfrom(64)                    # warm-up exchange (the first packet also pays for ARP resolution); not used
    for _ in range(n):
        t1 = now(CLIENT_ERROR)                                       # T1: request sent, by the client's (wrong) clock
        s.sendto(pack(t1), ("10.0.0.2", PORT)); data, _ = s.recvfrom(64); t4 = now(CLIENT_ERROR)   # T4: reply received
        _, t2, t3 = (struct.unpack("!d", data[i:i + 8])[0] for i in (0, 8, 16))
        offset = ((t2 - t1) + (t3 - t4)) / 2                         # how far the server clock is ahead of the client's: the correction to apply
        delay = (t4 - t1) - (t3 - t2)                                # round-trip network delay, excluding the server's processing time
        results.append((delay, offset))
    best = min(results)                                              # the sample with the smallest delay is the least distorted by queueing
    print(f"  samples {n}: offset estimates {min(o for d, o in results) * 1e3:+.1f} .. {max(o for d, o in results) * 1e3:+.1f} ms; "
          f"lowest-delay sample: delay {best[0] * 1e3:.1f} ms, offset {best[1] * 1e3:+.1f} ms (true correction is {-CLIENT_ERROR * 1e3:+.0f} ms)")
