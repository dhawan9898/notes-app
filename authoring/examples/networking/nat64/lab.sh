#!/bin/sh
# c6 (IPv6-only client) -- n64 (NAT64 gateway, TUN device "nat64") -- s4 (IPv4-only server)
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
for n in c6 n64 s4; do ip netns add $n; ip -n $n link set lo up; done
ip link add vc type veth peer name vn1; ip link set vc netns c6; ip link set vn1 netns n64
ip link add vn2 type veth peer name vs; ip link set vn2 netns n64; ip link set vs netns s4
ip -n c6 addr add 2001:db8:6::2/64 dev vc nodad; ip -n n64 addr add 2001:db8:6::1/64 dev vn1 nodad
ip -n n64 addr add 198.51.100.1/24 dev vn2; ip -n s4 addr add 198.51.100.2/24 dev vs
for l in "c6 vc" "n64 vn1" "n64 vn2" "s4 vs"; do set -- $l; ip -n $1 link set $2 up; done
ip -n c6 -6 route add default via 2001:db8:6::1 dev vc
ip -n s4 route add default via 198.51.100.1            # replies to the NAT64 pool address go to the gateway
ip netns exec n64 sysctl -qw net.ipv4.ip_forward=1 net.ipv6.conf.all.forwarding=1 net.ipv4.conf.all.rp_filter=0 net.ipv4.conf.default.rp_filter=0
ip netns exec c6 sysctl -qw net.ipv6.conf.all.accept_ra=0
echo "== the IPv6-only client has no IPv4 at all =="
echo '$ ip -4 addr show   (client, interface vc)'; ip -n c6 -4 addr show dev vc | grep -c inet | sed 's/^/IPv4 addresses on the client interface: /'
echo; echo "== DNS64: how an IPv4 address is embedded for each prefix length (RFC 6052 section 2.4) =="
python3 dns64.py
echo; echo "== NAT64 gateway starts; pool address 203.0.113.64 =="
ip netns exec n64 python3 nat64.py 203.0.113.64 14 > /tmp/n64.$$ 2>&1 &
sleep 1
ip -n n64 link set nat64 up; ip -n n64 -6 route add 64:ff9b::/96 dev nat64; ip -n n64 route add 203.0.113.64/32 dev nat64
ip netns exec s4 python3 server.py 198.51.100.2 11 > /tmp/s4.$$ 2>&1 &
sleep 1
echo '$ ping -6 -c 3 64:ff9b::c633:6402   (c633:6402 = 198.51.100.2)'
ip netns exec c6 ping -6 -c 3 -W 2 64:ff9b::c633:6402 | grep -E "bytes from|transmitted"
echo; echo "== UDP and TCP through the translator =="
ip netns exec c6 python3 client.py 198.51.100.2
sleep 1; echo; echo "== what the IPv4 server saw =="; cat /tmp/s4.$$
wait
echo; echo "== NAT64 gateway log =="; cat /tmp/n64.$$
rm -f /tmp/n64.$$ /tmp/s4.$$
