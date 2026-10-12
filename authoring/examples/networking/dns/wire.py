"""Show the bytes of a DNS query and of the response from the authoritative server, field by field."""
import socket
from dnslib_mini import *

q = build_query(0x1234, "web.example.test.", "A", rd=0)
def hexrow(b): return " ".join(f"{x:02x}" for x in b)
print("QUERY (%d bytes)" % len(q))
print("  header  :", hexrow(q[:12]), " id=0x1234 flags=0x0000 (query, no recursion desired) QDCOUNT=1 ANCOUNT=0 NSCOUNT=0 ARCOUNT=0")
off = 12
for label in "web.example.test".split("."):
    print(f"  label   : {q[off]:02x} {label.encode().hex(' ')}   (length {q[off]}, then the text '{label}')"); off += 1 + len(label)
print(f"  end     : {q[off]:02x}   (zero-length root label ends the name)")
print("  question:", hexrow(q[off + 1:]), " QTYPE=1 (A) QCLASS=1 (IN)")
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.settimeout(2); s.sendto(q, ("10.1.1.3", 53)); r, _ = s.recvfrom(512)
print("\nRESPONSE (%d bytes)" % len(r))
print("  header  :", hexrow(r[:12]), " flags=0x%04x: QR=1 (response), AA=1 (authoritative), RCODE=0 (no error); ANCOUNT=%d" % (int.from_bytes(r[2:4], "big"), int.from_bytes(r[6:8], "big")))
print("  answer 1:", hexrow(r[len(q):len(q) + 18]), "... (this toy server writes the full name again; real servers usually write c0 0c, a compression pointer back to offset 12)")
p = parse(r)
for rr in p["answers"]: print("  decoded :", rr)
