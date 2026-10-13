#!/usr/bin/env python3
"""A small stateful NAT64 (RFC 6146 / RFC 7915 / RFC 6052) on a Linux TUN device, for TCP, UDP and ICMP echo.
usage: nat64.py POOL_V4 RUN_SECONDS      (prefix is the well-known 64:ff9b::/96)
Not handled on purpose: fragments, ICMP errors, TCP state tracking, port-range limits, hairpinning."""
import fcntl, ipaddress, os, select, socket, struct, sys, time

POOL = ipaddress.IPv4Address(sys.argv[1]).packed
PREFIX = ipaddress.IPv6Address("64:ff9b::").packed[:12]
fd = os.open("/dev/net/tun", os.O_RDWR)
fcntl.ioctl(fd, 0x400454ca, struct.pack("16sH", b"nat64", 0x0001 | 0x1000))          # TUNSETIFF: IFF_TUN | IFF_NO_PI
t0 = time.monotonic()
out_map, in_map, next_port, stats = {}, {}, 40000, {"6to4": 0, "4to6": 0, "no mapping": 0}

def csum(data):
    if len(data) % 2: data += b"\0"
    s = sum(struct.unpack("!%dH" % (len(data) // 2), data))
    while s >> 16: s = (s & 0xffff) + (s >> 16)
    return ~s & 0xffff

def l4_checksum(src, dst, proto, seg, v6):
    pseudo = src + dst + (struct.pack("!IBBBB", len(seg), 0, 0, 0, proto) if v6 else struct.pack("!BBH", 0, proto, len(seg)))
    return csum(pseudo + seg)

def ipv4_header(src, dst, proto, payload_len, ttl):
    h = struct.pack("!BBHHHBBH4s4s", 0x45, 0, 20 + payload_len, 0, 0x4000, ttl, proto, 0, src, dst)     # DF set, no fragmentation
    return h[:10] + struct.pack("!H", csum(h)) + h[12:]

def ipv6_header(src, dst, proto, payload_len, hop):
    return struct.pack("!IHBB16s16s", 0x60000000, payload_len, proto, hop, src, dst)

def log(msg): print(f"  [{(time.monotonic() - t0) * 1000:6.0f} ms] {msg}", flush=True)

def from_v6(pkt):
    global next_port
    plen, nh, hop = struct.unpack("!HBB", pkt[4:8]); src, dst = pkt[8:24], pkt[24:40]; body = pkt[40:40 + plen]
    if dst[:12] != PREFIX or hop <= 1: return None
    v4dst = dst[12:]                                                              # RFC 6052: the IPv4 address sits in the low 32 bits of the /96
    if nh in (6, 17):
        sport, dport = struct.unpack("!HH", body[:4]); key = (src, nh, sport)
        if key not in out_map:
            out_map[key] = next_port; in_map[(nh, next_port)] = (src, sport); next_port += 1
            log(f"new mapping: [{ipaddress.IPv6Address(src)}]:{sport} <-> {ipaddress.IPv4Address(POOL)}:{out_map[key]}  ({'TCP' if nh == 6 else 'UDP'}, dest {ipaddress.IPv4Address(v4dst)})")
        seg = bytearray(body); seg[0:2] = struct.pack("!H", out_map[key])
        ck = 16 if nh == 6 else 6
        seg[ck:ck + 2] = b"\0\0"; seg[ck:ck + 2] = struct.pack("!H", l4_checksum(POOL, v4dst, nh, bytes(seg), False) or (0xffff if nh == 17 else 0))
        return ipv4_header(POOL, v4dst, nh, len(seg), hop - 1) + bytes(seg)
    if nh == 58 and body[0] == 128:                                               # ICMPv6 echo request -> ICMP echo request
        ident = struct.unpack("!H", body[4:6])[0]; key = (src, 1, ident)
        if key not in out_map:
            out_map[key] = next_port; in_map[(1, next_port)] = (src, ident); next_port += 1
            log(f"new mapping: ICMP echo id {ident} from [{ipaddress.IPv6Address(src)}] <-> id {out_map[key]} to {ipaddress.IPv4Address(v4dst)}")
        msg = bytearray(body); msg[0] = 8; msg[4:6] = struct.pack("!H", out_map[key]); msg[2:4] = b"\0\0"; msg[2:4] = struct.pack("!H", csum(bytes(msg)))
        return ipv4_header(POOL, v4dst, 1, len(msg), hop - 1) + bytes(msg)
    return None

def from_v4(pkt):
    ihl = (pkt[0] & 15) * 4; proto = pkt[9]; src, dst = pkt[12:16], pkt[16:20]; body = pkt[ihl:struct.unpack("!H", pkt[2:4])[0]]
    if dst != POOL: return None
    v6src = PREFIX + src                                                          # the IPv4 server appears as 64:ff9b::a.b.c.d
    if proto in (6, 17):
        dport = struct.unpack("!H", body[2:4])[0]
        if (proto, dport) not in in_map: stats["no mapping"] += 1; return None
        v6dst, cport = in_map[(proto, dport)]
        seg = bytearray(body); seg[2:4] = struct.pack("!H", cport); ck = 16 if proto == 6 else 6
        seg[ck:ck + 2] = b"\0\0"; seg[ck:ck + 2] = struct.pack("!H", l4_checksum(v6src, v6dst, proto, bytes(seg), True) or (0xffff if proto == 17 else 0))
        return ipv6_header(v6src, v6dst, proto, len(seg), pkt[8] - 1) + bytes(seg)
    if proto == 1 and body[0] == 0:                                               # ICMP echo reply -> ICMPv6 echo reply
        ident = struct.unpack("!H", body[4:6])[0]
        if (1, ident) not in in_map: stats["no mapping"] += 1; return None
        v6dst, cid = in_map[(1, ident)]
        msg = bytearray(body); msg[0] = 129; msg[4:6] = struct.pack("!H", cid); msg[2:4] = b"\0\0"
        msg[2:4] = struct.pack("!H", l4_checksum(v6src, v6dst, 58, bytes(msg), True))
        return ipv6_header(v6src, v6dst, 58, len(msg), pkt[8] - 1) + bytes(msg)
    return None

log(f"NAT64 up: prefix 64:ff9b::/96, IPv4 pool address {sys.argv[1]}")
while time.monotonic() - t0 < float(sys.argv[2]):
    if not select.select([fd], [], [], 0.1)[0]: continue
    pkt = os.read(fd, 65535)
    if pkt[0] >> 4 == 6: res = from_v6(pkt); stats["6to4"] += res is not None
    elif pkt[0] >> 4 == 4: res = from_v4(pkt); stats["4to6"] += res is not None
    else: res = None
    if res: os.write(fd, res)
log(f"done: {stats['6to4']} packets translated IPv6->IPv4, {stats['4to6']} IPv4->IPv6, {stats['no mapping']} IPv4 packets without a mapping")
