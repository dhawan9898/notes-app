#!/bin/sh
# Two "nodes", each with a bridge (cni0) and two pods. Pod CIDRs are routed between nodes (host-gw style, like Flannel host-gw / Calico without encapsulation).
# A Service 10.96.0.10:80 (ClusterIP) is implemented the way kube-proxy does it: DNAT in the nat table, chosen at random per new connection.
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
for n in node1 node2 client p11 p12 p21 p22; do ip netns add $n; ip -n $n link set lo up; done
for n in node1 node2; do ip netns exec $n sysctl -qw net.ipv4.ip_forward=1; done
# node-to-node link
ip link add n1 type veth peer name n2; ip link set n1 netns node1; ip link set n2 netns node2
ip -n node1 addr add 192.168.0.1/24 dev n1; ip -n node2 addr add 192.168.0.2/24 dev n2; ip -n node1 link set n1 up; ip -n node2 link set n2 up
mknode() { # node podcidr-prefix pod_a pod_b
    ip -n $1 link add cni0 type bridge; ip -n $1 addr add 10.244.$2.1/24 dev cni0; ip -n $1 link set cni0 up
    i=10; for pod in $3 $4; do i=$((i+1))
        ip link add h$pod type veth peer name eth0; ip link set eth0 netns $pod; ip link set h$pod netns $1
        ip -n $1 link set h$pod master cni0; ip -n $1 link set h$pod up
        ip -n $pod addr add 10.244.$2.$i/24 dev eth0; ip -n $pod link set eth0 up; ip -n $pod route add default via 10.244.$2.1
    done
}
mknode node1 1 p11 p12; mknode node2 2 p21 p22
ip -n node1 route add 10.244.2.0/24 via 192.168.0.2; ip -n node2 route add 10.244.1.0/24 via 192.168.0.1     # the "CNI": pod CIDR routes between nodes
echo "== pod to pod, different nodes =="; ip netns exec p11 ping -c 1 -W 2 10.244.2.11 | grep -E "bytes from"
# the traffic leaves node1 with the pod's own address (no NAT):
echo; echo "== the service: pods p12, p21, p22 run an HTTP-like responder that returns its own name =="
cat > /tmp/resp.py <<'PY'
import socket, sys
name, ip = sys.argv[1], sys.argv[2]
s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind((ip, 80)); s.listen(50)
s.settimeout(12)
try:
    while True:
        c, a = s.accept(); c.recv(100); c.sendall(f"{name} (saw client {a[0]})\n".encode()); c.close()
except socket.timeout: pass
PY
for p in "p12 10.244.1.12" "p21 10.244.2.11" "p22 10.244.2.12"; do set -- $p; ip netns exec $1 python3 /tmp/resp.py $1 $2 & done
sleep 1
# kube-proxy (nftables mode, simplified): in the nat table, a service chain picks one endpoint per new connection at random
nat() { cat <<NFT
table ip kube {
  chain services {
    type nat hook prerouting priority dstnat; policy accept;
    ip daddr 10.96.0.10 tcp dport 80 jump svc_web
  }
  chain output_services {
    type nat hook output priority -100; policy accept;
    ip daddr 10.96.0.10 tcp dport 80 jump svc_web
  }
  chain svc_web {
    numgen random mod 3 vmap { 0 : goto ep1, 1 : goto ep2, 2 : goto ep3 }
  }
  chain ep1 { meta l4proto tcp dnat to 10.244.1.12:80; }
  chain ep2 { meta l4proto tcp dnat to 10.244.2.11:80; }
  chain ep3 { meta l4proto tcp dnat to 10.244.2.12:80; }
  chain post {
    type nat hook postrouting priority srcnat; policy accept;
    ct status dnat ip saddr 10.244.1.0/24 ip daddr 10.244.1.0/24 masquerade    # endpoint on the client's own node: force the reply back through the node
  }
}
NFT
}
# the client pod p11 reaches the ClusterIP; its default route is its node (node1), where the DNAT rules live
nat | ip netns exec node1 nft -f -
echo '$ nft list chain ip kube svc_web   (on node1)'; ip netns exec node1 nft list chain ip kube svc_web
echo; echo "== 30 new connections from pod p11 to the Service IP 10.96.0.10:80 =="
for i in $(seq 1 30); do ip netns exec p11 python3 -c "
import socket; s=socket.socket(); s.settimeout(3); s.connect(('10.96.0.10',80)); s.sendall(b'GET /'); print(s.recv(100).decode().split()[0])"; done | sort | uniq -c
echo; echo "== one connection, conntrack view: the client talks to the VIP, the endpoint answers it =="
ip netns exec p11 python3 -c "
import socket; s=socket.socket(); s.connect(('10.96.0.10',80)); print('connected to', s.getpeername()); s.sendall(b'GET /'); print('reply:', s.recv(100).decode().strip())"
echo; echo "== endpoint p22 removed from the Service (as when its pod is deleted or fails readiness): new connections avoid it =="
ip netns exec node1 nft flush chain ip kube svc_web
ip netns exec node1 nft add rule ip kube svc_web numgen random mod 2 vmap '{ 0 : goto ep1, 1 : goto ep2 }'
for i in $(seq 1 30); do ip netns exec p11 python3 -c "
import socket; s=socket.socket(); s.settimeout(3); s.connect(('10.96.0.10',80)); s.sendall(b'GET /'); print(s.recv(100).decode().split()[0])"; done | sort | uniq -c
kill $(jobs -p) 2>/dev/null; wait 2>/dev/null; rm -f /tmp/resp.py
