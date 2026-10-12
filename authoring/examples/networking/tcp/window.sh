#!/bin/sh
# The receive window limits throughput to  window / RTT.  100 Mbit/s path, 40 ms round-trip time, 6 s flows, no loss.
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
ip netns add client; ip netns add server; ip link add vc type veth peer name vs; ip link set vc netns client; ip link set vs netns server
ip -n client addr add 10.0.0.1/24 dev vc; ip -n server addr add 10.0.0.2/24 dev vs
for x in "client vc" "server vs"; do set -- $x; ip -n $1 link set $2 up; ip -n $1 link set lo up; done
for ns_if in "client vc" "server vs"; do set -- $ns_if; ip netns exec $1 tc qdisc replace dev $2 root netem delay 20ms rate 100mbit limit 1000; done
echo "bandwidth-delay product = 100 Mbit/s x 0.040 s = 500000 bytes"
printf "%-26s %10s %22s\n" "client receive buffer" "Mbit/s" "window/RTT prediction"
for buf in 32768 131072 524288 ""; do
  ip netns exec server python3 transfer.py server cubic 6 > /dev/null &
  sleep 0.3
  rate=$(ip netns exec client python3 transfer.py client $buf); wait
  if [ -n "$buf" ]; then pred=$(python3 -c "print(f'{$buf*8/0.04/1e6:.0f}')"); lbl="$buf bytes (requested)"; else pred="(auto-tuned)"; lbl="default (auto-tuning)"; fi
  printf "%-26s %10s %22s\n" "$lbl" "$rate" "$pred"
done
echo; echo "ss -ti on the sender while a default flow runs (cwnd, rtt, mss, bytes in flight):"
ip netns exec server python3 transfer.py server cubic 4 > /dev/null &
sleep 0.3; ip netns exec client python3 transfer.py client > /dev/null &
sleep 2
ip netns exec server ss -ti dst 10.0.0.1 | python3 -c "
import sys
t = sys.stdin.read().split()
want = ('cubic', 'rto:', 'rtt:', 'mss:', 'cwnd:', 'ssthresh:', 'bytes_retrans:', 'retrans:')
print('  ' + ' '.join(x for x in t if x.startswith(want)))
print('  delivery_rate', t[t.index('delivery_rate') + 1], '  (the base round-trip time is 40 ms; the larger rtt is queueing in the 1000-packet buffer)')"
wait
