#!/usr/bin/env python3
"""server: send for DURATION seconds using a chosen congestion control algorithm; client: count what arrives."""
import socket, sys, time

def retrans():
    with open("/proc/net/snmp") as f:
        lines = [l.split() for l in f if l.startswith("Tcp:")]
    return int(dict(zip(lines[0], lines[1]))["RetransSegs"])

role = sys.argv[1]
if role == "server":
    cc, duration = sys.argv[2], float(sys.argv[3])
    l = socket.socket(); l.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); l.bind(("10.0.0.2", 5001)); l.listen(1)
    c, _ = l.accept(); c.setsockopt(socket.IPPROTO_TCP, socket.TCP_CONGESTION, cc.encode())
    r0 = retrans(); chunk = b"x" * 65536; end = time.time() + duration
    while time.time() < end: c.sendall(chunk)
    c.shutdown(socket.SHUT_WR); c.recv(1); print(retrans() - r0)           # prints retransmitted segments
else:
    s = socket.socket()
    if len(sys.argv) > 2: s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, int(sys.argv[2]))      # optional: fixed receive buffer in bytes
    s.connect(("10.0.0.2", 5001)); t0 = time.time(); got = 0
    while True:
        d = s.recv(1 << 20)
        if not d: break
        got += len(d)
    dt = time.time() - t0; print(f"{got * 8 / dt / 1e6:.1f}")                 # Mbit/s
