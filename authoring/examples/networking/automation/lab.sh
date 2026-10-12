#!/bin/sh
# Declarative configuration of a Linux "router" (namespace dev) with a neighbor (namespace peer) on a virtual cable.
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
ip netns add dev; ip netns add peer; ip link add uplink type veth peer name p0; ip link set uplink netns dev; ip link set p0 netns peer
ip -n peer addr add 10.0.0.2/24 dev p0; ip -n peer link set p0 up; ip -n peer link set lo up; ip -n dev link set lo up
R="ip netns exec dev python3 reconcile.py"
echo '$ reconcile.py desired.json --check          (dry run: what would change?)'; $R desired.json --check
echo; echo '$ reconcile.py desired.json                    (apply)'; $R desired.json
echo; echo '$ reconcile.py desired.json                    (apply again: idempotent)'; $R desired.json
echo; echo '$ ip addr add 10.0.0.1/24 dev uplink          (the imperative way, a second time)'; ip -n dev addr add 10.0.0.1/24 dev uplink 2>&1
echo; echo "== drift: someone changes the device by hand =="; ip -n dev link set uplink mtu 9000; ip -n dev addr add 10.66.0.1/24 dev uplink; ip -n dev route del 10.9.0.0/16
echo '$ reconcile.py desired.json --check --strict'; $R desired.json --check --strict
echo; echo '$ reconcile.py desired.json --strict'; $R desired.json --strict
echo; echo '$ reconcile.py bad.json                         (validation happens before any change)'; $R bad.json 2>&1 | tail -1
echo; echo "== a change that breaks reachability, with automatic rollback =="
echo '$ reconcile.py change.json --strict --rollback-ping 10.0.0.2'; $R change.json --strict --rollback-ping 10.0.0.2
echo; echo "IPv4 addresses on uplink after the rollback:"; ip -n dev -4 -br addr show uplink | awk '{print "  " $3}'
echo; echo "routes after rollback:"; ip -n dev -4 route | sed "s/^/  /"
echo; echo "$ reconcile.py desired.json --strict --check   (is the device back in the desired state?)"; $R desired.json --strict --check
