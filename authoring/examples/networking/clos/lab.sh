#!/bin/sh
# Leaf-spine Clos: hosts h1,h2 behind leaves l1,l2; spines s1,s2 (every leaf connects to every spine).
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
for n in h1 h2 l1 l2 s1 s2; do ip netns add $n; ip -n $n link set lo up; ip netns exec $n sysctl -qw net.ipv4.ip_forward=1; done
link() { ip link add $1 type veth peer name $2; ip link set $1 netns $3; ip link set $2 netns $4; ip -n $3 addr add $5 dev $1; ip -n $4 addr add $6 dev $2; ip -n $3 link set $1 up; ip -n $4 link set $2 up; }
link h1e l1h h1 l1 10.1.0.2/24 10.1.0.1/24
link h2e l2h h2 l2 10.2.0.2/24 10.2.0.1/24
link l1s1 s1l1 l1 s1 10.0.11.1/30 10.0.11.2/30
link l1s2 s2l1 l1 s2 10.0.12.1/30 10.0.12.2/30
link l2s1 s1l2 l2 s1 10.0.21.1/30 10.0.21.2/30
link l2s2 s2l2 l2 s2 10.0.22.1/30 10.0.22.2/30
ip -n h1 route add default via 10.1.0.1; ip -n h2 route add default via 10.2.0.1
# each leaf has an equal-cost route to the other leaf's subnet through both spines (what BGP/OSPF ECMP would install)
ip -n l1 route add 10.2.0.0/24 nexthop via 10.0.11.2 dev l1s1 nexthop via 10.0.12.2 dev l1s2
ip -n l2 route add 10.1.0.0/24 nexthop via 10.0.21.2 dev l2s1 nexthop via 10.0.22.2 dev l2s2
ip -n s1 route add 10.2.0.0/24 via 10.0.21.1; ip -n s1 route add 10.1.0.0/24 via 10.0.11.1
ip -n s2 route add 10.2.0.0/24 via 10.0.22.1; ip -n s2 route add 10.1.0.0/24 via 10.0.12.1
echo '$ ip route show 10.2.0.0/24   (on leaf l1)'; ip -n l1 route show 10.2.0.0/24
ip netns exec h1 ping -c 1 -W 3 10.2.0.2 | grep -E "transmitted"

# send N distinct UDP flows (different source ports) from h1 to h2 and count packets leaving l1 on each uplink
flows() { ip netns exec h1 python3 -c "
import socket,sys
n=int(sys.argv[1]); mode=sys.argv[2]
for i in range(n):
    s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    if mode=='many': s.bind(('',20000+i))      # one packet from each of n different source ports
    else: s.bind(('',20000))                   # n packets all from ONE flow (same 5-tuple)
    s.sendto(b'x',('10.2.0.2',9)); s.close()
" $1 $2; }
count() { for i in l1s1 l1s2; do ip -n l1 -s -j link show $i | python3 -c "import json,sys; print(json.load(sys.stdin)[0]['stats64']['tx']['packets'], end=' ')"; done; echo; }
delta() { a=$(count); eval "$1"; b=$(count); python3 -c "
a='$a'.split(); b='$b'.split(); d=[int(y)-int(x) for x,y in zip(a,b)]
print('packets via spine1 / spine2: %d / %d' % tuple(d))"; }

for pol in 0 1; do
  ip netns exec l1 sysctl -qw net.ipv4.fib_multipath_hash_policy=$pol
  case $pol in 0) name="L3 (src/dst IP only)";; 1) name="L4 (5-tuple)";; esac
  echo; echo "== fib_multipath_hash_policy=$pol: hash on $name =="
  echo -n "200 flows, different source ports:  "; delta "flows 200 many"
  echo -n "200 packets, one single flow:       "; delta "flows 200 one"
done

echo; echo "== link failure: l1's link to spine1 goes down =="
ip -n l1 link set l1s1 down
echo '$ ip route show 10.2.0.0/24   (on l1)'; ip -n l1 route show 10.2.0.0/24
ip netns exec l1 sysctl -qw net.ipv4.fib_multipath_hash_policy=1
echo -n "200 flows, different source ports:  "; delta "flows 200 many"
