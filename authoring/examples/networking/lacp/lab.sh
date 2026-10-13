#!/bin/sh
# Two "switches" (namespaces a and b) joined by two veth links, each side running a Linux 802.3ad (LACP) bond.
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
ip netns add a; ip netns add b
for i in 1 2; do ip link add a$i type veth peer name b$i; ip link set a$i netns a; ip link set b$i netns b; done
mkbond() { # ns policy
    ip -n $1 link add bond0 type bond mode 802.3ad lacp_rate fast xmit_hash_policy $2 miimon 100
    for i in 1 2; do ip -n $1 link set $1$i down; ip -n $1 link set $1$i master bond0; done
    ip -n $1 link set bond0 up
}
mkbond a layer3+4; mkbond b layer3+4
ip -n a addr add 10.0.0.1/24 dev bond0; ip -n b addr add 10.0.0.2/24 dev bond0
sleep 4
echo '== negotiated state, as seen by switch a =='
ip netns exec a awk '/^Slave Interface/{s=$3} /MII Status/{if(s!="")print s ": link " $3} /Aggregator ID/{print s ": aggregator ID " $3}' /proc/net/bonding/bond0
echo; echo '== the LACPDU that switch a sends on a1 =='
ip netns exec a python3 lacpdu.py a1 4

echo; echo '== distribution: 64 UDP flows a -> b with different source ports, packets sent on each member =='
flows() { ip netns exec a python3 -c "
import socket
for i in range(64):
    s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.bind(('',30000+i)); s.sendto(b'x',('10.0.0.2',9)); s.close()"; }
txp() { for i in 1 2; do ip -n a -s -j link show a$i | python3 -c "import json,sys; print(json.load(sys.stdin)[0]['stats64']['tx']['packets'], end=' ')"; done; echo; }
ip netns exec a ping -c 1 -W 2 10.0.0.2 > /dev/null          # warm-up: ARP
for pol in layer2 layer2+3 layer3+4; do
    ip -n a link set bond0 down; ip -n a link set bond0 type bond xmit_hash_policy $pol; ip -n a link set bond0 up; sleep 3
    before=$(txp); flows; after=$(txp)
    python3 -c "b='$before'.split(); a='$after'.split(); print('%-10s a1: %d  a2: %d' % ('$pol', int(a[0])-int(b[0]), int(a[1])-int(b[1])))"
done

echo; echo '== failure: link a1 goes down while pinging =='
ip -n a link set bond0 down; ip -n a link set bond0 type bond xmit_hash_policy layer3+4; ip -n a link set bond0 up; sleep 3
ip netns exec a ping -c 20 -i 0.1 -W 1 10.0.0.2 > /tmp/ping.$$ &
sleep 0.5; ip -n a link set a1 down; wait
grep -E "transmitted" /tmp/ping.$$; rm -f /tmp/ping.$$
sleep 2
ip netns exec a awk '/^Slave Interface/{s=$3} /MII Status/{if(s!="")print s ": link " $3}' /proc/net/bonding/bond0
echo -n "flows after failure:  a1/a2 packets sent: "; before=$(txp); flows; after=$(txp)
python3 -c "b='$before'.split(); a='$after'.split(); print('%d / %d' % (int(a[0])-int(b[0]), int(a[1])-int(b[1])))"
ip -n a link set a1 up; sleep 4
echo -n "after recovery: "; ip netns exec a awk '/Aggregator ID/{printf "%s ", $3} END{print "(aggregator IDs of the two members)"}' /proc/net/bonding/bond0

echo; echo '== misconfiguration: switch c runs LACP, switch d has a static bundle (balance-xor, no LACP) =='
ip netns add c; ip netns add d
for i in 1 2; do ip link add c$i type veth peer name d$i; ip link set c$i netns c; ip link set d$i netns d; done
mk2() { ip -n $1 link add bond0 type bond mode $2 miimon 100 $3; for i in 1 2; do ip -n $1 link set $1$i down; ip -n $1 link set $1$i master bond0; done; ip -n $1 link set bond0 up; }
mk2 c 802.3ad "lacp_rate fast"; mk2 d balance-xor ""
ip -n c addr add 10.0.1.1/24 dev bond0; ip -n d addr add 10.0.1.2/24 dev bond0; sleep 6
ip netns exec c awk '/^Slave Interface/{s=$3} /Aggregator ID/{print s ": aggregator ID " $3}' /proc/net/bonding/bond0
echo "two different IDs: LACP never saw a partner, so each port is its own aggregator and only one carries traffic"
ip netns exec c ping -c 2 -W 1 10.0.1.2 | grep transmitted
