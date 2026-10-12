#!/bin/sh
# Client and server namespaces, 40 ms round-trip time (20 ms each way), then time TCP connect, the TLS handshake and the first response byte.
sh certs.sh > /dev/null
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
ip netns add client; ip netns add server; ip link add vc type veth peer name vs; ip link set vc netns client; ip link set vs netns server
ip -n client addr add 10.0.0.1/24 dev vc; ip -n server addr add 10.0.0.2/24 dev vs
for x in "client vc" "server vs"; do set -- $x; ip -n $1 link set $2 up; ip -n $1 link set lo up; ip netns exec $1 tc qdisc add dev $2 root netem delay 20ms; done
ip netns exec server python3 tls_lab.py server &
SPID=$!; sleep 0.7
ip netns exec client python3 tls_lab.py client
echo; echo "Same test with TCP_NODELAY off (Nagle's algorithm enabled on the client):"
NODELAY=0 ip netns exec client python3 tls_lab.py client | sed -n '1p;4p'
kill $SPID 2>/dev/null
