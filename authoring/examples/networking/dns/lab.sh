#!/bin/sh
# Root, TLD and authoritative servers plus clients, all in one throwaway network namespace.
ip link set lo up
ip addr add 10.1.1.1/32 dev lo; ip addr add 10.1.1.2/32 dev lo; ip addr add 10.1.1.3/32 dev lo
python3 servers.py &
SPID=$!; sleep 0.5
echo "=== the wire format ==="; python3 wire.py
echo; echo "=== an iterative caching resolver ==="; python3 resolver.py
echo "=== the real dig tool against our authoritative server ==="
dig @10.1.1.3 web.example.test A +norecurse +noall +comments +answer +authority +additional +time=2 +tries=1 | grep -v "^;; \(OPT\|WARNING\)" | sed '/^$/d'
echo; dig @10.1.1.1 www.example.test A +norecurse +noall +comments +answer +authority +additional +time=2 +tries=1 | grep -v "^;; \(OPT\|WARNING\)" | sed '/^$/d'
kill $SPID
