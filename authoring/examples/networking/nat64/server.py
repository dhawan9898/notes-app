#!/usr/bin/env python3
"""IPv4-only echo server on TCP and UDP port 7000, runs for N seconds and reports what addresses it saw: server.py ADDR SECONDS"""
import select, socket, sys, time
addr, secs = sys.argv[1], float(sys.argv[2])
u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); u.bind((addr, 7000))
t = socket.socket(socket.AF_INET, socket.SOCK_STREAM); t.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); t.bind((addr, 7000)); t.listen(5)
end = time.time() + secs
while time.time() < end:
    r, _, _ = select.select([u, t], [], [], 0.1)
    for s in r:
        if s is u:
            d, a = u.recvfrom(2048); u.sendto(d.upper(), a); print(f"  server saw UDP from {a[0]}:{a[1]}", flush=True)
        else:
            c, a = t.accept(); print(f"  server saw TCP from {a[0]}:{a[1]}", flush=True)
            c.settimeout(3); d = b""
            while True:                                 # read until the client's FIN, then answer (closing with unread data would send a RST)
                chunk = c.recv(65536)
                if not chunk: break
                d += chunk
            c.sendall(d.upper()); c.close()
