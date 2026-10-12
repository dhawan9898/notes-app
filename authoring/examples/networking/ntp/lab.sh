#!/bin/sh
# Toy NTP between two namespaces. The client clock is 250 ms ahead, so the correct offset is -250 ms.
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
ip netns add client; ip netns add server; ip link add vc type veth peer name vs; ip link set vc netns client; ip link set vs netns server
ip -n client addr add 10.0.0.1/24 dev vc; ip -n server addr add 10.0.0.2/24 dev vs
for x in "client vc" "server vs"; do set -- $x; ip -n $1 link set $2 up; ip -n $1 link set lo up; done
ip netns exec server python3 ntp_toy.py server &
SPID=$!; sleep 0.5
run() { echo "$1"; ip netns exec client tc qdisc replace dev vc root netem $2; ip netns exec server tc qdisc replace dev vs root netem $3; ip netns exec client python3 ntp_toy.py client ${4:-1}; }
run "1) symmetric path: 10 ms each way"                         "delay 10ms" "delay 10ms"
run "2) asymmetric path: 2 ms to the server, 22 ms back"        "delay 2ms"  "delay 22ms"
run "3) symmetric but jittery: 10 ms +/- 8 ms each way, one sample"   "delay 10ms 8ms" "delay 10ms 8ms" 1
run "4) the same jittery path, best of 16 samples (lowest delay)"     "delay 10ms 8ms" "delay 10ms 8ms" 16
kill $SPID
