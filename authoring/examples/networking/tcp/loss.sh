#!/bin/sh
# Throughput of one TCP flow vs random packet loss, for two congestion control algorithms.
# Path: 100 Mbit/s, 20 ms round-trip time (10 ms each way); each flow runs for 8 seconds; every setting is run twice.
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
ip netns add client; ip netns add server; ip link add vc type veth peer name vs; ip link set vc netns client; ip link set vs netns server
ip -n client addr add 10.0.0.1/24 dev vc; ip -n server addr add 10.0.0.2/24 dev vs
for x in "client vc" "server vs"; do set -- $x; ip -n $1 link set $2 up; ip -n $1 link set lo up; done
SECS=8
printf "%-8s %-6s %22s %22s\n" "algo" "loss" "Mbit/s (run 1, run 2)" "retransmits (1, 2)"
for cc in reno cubic; do
  for loss in 0% 0.1% 1% 2%; do
    for ns_if in "client vc" "server vs"; do set -- $ns_if; ip netns exec $1 tc qdisc replace dev $2 root netem delay 10ms loss $loss rate 100mbit limit 1000; done
    out=""; rx=""
    for run in 1 2; do
      ip netns exec server python3 transfer.py server $cc $SECS > /tmp/retx.$$ &
      sleep 0.3
      rate=$(ip netns exec client python3 transfer.py client); wait
      out="$out $rate"; rx="$rx $(cat /tmp/retx.$$)"
    done
    printf "%-8s %-6s %22s %22s\n" $cc $loss "$out" "$rx"
  done
done
rm -f /tmp/retx.$$
