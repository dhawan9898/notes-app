#!/bin/sh
# Builds two "computers" (network namespaces) joined by a virtual cable, as an unprivileged user inside a throwaway user+network+mount namespace.
set -e
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
ip netns add pc1; ip netns add pc2
ip link add v1 type veth peer name v2                 # a virtual Ethernet cable with a plug at each end
ip link set v1 netns pc1; ip link set v2 netns pc2
ip -n pc1 addr add 192.168.1.10/24 dev v1; ip -n pc2 addr add 192.168.1.20/24 dev v2
ip -n pc1 link set v1 up; ip -n pc2 link set v2 up; ip -n pc1 link set lo up
echo '$ ip -br addr        (on pc1)';              ip -n pc1 -br addr | grep -E "^(lo|v1)"
echo '$ ip route           (on pc1)';              ip -n pc1 route
echo '$ ip neigh           (on pc1, before any traffic)'; ip -n pc1 neigh
echo '$ ping -c 3 192.168.1.20   (pc1 -> pc2)';    ip netns exec pc1 ping -c 3 192.168.1.20 | grep -E "bytes from|transmitted|rtt"
echo '$ ip neigh           (on pc1, after the ping)'; ip -n pc1 neigh | sed 's/\(..:..:..:..:..:..\)/<mac>/'
echo '$ ip route get 8.8.8.8   (pc1 has no route to the Internet)'; ip -n pc1 route get 8.8.8.8 2>&1 || true
ip -n pc1 route add default via 192.168.1.20
echo '$ ip route add default via 192.168.1.20 ; ip route get 8.8.8.8'; ip -n pc1 route get 8.8.8.8 | head -1 | sed 's/ uid [0-9]*//'
