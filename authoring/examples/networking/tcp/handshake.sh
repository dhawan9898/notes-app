#!/bin/sh
# Two hosts joined by a veth pair. Capture a complete TCP connection: handshake, a request/response, and teardown.
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
ip netns add client; ip netns add server; ip link add vc type veth peer name vs; ip link set vc netns client; ip link set vs netns server
ip -n client addr add 10.0.0.1/24 dev vc; ip -n server addr add 10.0.0.2/24 dev vs
for x in "client vc" "server vs"; do set -- $x; ip -n $1 link set $2 up; ip -n $1 link set lo up; done
ip netns exec server tc qdisc add dev vs root netem delay 10ms             # 10 ms on the server -> client direction, so the round-trip time is about 10 ms
ip netns exec server python3 -c '
import socket
l = socket.socket(); l.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); l.bind(("10.0.0.2", 5000)); l.listen(5)
c, _ = l.accept(); req = c.recv(100); c.sendall(b"HTTP/1.0 200 OK\r\n\r\nhello"); c.close()' &
sleep 0.5
ip netns exec client python3 sniff.py vc 5000 12 &
sleep 0.5
ip netns exec client python3 -c '
import socket
s = socket.create_connection(("10.0.0.2", 5000)); s.sendall(b"GET / HTTP/1.0\r\n\r\n"); print("client got:", s.recv(100)[-5:]); s.close()'
wait
