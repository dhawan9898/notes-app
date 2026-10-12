"""A minimal DNS wire-format encoder/decoder (RFC 1035) used by the lab: just enough for A, AAAA, NS, CNAME, MX, TXT, SOA."""
import socket, struct

TYPES = {"A": 1, "NS": 2, "CNAME": 5, "SOA": 6, "MX": 15, "TXT": 16, "AAAA": 28}
NAMES = {v: k for k, v in TYPES.items()}
RCODES = {0: "NOERROR", 1: "FORMERR", 2: "SERVFAIL", 3: "NXDOMAIN", 5: "REFUSED"}

def enc_name(name):
    out = b""
    for label in name.rstrip(".").split("."):
        if label: out += bytes([len(label)]) + label.encode()
    return out + b"\x00"

def dec_name(msg, off):
    labels, jumped, end = [], False, None
    while True:
        n = msg[off]
        if n == 0: off += 1; break
        if n & 0xC0 == 0xC0:                                   # compression pointer: the name continues elsewhere in the message
            ptr = ((n & 0x3F) << 8) | msg[off + 1]
            if not jumped: end = off + 2
            off, jumped = ptr, True; continue
        labels.append(msg[off + 1:off + 1 + n].decode()); off += 1 + n
    return ".".join(labels) + ".", (end if jumped else off)

def rdata_enc(t, v):
    if t == "A": return socket.inet_aton(v)
    if t == "AAAA": return socket.inet_pton(socket.AF_INET6, v)
    if t in ("NS", "CNAME"): return enc_name(v)
    if t == "MX": return struct.pack("!H", v[0]) + enc_name(v[1])
    if t == "TXT": b = v.encode(); return bytes([len(b)]) + b
    raise ValueError(t)

def rdata_dec(t, msg, off, ln):
    if t == "A": return socket.inet_ntoa(msg[off:off + 4])
    if t == "AAAA": return socket.inet_ntop(socket.AF_INET6, msg[off:off + 16])
    if t in ("NS", "CNAME"): return dec_name(msg, off)[0]
    if t == "MX": return (struct.unpack("!H", msg[off:off + 2])[0], dec_name(msg, off + 2)[0])
    if t == "TXT": return msg[off + 1:off + 1 + msg[off]].decode()
    return msg[off:off + ln].hex()

def build_query(qid, name, qtype, rd=0):
    return struct.pack("!HHHHHH", qid, rd << 8, 1, 0, 0, 0) + enc_name(name) + struct.pack("!HH", TYPES[qtype], 1)

def build_response(qid, qname, qtype, rcode, aa, answers, authority, additional):
    flags = 0x8000 | (aa << 10) | rcode                         # QR=1, AA, RCODE
    msg = struct.pack("!HHHHHH", qid, flags, 1, len(answers), len(authority), len(additional)) + enc_name(qname) + struct.pack("!HH", TYPES[qtype], 1)
    for sec in (answers, authority, additional):
        for (n, t, ttl, v) in sec:
            rd = rdata_enc(t, v); msg += enc_name(n) + struct.pack("!HHIH", TYPES[t], 1, ttl, len(rd)) + rd
    return msg

def parse(msg):
    qid, flags, qd, an, ns, ar = struct.unpack("!HHHHHH", msg[:12]); off = 12
    qname, off = dec_name(msg, off); qtype = NAMES.get(struct.unpack("!H", msg[off:off + 2])[0], "?"); off += 4
    secs = []
    for count in (an, ns, ar):
        sec = []
        for _ in range(count):
            n, off = dec_name(msg, off); t, c, ttl, ln = struct.unpack("!HHIH", msg[off:off + 10]); off += 10
            tn = NAMES.get(t, str(t)); sec.append((n, tn, ttl, rdata_dec(tn, msg, off, ln))); off += ln
        secs.append(sec)
    return {"id": qid, "qr": flags >> 15, "aa": (flags >> 10) & 1, "tc": (flags >> 9) & 1, "rcode": RCODES.get(flags & 15, str(flags & 15)),
            "qname": qname, "qtype": qtype, "answers": secs[0], "authority": secs[1], "additional": secs[2]}
