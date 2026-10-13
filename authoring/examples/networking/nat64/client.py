#!/usr/bin/env python3
"""IPv6-only client: resolves nothing, just connects to the NAT64 address of an IPv4 server: client.py V4ADDR"""
import socket, sys
sys.path.insert(0, ".")
from dns64 import synthesize
target = str(synthesize(sys.argv[1]))
print(f"  client (IPv6 only) uses the synthesized address {target}")
u = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM); u.settimeout(2); u.sendto(b"hello over udp", (target, 7000)); print("  UDP reply:", u.recvfrom(100)[0].decode())
t = socket.socket(socket.AF_INET6, socket.SOCK_STREAM); t.settimeout(3); t.connect((target, 7000)); t.sendall(b"x" * 20000 + b"hello over tcp"); t.shutdown(socket.SHUT_WR)
data = b""
while True:
    d = t.recv(65536)
    if not d: break
    data += d
print(f"  TCP reply: {len(data)} bytes, ends with {data[-14:].decode()!r}")
