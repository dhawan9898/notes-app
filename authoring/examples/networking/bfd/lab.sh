#!/bin/sh
# Two routers r1 and r2 on one link, each running bfd.py. Scenarios: bring-up, link failure, timer negotiation, TTL check.
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
ip netns add r1; ip netns add r2
ip link add v1 type veth peer name v2; ip link set v1 netns r1; ip link set v2 netns r2
ip -n r1 addr add 192.0.2.1/24 dev v1; ip -n r2 addr add 192.0.2.2/24 dev v2
ip -n r1 link set v1 up; ip -n r2 link set v2 up; ip -n r1 link set lo up; ip -n r2 link set lo up
run() { ns=$1; shift; ip netns exec $ns python3 bfd.py "$@"; }

echo "== 1. bring-up: both sides 100 ms x 3 =="
run r1 192.0.2.1 192.0.2.2 11 100 100 3 5 > /tmp/r1.$$ & run r2 192.0.2.2 192.0.2.1 22 100 100 3 5 > /tmp/r2.$$ & wait
echo "router r1:"; cat /tmp/r1.$$; echo "router r2:"; cat /tmp/r2.$$

echo; echo "== 2. failure: from t=4 s the link silently drops all packets (carrier stays up, like a broken switch in the path) =="
(sleep 4; ip netns exec r1 tc qdisc add dev v1 root netem loss 100%; ip netns exec r2 tc qdisc add dev v2 root netem loss 100%) &
run r1 192.0.2.1 192.0.2.2 11 100 100 3 7 > /tmp/r1.$$ & run r2 192.0.2.2 192.0.2.1 22 100 100 3 7 > /tmp/r2.$$ & wait
echo "router r1 (the loss starts at about 4000 ms):"; cat /tmp/r1.$$
ip netns exec r1 tc qdisc del dev v1 root; ip netns exec r2 tc qdisc del dev v2 root

echo; echo "== 3. timer negotiation: r1 wants 50 ms x 3, r2 can only receive every 400 ms and wants 200 ms x 5 =="
run r1 192.0.2.1 192.0.2.2 11 50 50 3 5 > /tmp/r1.$$ & run r2 192.0.2.2 192.0.2.1 22 200 400 5 5 > /tmp/r2.$$ & wait
echo "router r1:"; grep -E "negotiated|->" /tmp/r1.$$; echo "router r2:"; grep -E "negotiated|->" /tmp/r2.$$

echo; echo "== 4. TTL check: r2 sends with TTL 64 (as a spoofer several hops away would) =="
run r1 192.0.2.1 192.0.2.2 11 100 100 3 4 > /tmp/r1.$$ & run r2 192.0.2.2 192.0.2.1 22 100 100 3 4 --ttl 64 > /tmp/r2.$$ & wait
echo "router r1:"; cat /tmp/r1.$$
rm -f /tmp/r1.$$ /tmp/r2.$$
