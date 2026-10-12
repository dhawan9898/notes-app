import ipaddress as ip

print("Ex 1: 192.168.10.77 in binary:", ".".join(f"{int(o):08b}" for o in "192.168.10.77".split(".")))
print("Ex 2: 172.20.5.9/20 ->", ip.ip_interface("172.20.5.9/20").network, "| mask", ip.ip_network("172.20.5.9/20", strict=False).netmask, "| usable hosts", ip.ip_network("172.20.5.9/20", strict=False).num_addresses - 2)
n = ip.ip_network("10.20.77.130/21", strict=False)
print("Ex 3: 10.20.77.130/21 -> network", n.network_address, "broadcast", n.broadcast_address, "range", n.network_address + 1, "-", n.broadcast_address - 1)
for a, b in [("192.168.1.10/24", "192.168.1.200/24"), ("10.0.0.5/25", "10.0.0.130/25"), ("172.16.4.1/22", "172.16.7.250/22")]:
    na, nb = ip.ip_interface(a).network, ip.ip_interface(b).network
    print(f"Ex 4: {a} and {b}: same network? {na == nb} ({na} vs {nb})")
def prefix_for(hosts):                      # smallest block with room for the hosts plus network and broadcast addresses
    bits = (hosts + 2 - 1).bit_length()
    return 32 - bits, 2 ** bits - 2
for h in (25, 100, 500):
    p, usable = prefix_for(h)
    print(f"Ex 5: {h} hosts need a /{p} ({usable} usable addresses)")
print("Ex 6: split 192.168.5.0/24 into four equal subnets:", [str(s) for s in ip.ip_network("192.168.5.0/24").subnets(new_prefix=26)])
for a in ["10.1.2.3", "172.32.0.1", "192.168.255.1", "169.254.7.7", "100.64.0.9", "8.8.8.8"]:
    x = ip.ip_address(a); kind = "private (RFC 1918)" if x in ip.ip_network("10.0.0.0/8") or x in ip.ip_network("172.16.0.0/12") or x in ip.ip_network("192.168.0.0/16") else "link-local (DHCP failed?)" if x.is_link_local else "shared/CGNAT space" if x in ip.ip_network("100.64.0.0/10") else "public"
    print(f"Ex 7: {a:15} {kind}")
print("Ex 8: aggregate 10.1.0.0/24 + 10.1.1.0/24 + 10.1.2.0/24 + 10.1.3.0/24 ->", list(ip.collapse_addresses([ip.ip_network(f"10.1.{i}.0/24") for i in range(4)])))
