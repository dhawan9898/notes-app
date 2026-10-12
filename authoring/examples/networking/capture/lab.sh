#!/bin/sh
# Capture a TCP transfer at the sender over a lossy 20 ms path, write a pcap, then analyse it with a script.
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
ip netns add client; ip netns add server; ip link add vc type veth peer name vs; ip link set vc netns client; ip link set vs netns server
ip -n client addr add 10.0.0.1/24 dev vc; ip -n server addr add 10.0.0.2/24 dev vs
for x in "client vc" "server vs"; do set -- $x; ip -n $1 link set $2 up; ip -n $1 link set lo up; ip netns exec $1 tc qdisc add dev $2 root netem delay 10ms loss 1% rate 50mbit limit 1000; done
ip netns exec server python3 -c '
import socket, time
l = socket.socket(); l.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); l.bind(("10.0.0.2", 5001)); l.listen(1)
c, _ = l.accept(); end = time.time() + 5
while time.time() < end: c.sendall(b"x" * 65536)
c.close()' &
sleep 0.5
ip netns exec server python3 capture.py vs capture.pcap 7 &
sleep 0.5
ip netns exec client python3 -c '
import socket
s = socket.create_connection(("10.0.0.2", 5001)); n = 0
while True:
    d = s.recv(1 << 20)
    if not d: break
    n += len(d)
print("client received", n, "bytes")'
wait
python3 analyze.py capture.pcap
echo; echo "file: $(ls -l capture.pcap | awk '{print $5}') bytes; first 4 bytes (magic number, little-endian a1b2c3d4): $(head -c 4 capture.pcap | od -An -tx1)"
