#!/bin/sh
# An SNMP agent (snmpd) in one namespace polled by a manager in another, over a virtual cable. Needs net-snmp (snmpd, snmpget, snmpwalk).
mount -t tmpfs tmpfs /run; mkdir -p /run/netns /tmp/snmp-lab
ip netns add nms; ip netns add agent; ip link add vn type veth peer name va; ip link set vn netns nms; ip link set va netns agent
ip -n nms addr add 10.0.0.1/24 dev vn; ip -n agent addr add 10.0.0.2/24 dev va
for x in "nms vn" "agent va"; do set -- $x; ip -n $1 link set $2 up; ip -n $1 link set lo up; done
SNMP_PERSISTENT_DIR=/tmp/snmp-lab ip netns exec agent snmpd -f -C -c snmpd.conf -Lf /tmp/snmp-lab/log -p /tmp/snmp-lab/pid > /dev/null 2>&1 &
SP=$!
ip netns exec agent python3 -c '
import socket
l = socket.socket(); l.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); l.bind(("10.0.0.2", 9000)); l.listen(1)
c, _ = l.accept()
while c.recv(1 << 20): pass' &
sleep 2
echo "== a v2c get with the community string, and a v3 get with a wrong password =="
ip netns exec nms snmpget -v2c -c demo -On 10.0.0.2 1.3.6.1.2.1.1.5.0
ip netns exec nms snmpget -v3 -l authNoPriv -u nmsuser -a SHA -A wrongpassword 10.0.0.2 1.3.6.1.2.1.1.5.0 2>&1 | head -1
ip netns exec nms snmpget -v2c -c notthecommunity -t 1 -r 0 10.0.0.2 1.3.6.1.2.1.1.5.0 2>&1 | head -1
echo; echo "== poll interface counters =="
ip netns exec nms python3 poll.py
echo; echo "== cross-check against the kernel's own counter on the agent (ip -s link) =="
ip -n agent -s link show va | sed -n 3,4p
kill $SP 2>/dev/null; wait 2>/dev/null
