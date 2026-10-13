---
title: BFD — Bidirectional Forwarding Detection
track: networking
module: Resilience
moduleOrder: 6
order: 2
level: advanced
summary: "Sub-second failure detection independent of media and routing protocol: packet fields, session states, timers, modes, pitfalls, and a working BFD implementation run in a two-router lab (bring-up, silent-loss detection, timer negotiation, TTL 255 check)."
prerequisites:
  - "OSPFv2 Operation: Neighbors, DR/BDR, Flooding and SPF"
  - "BGP-4 Fundamentals: Sessions, Messages, Attributes and Best-Path Selection"
sources:
  - label: "RFC 5880 — Bidirectional Forwarding Detection (BFD)"
    url: https://www.rfc-editor.org/rfc/rfc5880
  - label: "RFC 5881 — BFD for IPv4 and IPv6 (Single Hop)"
    url: https://www.rfc-editor.org/rfc/rfc5881
  - label: "RFC 5883 — BFD for Multihop Paths"
    url: https://www.rfc-editor.org/rfc/rfc5883
  - label: "RFC 5884 / RFC 7130 / RFC 8971 — BFD for MPLS LSPs; BFD on LAG members (micro-BFD); BFD for VXLAN"
    url: https://www.rfc-editor.org/rfc/rfc7130
  - label: "RFC 7880 — Seamless BFD (S-BFD)"
    url: https://www.rfc-editor.org/rfc/rfc7880
---

Routing protocols detect neighbor failure by missing Hellos: OSPF's 40 s dead interval, BGP's hold time of 90–180 s. That is far too slow for modern applications. Lowering protocol timers burdens the control plane and still doesn't work for media that give no link-down signal (e.g. switches, optical gear, tunnels). **BFD** is a lightweight hello protocol that is independent of the routing protocols, media and encapsulation, and designed for hardware implementation.

## How it works

Two systems exchange BFD control packets over UDP at a negotiated rate. If no packet arrives for `detection time`, the session goes down and BFD notifies its **clients** (OSPF, IS-IS, BGP, static routes, VRRP, PIM, MPLS, ...), which immediately tear down the adjacency or remove the route, without waiting for their own timers.

| Transport | Port |
|---|---|
| Single-hop control (RFC 5881) | UDP **3784**, source port 49152–65535, **TTL/hop limit 255** (receivers verify) |
| Echo | UDP **3785** |
| Multihop (RFC 5883) | UDP **4784** |
| Micro-BFD on LAG (RFC 7130) | UDP 6784 |

### Control packet (24 bytes, optional authentication)

| Field | Size | Meaning |
|---|---|---|
| Version | 3 bits | 1 |
| Diagnostic | 5 | why the session last went down (0 none, 1 control detection time expired, 2 echo failed, 3 neighbor signaled down, 4 forwarding plane reset, 5 path down, 7 administratively down...) |
| State | 2 | 0 AdminDown, 1 Down, 2 Init, 3 Up |
| Flags | 6 | **P**oll, **F**inal, **C** (control plane independent), **A** (authentication present), **D** (demand mode), **M** (multipoint) |
| Detect Mult | 8 | multiplier for detection time |
| Length | 8 | |
| My Discriminator / Your Discriminator | 32 + 32 | identify the session at each end (so many sessions can share the same IP pair) |
| Desired Min TX Interval | 32 | µs the local system would like to use |
| Required Min RX Interval | 32 | µs the local system can accept |
| Required Min Echo RX Interval | 32 | 0 = no echo support |

### Session establishment and timers

A **three-way handshake** (Down → Init → Up) with discriminators prevents false positives. While not Up, packets are sent at ≥ 1 s intervals ("slow start"), until Up, when the negotiated fast rate is used.

- **Transmit interval** = `max(local Desired Min TX, remote Required Min RX)`.
- **Detection time** at the receiver = `remote Detect Mult × agreed remote transmit interval`.

Example: both sides at 300 ms and multiplier 3 → detect failure after 900 ms. At 50 ms × 3 → 150 ms. Parameters can be changed in-session using **Poll/Final** sequences without flapping.

### Modes

- **Asynchronous mode** (the common one): both ends send periodic packets.
- **Echo function**: one end sends packets that the other loops back in the forwarding plane (not seen by its BFD process); it tests forwarding without involving the peer's CPU. It runs at a higher rate while control packets slow down. Support varies, and it requires care with rp_filter/ICMP redirects.
- **Demand mode**: periodic packets are suppressed and a system checks only on demand; used for large numbers of sessions or links with their own detection.

## Using BFD

BFD registers with clients per neighbor:

```text
# FRR: bfdd profile and clients
bfd
 profile fast
  detect-multiplier 3
  transmit-interval 300
  receive-interval 300
 peer 192.0.2.2 interface eth0
  profile fast
!
router ospf
interface eth1
 ip ospf bfd
router bgp 65001
 neighbor 192.0.2.2 bfd profile fast
ip route 10.2.0.0/16 192.0.2.2 bfd 
```

```text
show bfd peers           # state, discriminators, intervals, diagnostics, counters
```

Typical settings: 300 ms × 3 on software platforms and campus, **50 ms × 3** on hardware-offloaded platforms (ASIC or line-card CPU) in a data center / provider core.

### Variants

- **BFD for LAG members** (micro-BFD, RFC 7130): one session per member link tests each physical member that LACP's 1–30 s timers can't detect quickly.
- **BFD for MPLS LSPs** (RFC 5884) and **multihop** sessions for loopback-to-loopback (eBGP multihop, iBGP): the detection reflects the IGP path.
- **BFD over VXLAN** (RFC 8971), **S-BFD** (RFC 7880: stateless reflector, no per-session negotiation, used in SR policies).

## Lab: a working BFD implementation

The program below implements the BFD asynchronous mode of RFC 5880 with the single-hop rules of RFC 5881: real 24-byte control packets, three-way state machine, slow start at 1 s, transmit interval and detection time computed from the peer's advertised values, 75-100 % jitter on the transmit timer, UDP port 3784 with a random source port in 49152-65535, TTL 255 on send and a TTL check on receive. It deliberately omits authentication, Poll/Final parameter changes, demand and echo modes. Two namespaces `r1` and `r2` each run one copy.

```python
#!/usr/bin/env python3
"""A small BFD (RFC 5880/5881) asynchronous-mode implementation: real 24-byte control packets on UDP 3784 with TTL 255.
Left out on purpose: authentication, Poll/Final parameter changes, demand and echo modes.
usage: bfd.py LOCAL_IP PEER_IP MY_DISCR DESIRED_TX_MS REQUIRED_RX_MS DETECT_MULT RUN_SECONDS [--ttl N]"""
import random, select, socket, struct, sys, time

ADMIN_DOWN, DOWN, INIT, UP = range(4)
NAME = ["AdminDown", "Down", "Init", "Up"]
DIAG = {0: "none", 1: "control detection time expired", 3: "neighbor signaled session down"}

local_ip, peer_ip, my_disc = sys.argv[1], sys.argv[2], int(sys.argv[3])
desired_us, required_us, mult, run_s = int(sys.argv[4]) * 1000, int(sys.argv[5]) * 1000, int(sys.argv[6]), float(sys.argv[7])
send_ttl = int(sys.argv[sys.argv.index("--ttl") + 1]) if "--ttl" in sys.argv else 255

IP_RECVTTL = getattr(socket, "IP_RECVTTL", 12)                         # Linux value; older Pythons do not export the name
IP_TTL = socket.IP_TTL
rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
rx.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
rx.setsockopt(socket.IPPROTO_IP, IP_RECVTTL, 1)                   # ask the kernel for the received TTL
rx.bind((local_ip, 3784))
tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
tx.setsockopt(socket.IPPROTO_IP, socket.IP_TTL, send_ttl)                # RFC 5881: single hop packets are sent with TTL 255
tx.bind((local_ip, random.randint(49152, 65535)))                        # source port in 49152-65535

state, diag, remote_state, remote_disc = DOWN, 0, DOWN, 0
remote_min_rx_us, remote_desired_us, remote_mult = 1, 0, 0
dropped_ttl = 0
t0 = time.monotonic()
def log(msg): print(f"  [{(time.monotonic() - t0) * 1000:7.0f} ms] {msg}", flush=True)

def tx_interval_us():
    desired = desired_us if state == UP else max(desired_us, 1_000_000)          # not Up: at least 1 s ("slow start")
    return max(desired, remote_min_rx_us)                                       # never faster than the peer can receive

def send():
    flags = 0                                                                   # no P/F/C/A/D/M flags
    pkt = struct.pack("!BBBBIIIII", (1 << 5) | diag, (state << 6) | flags, mult, 24, my_disc, remote_disc,
                      desired_us if state == UP else max(desired_us, 1_000_000), required_us, 0)
    tx.sendto(pkt, (peer_ip, 3784))

def set_state(new, new_diag):
    global state, diag
    if new != state:
        log(f"{NAME[state]} -> {NAME[new]}" + (f"   (diagnostic: {DIAG.get(new_diag, new_diag)})" if new == DOWN else ""))
    state, diag = new, new_diag

def receive(data, ttl):
    global remote_state, remote_disc, remote_min_rx_us, remote_desired_us, remote_mult, last_rx, dropped_ttl
    if ttl != 255: dropped_ttl += 1; return                                     # RFC 5881 s5: GTSM check, discard if TTL != 255
    if len(data) < 24: return
    b0, b1, dm, length, md, yd, dtx, rrx, _ = struct.unpack("!BBBBIIIII", data[:24])
    if b0 >> 5 != 1 or dm == 0 or md == 0: return
    rstate = b1 >> 6
    if yd != 0 and yd != my_disc: return                                        # not for this session
    if yd == 0 and rstate not in (DOWN, ADMIN_DOWN): return
    remote_state, remote_disc, remote_min_rx_us, remote_desired_us, remote_mult = rstate, md, rrx, dtx, dm
    last_rx = time.monotonic()
    if rstate == ADMIN_DOWN: set_state(DOWN, 3)
    elif state == DOWN:
        if rstate == DOWN: set_state(INIT, 0)
        elif rstate == INIT: set_state(UP, 0)
    elif state == INIT:
        if rstate in (INIT, UP): set_state(UP, 0)
    elif state == UP:
        if rstate == DOWN: set_state(DOWN, 3)

def detection_time_s():
    return remote_mult * max(required_us, remote_desired_us) / 1e6              # RFC 5880 6.8.4 (asynchronous mode)

last_rx = time.monotonic()
log(f"start: desired tx {desired_us // 1000} ms, required rx {required_us // 1000} ms, detect multiplier {mult}")
next_tx = time.monotonic()
reported_interval = None
while time.monotonic() - t0 < run_s:
    now = time.monotonic()
    if now >= next_tx:
        send()
        iv = tx_interval_us() / 1e6
        next_tx = now + iv * (random.uniform(0.75, 1.0) if mult > 1 else random.uniform(0.75, 0.90))   # RFC 5880 6.8.7 jitter
        cur = (state, tx_interval_us(), detection_time_s())
        if state == UP and cur != reported_interval:
            reported_interval = cur
            log(f"negotiated: transmit every {tx_interval_us() / 1000:.0f} ms, detection time {detection_time_s() * 1000:.0f} ms (= remote multiplier {remote_mult} x {max(required_us, remote_desired_us) / 1000:.0f} ms)")
    r, _, _ = select.select([rx], [], [], max(0, min(next_tx - time.monotonic(), 0.005)))
    if r:
        data, anc, _, _ = rx.recvmsg(256, 64)
        ttl = next((struct.unpack("i", d[:4])[0] for lv, ty, d in anc if lv == socket.IPPROTO_IP and ty == IP_TTL), 0)
        receive(data, ttl)
    if state in (INIT, UP) and time.monotonic() - last_rx > detection_time_s():
        set_state(DOWN, 1)
        remote_state, remote_disc = DOWN, 0
log(f"end: final state {NAME[state]}" + (f", {dropped_ttl} packets discarded for TTL != 255" if dropped_ttl else ""))
```

```bash
#!/bin/sh
# Two routers r1 and r2 on one link, each running bfd.py. Scenarios: bring-up, link failure, timer negotiation, TTL check.
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
ip netns add r1; ip netns add r2
ip link add v1 type veth peer name v2; ip link set v1 netns r1; ip link set v2 netns r2
ip -n r1 addr add 192.0.2.1/24 dev v1; ip -n r2 addr add 192.0.2.2/24 dev v2
ip -n r1 link set v1 up; ip -n r2 link set v2 up; ip -n r1 link set lo up; ip -n r2 link set lo up
run() { ns=$1; shift; ip netns exec $ns python3 bfd.py "$@"; }

echo "== 1. bring-up: both sides 100 ms x 3 =="
run r1 192.0.2.1 192.0.2.2 11 100 100 3 5 > /tmp/r1.$$ & run r2 192.0.2.2 192.0.2.1 22 100 100 3 5 > /tmp/r2.$$ & wait
echo "router r1:"; cat /tmp/r1.$$; echo "router r2:"; cat /tmp/r2.$$

echo; echo "== 2. failure: from t=4 s the link silently drops all packets (carrier stays up, like a broken switch in the path) =="
(sleep 4; ip netns exec r1 tc qdisc add dev v1 root netem loss 100%; ip netns exec r2 tc qdisc add dev v2 root netem loss 100%) &
run r1 192.0.2.1 192.0.2.2 11 100 100 3 7 > /tmp/r1.$$ & run r2 192.0.2.2 192.0.2.1 22 100 100 3 7 > /tmp/r2.$$ & wait
echo "router r1 (the loss starts at about 4000 ms):"; cat /tmp/r1.$$
ip netns exec r1 tc qdisc del dev v1 root; ip netns exec r2 tc qdisc del dev v2 root

echo; echo "== 3. timer negotiation: r1 wants 50 ms x 3, r2 can only receive every 400 ms and wants 200 ms x 5 =="
run r1 192.0.2.1 192.0.2.2 11 50 50 3 5 > /tmp/r1.$$ & run r2 192.0.2.2 192.0.2.1 22 200 400 5 5 > /tmp/r2.$$ & wait
echo "router r1:"; grep -E "negotiated|->" /tmp/r1.$$; echo "router r2:"; grep -E "negotiated|->" /tmp/r2.$$

echo; echo "== 4. TTL check: r2 sends with TTL 64 (as a spoofer several hops away would) =="
run r1 192.0.2.1 192.0.2.2 11 100 100 3 4 > /tmp/r1.$$ & run r2 192.0.2.2 192.0.2.1 22 100 100 3 4 --ttl 64 > /tmp/r2.$$ & wait
echo "router r1:"; cat /tmp/r1.$$
rm -f /tmp/r1.$$ /tmp/r2.$$
```

```text
$ unshare -Urnm sh lab.sh 2>&1
== 1. bring-up: both sides 100 ms x 3 ==
router r1:
  [      0 ms] start: desired tx 100 ms, required rx 100 ms, detect multiplier 3
  [      5 ms] Down -> Init
  [   1587 ms] Init -> Up
  [   2488 ms] negotiated: transmit every 100 ms, detection time 300 ms (= remote multiplier 3 x 100 ms)
  [   5001 ms] end: final state Up
router r2:
  [      0 ms] start: desired tx 100 ms, required rx 100 ms, detect multiplier 3
  [    784 ms] Down -> Up
  [   1582 ms] negotiated: transmit every 100 ms, detection time 3000 ms (= remote multiplier 3 x 1000 ms)
  [   2532 ms] negotiated: transmit every 100 ms, detection time 300 ms (= remote multiplier 3 x 100 ms)
  [   5001 ms] end: final state Up

== 2. failure: from t=4 s the link silently drops all packets (carrier stays up, like a broken switch in the path) ==
router r1 (the loss starts at about 4000 ms):
  [      0 ms] start: desired tx 100 ms, required rx 100 ms, detect multiplier 3
  [      5 ms] Down -> Init
  [   1598 ms] Init -> Up
  [   1792 ms] negotiated: transmit every 100 ms, detection time 300 ms (= remote multiplier 3 x 100 ms)
  [   4212 ms] Up -> Down   (diagnostic: control detection time expired)
  [   7005 ms] end: final state Down

== 3. timer negotiation: r1 wants 50 ms x 3, r2 can only receive every 400 ms and wants 200 ms x 5 ==
router r1:
  [    908 ms] Down -> Up
  [   1537 ms] negotiated: transmit every 400 ms, detection time 5000 ms (= remote multiplier 5 x 1000 ms)
  [   1929 ms] negotiated: transmit every 400 ms, detection time 1000 ms (= remote multiplier 5 x 200 ms)
router r2:
  [      2 ms] Down -> Init
  [   1539 ms] Init -> Up
  [   1863 ms] negotiated: transmit every 200 ms, detection time 1200 ms (= remote multiplier 3 x 400 ms)

== 4. TTL check: r2 sends with TTL 64 (as a spoofer several hops away would) ==
router r1:
  [      0 ms] start: desired tx 100 ms, required rx 100 ms, detect multiplier 3
  [   4001 ms] end: final state Down, 4 packets discarded for TTL != 255
```

Reading the four scenarios (exact millisecond values vary from run to run because of timer jitter and process start-up):

1. **Bring-up** takes roughly one to two seconds, not 100 ms, because a session that is not Up sends only once per second (the "slow start" rule that stops a flapping neighbor from flooding the link). The states move Down, Init and Up through the discriminator handshake. Note the first `negotiated` line: right after the session comes Up the detection time is still 3000 ms, because it is computed from the **last packet received**, which still carried the peer's slow-start interval of 1 s. It drops to 300 ms (3 x 100 ms) as soon as a packet with the fast interval arrives. A real implementation uses Poll/Final to make both ends switch together; this one just follows the peer's packets.
2. **Failure** with the link still electrically up (a netem 100 % loss, the equivalent of a broken switch or a firewall silently dropping packets): `r1` declares `Up -> Down` with diagnostic 1 (control detection time expired) about 300 ms after the last good packet, in the neighbourhood of 4.2-4.4 s here, where the loss started at about 4.0 s. A routing protocol relying on its own hello timers would have needed 40 s (OSPF default dead interval) or up to 3 minutes (BGP hold time). Carrier-based detection would not have noticed at all.
3. **Timer negotiation**: `r1` wants to send every 50 ms but `r2` says it can receive only every 400 ms, so `r1` transmits every 400 ms (`max(own desired, remote required RX)`). `r2` wants 200 ms and `r1` accepts 50 ms, so `r2` sends every 200 ms. The detection times are asymmetric as a result: `r1` waits 5 x 200 ms = 1000 ms for `r2`, while `r2` waits 3 x 400 ms = 1200 ms for `r1`. The multiplier belongs to the **sender**, the interval to the agreement between the two.
4. **TTL 255 check**: packets sent with TTL 64 are discarded on receive, so `r1` never leaves Down and counts the discards. A device that is not directly attached cannot craft a packet that arrives with TTL 255, which is the entire security argument of RFC 5881 (the Generalized TTL Security Mechanism) and why BFD needs no authentication on a single hop but does on multihop sessions (RFC 5883).

## Pitfalls

1. **Aggressive timers on a loaded CPU cause false downs**: a software BFD session at 50 ms needs real-time scheduling; a CPU spike or control-plane policing drop produces flaps that drop BGP/OSPF adjacencies, amplifying instability. Prefer hardware-assisted BFD for < 300 ms.
2. **Control-plane independence (C-bit)**: does BFD continue when the control plane restarts (graceful restart)? If C=1 and the forwarding plane is healthy, BFD must stay up while the routing process restarts. Combining GR and BFD requires matching settings, otherwise BFD declares the failure that GR wanted to mask.
3. **Asymmetric failures**: BFD detects one-way failures because both directions' packets are needed to stay Up.
4. **ACLs / CoPP** must permit UDP 3784/4784 from known peers, with TTL 255 enforcement.
5. **Scale**: hundreds of sessions at 50 ms multiply packet rates (each is 20 pps per direction); check platform limits.
6. **Does not detect everything**: BFD proves the path works for BFD packets; QoS, ACL or hashing issues that affect only data traffic are invisible, and with ECMP/LAG a single BFD flow tests only the path its hash selects.
7. **Security**: authenticate (keyed SHA1 / meticulous modes, RFC 5880 §6.7) for multihop, and rely on TTL 255 for single hop.

## Key takeaways

- BFD gives fast, protocol-agnostic failure detection with a simple UDP hello and three-way handshake; detection time = detect-mult × negotiated interval.
- Clients (OSPF, IS-IS, BGP, static, VRRP, MPLS, LAG) tear down state on BFD down; the timers are negotiated per session.
- Use hardware-offloaded BFD for ≤ 50 ms; software implementations should stay at several hundred ms to avoid false positives.
- Plan for GR interactions, CoPP/ACL permits, and LAG/ECMP coverage (micro-BFD).
