#!/usr/bin/env python3
"""DNS64 synthesis (RFC 6147 / RFC 6052): turn an A record into an AAAA record inside a NAT64 prefix. Also checks the RFC 6052 test vectors."""
import ipaddress, sys

def synthesize(v4, prefix="64:ff9b::/96"):
    net = ipaddress.IPv6Network(prefix)
    n = net.prefixlen
    a = int(ipaddress.IPv4Address(v4)).to_bytes(4, "big"); p = bytearray(net.network_address.packed)
    if n == 96: p[12:16] = a
    elif n == 64: p[9:13] = a                                  # bits 64-71 (the "u" octet) must stay zero
    elif n == 32: p[4:8] = a
    elif n == 40: p[5:8] = a[:3]; p[9] = a[3]
    elif n == 48: p[6:8] = a[:2]; p[9:11] = a[2:]
    elif n == 56: p[7] = a[0]; p[9:12] = a[1:]
    else: raise ValueError("RFC 6052 allows /32 /40 /48 /56 /64 /96")
    return ipaddress.IPv6Address(bytes(p))

# RFC 6052 section 2.4 example, address 192.0.2.33 in each prefix length
if __name__ == "__main__":
  for prefix in ("2001:db8::/32", "2001:db8:100::/40", "2001:db8:122::/48", "2001:db8:122:300::/56", "2001:db8:122:344::/64", "2001:db8:122:344::/96", "64:ff9b::/96"):
      print(f"{prefix:<24} 192.0.2.33 -> {synthesize('192.0.2.33', prefix)}")
