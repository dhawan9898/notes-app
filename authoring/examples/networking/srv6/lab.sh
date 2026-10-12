#!/bin/sh
# SRv6: h1 -- r1 -- r2 -- r3 -- h2.  r1 steers traffic to h2 through the SIDs of r2 (End) and r3 (End.DT6) using a Segment Routing Header.
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
for n in h1 r1 r2 r3 h2; do ip netns add $n; ip -n $n link set lo up; ip netns exec $n sysctl -qw net.ipv6.conf.all.forwarding=1 net.ipv6.conf.default.forwarding=1 net.ipv6.conf.all.seg6_enabled=1 net.ipv6.conf.default.seg6_enabled=1; done
link() { ip link add $1 type veth peer name $2; ip link set $1 netns $3; ip link set $2 netns $4; ip -n $3 addr add $5 dev $1 nodad; ip -n $4 addr add $6 dev $2 nodad; ip -n $3 link set $1 up; ip -n $4 link set $2 up; ip netns exec $3 sysctl -qw net.ipv6.conf.$1.seg6_enabled=1; ip netns exec $4 sysctl -qw net.ipv6.conf.$2.seg6_enabled=1; }
link a1 a2 h1 r1 2001:db8:1::2/64 2001:db8:1::1/64
link b1 b2 r1 r2 fc00:12::1/64 fc00:12::2/64
link c1 c2 r2 r3 fc00:23::2/64 fc00:23::3/64
link d1 d2 r3 h2 2001:db8:2::1/64 2001:db8:2::2/64
# ordinary IPv6 routing for the underlay and the customer prefixes
ip -n h1 -6 route add default via 2001:db8:1::1; ip -n h2 -6 route add default via 2001:db8:2::1
ip -n r1 -6 route add fc00:2::/64 via fc00:12::2; ip -n r1 -6 route add fc00:3::/64 via fc00:12::2
ip -n r2 -6 route add fc00:3::/64 via fc00:23::3; ip -n r2 -6 route add fc00:1::/64 via fc00:12::1
ip -n r3 -6 route add fc00:1::/64 via fc00:23::2; ip -n r3 -6 route add fc00:2::/64 via fc00:23::2
ip -n r2 -6 route add 2001:db8:1::/64 via fc00:12::1; ip -n r3 -6 route add 2001:db8:1::/64 via fc00:23::2
# SIDs: r2 runs the plain "End" behavior (advance to the next segment); r3 runs End.DT6 (decapsulate and look the inner destination up in the main table)
ip -n r2 -6 route add fc00:2::1/128 encap seg6local action End dev c1
ip -n r3 -6 route add fc00:3::d6/128 encap seg6local action End.DT6 table main dev d1
# r1 steers the customer prefix of h2 into an SRv6 tunnel with a two-segment list
ip -n r1 -6 route add 2001:db8:2::/64 encap seg6 mode encap segs fc00:2::1,fc00:3::d6 dev b1
echo '$ ip -6 route show 2001:db8:2::/64   (on r1)'; ip -n r1 -6 route show 2001:db8:2::/64
echo '$ ip -6 route show fc00:2::1   (on r2)';  ip -n r2 -6 route show | grep "fc00:2::1 "
echo '$ ip -6 route show fc00:3::d6   (on r3)'; ip -n r3 -6 route show | grep d6
ip netns exec h1 ping -6 -c 1 -W 5 2001:db8:2::2 > /dev/null 2>&1        # warm-up: lets IPv6 neighbor discovery finish on every link
echo; echo '$ ping -6 -c 1 2001:db8:2::2   (h1 -> h2), captured on all three core links at once'
ip netns exec r1 python3 sniff6.py b1 1 > /tmp/cap1.$$ &
ip netns exec r2 python3 sniff6.py c1 1 > /tmp/cap2.$$ &
ip netns exec r3 python3 sniff6.py d1 1 > /tmp/cap3.$$ &
sleep 0.7
ip netns exec h1 ping -6 -c 1 -W 3 2001:db8:2::2 | grep -E "bytes from|transmitted"
wait
echo; echo "r1 sends on the r1--r2 link (SR source node: encapsulates):"; cat /tmp/cap1.$$
echo; echo "r2 sends on the r2--r3 link (after its End behavior):"; cat /tmp/cap2.$$
echo; echo "r3 sends on the r3--h2 link (after End.DT6 decapsulation: a plain IPv6 packet again):"; cat /tmp/cap3.$$
rm -f /tmp/cap1.$$ /tmp/cap2.$$ /tmp/cap3.$$
