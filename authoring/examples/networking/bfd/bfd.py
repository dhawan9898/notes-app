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
