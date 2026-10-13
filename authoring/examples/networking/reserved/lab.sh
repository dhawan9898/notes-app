#!/bin/sh
# h1 -- br0 (Linux bridge in namespace sw) -- h2. Which reserved-address frames does the bridge relay?
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
for n in h1 h2 sw; do ip netns add $n; ip -n $n link set lo up; done
ip link add e1 type veth peer name p1; ip link set e1 netns h1; ip link set p1 netns sw
ip link add e2 type veth peer name p2; ip link set e2 netns h2; ip link set p2 netns sw
ip -n h1 link set e1 up; ip -n h2 link set e2 up
ip -n sw link add br0 type bridge stp_state 0; ip -n sw link set p1 master br0; ip -n sw link set p2 master br0
ip -n sw link set p1 up; ip -n sw link set p2 up; ip -n sw link set br0 up
sleep 1
try() {
    ip netns exec h2 python3 recv.py e2 1.5 &
    sleep 0.4; ip netns exec h1 python3 send.py e1; wait
}
echo '$ ip -d link show br0 | grep -o "group_fwd_mask [0-9a-fx]*"'; ip -n sw -d link show br0 | grep -o "group_fwd_mask [0-9a-fx]*"
echo; echo "== default bridge: what does h2 receive from the 7 frames h1 sent? =="; try
echo; echo "== set group_fwd_mask 0x4000 (bit 14 = 01-80-C2-00-00-0E, LLDP) =="
ip -n sw link set br0 type bridge group_fwd_mask 0x4000; try
echo; echo "== try to also forward the STP/PAUSE/LACP addresses (mask 0x0007) =="
ip -n sw link set br0 type bridge group_fwd_mask 0x0007 2>&1 | sed 's/^/  /'
echo; echo "== enable STP on the bridge: BPDUs are now consumed by the bridge itself =="
ip -n sw link set br0 type bridge group_fwd_mask 0 stp_state 1; sleep 1; try
