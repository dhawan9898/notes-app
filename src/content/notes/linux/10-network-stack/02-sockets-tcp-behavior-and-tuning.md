---
title: Sockets, TCP Behavior and Linux Network Tuning
track: linux
module: Network Stack
moduleOrder: 10
order: 2
level: expert
summary: How TCP connections behave on Linux — accept and SYN queues, state machine and TIME_WAIT, congestion control, windows and buffer autotuning, Nagle and delayed ACK, keepalive — with the sysctls that matter, how to read ss -ti, and tuning recipes by workload.
sources:
  - label: "RFC 9293 — Transmission Control Protocol (TCP); RFC 5681 — TCP Congestion Control; RFC 6298 — RTO; RFC 7323 — Window Scaling and Timestamps"
    url: https://www.rfc-editor.org/rfc/rfc9293
  - label: "RFC 6928 — Increasing TCP's Initial Window; RFC 8312 — CUBIC; RFC 7413 — TCP Fast Open; RFC 2018 — SACK"
    url: https://www.rfc-editor.org/rfc/rfc8312
  - label: "Linux kernel docs — networking/ip-sysctl.rst (tcp_*, net.core.*)"
    url: https://docs.kernel.org/networking/ip-sysctl.html
  - label: "man 7 tcp, man 7 socket, man 7 ip, man 8 ss, man 8 nstat"
    url: https://man7.org/linux/man-pages/man7/tcp.7.html
  - label: "Cardwell et al. — BBR - Congestion-Based Congestion Control (ACM Queue 2016)"
  - label: "Stevens — TCP/IP Illustrated, Vol. 1; Fall & Stevens"
---

TCP gives applications a reliable ordered byte stream over lossy IP. The kernel implements it, and almost every "network is slow" problem is some interaction between TCP's mechanisms and the path. The vocabulary below lets you read `ss`, `nstat` and packet captures and tune with intent.

## Server-side socket lifecycle and queues

```text
socket() -> bind() -> listen(backlog) -> accept() -> read()/write() -> close()
client:  socket() -> connect() -> write()/read() -> close()
```

A listening socket has **two queues**:

1. **SYN queue (incomplete connections)**: entries in `SYN_RECV` after the server sent SYN-ACK and awaits the final ACK. Bounded by `net.ipv4.tcp_max_syn_backlog` (2048 on the example host). When full, new SYNs are dropped, or with `tcp_syncookies=1` the server encodes state in the sequence number and avoids storing it (protects against SYN floods; some TCP options are lost).
2. **Accept queue (completed connections)**: connections in `ESTABLISHED` waiting for the application to call `accept()`. Its length is `min(backlog argument to listen(), net.core.somaxconn)` (somaxconn = 4096 on the example host; older kernels defaulted to 128).

```bash
$ ss -ltn
State  Recv-Q Send-Q Local Address:Port
LISTEN 0      100    0.0.0.0:5014        # for a listener: Recv-Q = current accept-queue length, Send-Q = its maximum (backlog)
```

If Recv-Q approaches Send-Q the application isn't accepting fast enough: later connections are dropped or reset (`nstat -az TcpExtListenOverflows TcpExtListenDrops`, clients see timeouts or `ECONNREFUSED`/`ECONNRESET` depending on `tcp_abort_on_overflow`). Fix: a faster accept loop (multiple acceptors/`SO_REUSEPORT`, epoll), a larger `listen(fd, 4096)` backlog **and** `somaxconn`. Many frameworks default to backlog 128–511.

`SO_REUSEADDR` lets a server rebind while old connections are in TIME_WAIT; `SO_REUSEPORT` lets multiple sockets/processes bind the same port with the kernel balancing connections (per-CPU accept queues, no thundering herd).

## The state machine and its problem children

Connection setup is the three-way handshake `SYN → SYN-ACK → ACK`; teardown uses `FIN`/`ACK` in each direction; abrupt termination uses `RST`.

| State | Where | Notes |
|---|---|---|
| `LISTEN`, `SYN_SENT`, `SYN_RECV`, `ESTABLISHED` | handshake and data | |
| `FIN_WAIT_1/2`, `CLOSING`, `LAST_ACK` | teardown | `FIN_WAIT_2` orphaned sockets time out after `tcp_fin_timeout` (60 s) |
| **`CLOSE_WAIT`** | peer sent FIN, **local application hasn't called `close()`** | accumulating `CLOSE_WAIT` = **application bug** (leaked sockets): the kernel waits forever for you; `ss -tan state close-wait`, find the owner with `ss -tanp`, fix the code (close on EOF/error) |
| **`TIME_WAIT`** | the side that closes first waits 2×MSL (**60 s** on Linux, fixed) | protects against delayed segments from the old connection being mistaken for a new one; harmless in small numbers; many thousands on a client making short connections to one server can exhaust ephemeral ports |

Handling TIME_WAIT/port exhaustion properly (in order): reuse connections (HTTP keep-alive, connection pools, multiplexing HTTP/2), let the server close first rather than the client when appropriate, widen `net.ipv4.ip_local_port_range` (default 32768–60999 ≈ 28k ports per source IP per destination IP:port), add source IPs, and for outgoing connections `net.ipv4.tcp_tw_reuse=1` (default `2` = loopback only on recent kernels) which safely reuses TIME_WAIT sockets for *outgoing* connections using timestamps. **Do not** use the removed `tcp_tw_recycle` (it broke clients behind NAT and was deleted in 4.12).

`RST` causes: connecting to a closed port, an application closing with unread data, firewall/LB idle-timeout resets, `SO_LINGER{1,0}`. `ss -ti` and tcpdump show who sent it.

## Reliability and congestion control

- **Sequence numbers and ACKs**, **retransmission timeout (RTO)** computed from smoothed RTT (min 200 ms on Linux; `rto:208` in the `ss` sample below), **fast retransmit** after 3 duplicate ACKs or **SACK** evidence (`tcp_sack=1`), **RACK/TLP** for faster tail-loss recovery, exponential backoff on repeated timeouts (up to `tcp_retries2` = 15 ≈ 15 minutes before declaring a dead connection; `tcp_syn_retries` = 6 ≈ 127 s for connect()).
- **Flow control**: the receiver advertises a window (bytes it can buffer); with **window scaling** (`wscale:7,7` = ×128, RFC 7323) up to 1 GiB.
- **Congestion control** limits the sender by `cwnd` (congestion window, in segments) in addition to the receiver window: **slow start** from the **initial window** (10 segments ≈ 14 KB, RFC 6928) doubling per RTT until loss or `ssthresh`, then **congestion avoidance**. A new connection's first 14 KB take one RTT; transferring 1 MB needs ~7 RTTs of ramp-up, which is why connection reuse and `tcp_slow_start_after_idle=0` (don't reset `cwnd` after idle; the default is `1`) help request/response services.
- Algorithms (`net.ipv4.tcp_congestion_control`; list in `tcp_available_congestion_control`; load modules to add): **CUBIC** (default; loss-based, window grows as a cubic function of time since the last loss, good on high-BDP paths), **Reno**, **BBR** (model-based: estimates bottleneck bandwidth and min RTT, doesn't treat all loss as congestion; strong on lossy long-haul paths and with shallow buffers; can be unfair to CUBIC flows; pair with `fq` qdisc for pacing: `sysctl net.core.default_qdisc=fq net.ipv4.tcp_congestion_control=bbr`), **DCTCP** (data centers with ECN). Congestion control affects only the *sending* side, so change it where the bulk data originates.

### Bandwidth-delay product and buffers

To fill a path you need `window ≥ BDP = bandwidth × RTT`. A 1 Gb/s link with 80 ms RTT has BDP = 10 MB: with 64 KB windows, throughput is capped at `window/RTT = 64 KB / 0.08 s ≈ 6.5 Mb/s`. Linux autotunes socket buffers: `net.ipv4.tcp_rmem = min default max` (example: `4096 131072 6291456`) and `tcp_wmem` (`4096 16384 4194304`); the *maximum* bounds long-fat-network throughput. Increase the max (`net.core.rmem_max`/`wmem_max` only limit `setsockopt(SO_RCVBUF)` values; autotuning obeys the `tcp_*mem` max) for high-BDP paths, but not blindly: larger buffers cost memory per connection and add queueing latency. Setting `SO_RCVBUF`/`SO_SNDBUF` explicitly **disables autotuning** for that socket (and the kernel doubles the value for bookkeeping), so avoid it unless you know why.

`tcp_notsent_lowat` limits unsent data queued in the send buffer (useful for HTTP/2 prioritization and latency).

## Latency-related behaviors

- **Nagle's algorithm** (hold small writes until the previous data is acknowledged) combined with the receiver's **delayed ACK** (wait up to 40 ms hoping to piggyback) can create fixed ~40 ms stalls in request/response protocols that write headers and body separately ("write-write-read"). Fix: set `TCP_NODELAY` on the socket (most RPC/HTTP libraries do) or coalesce writes (`writev`, `MSG_MORE`, `TCP_CORK` to batch then uncork).
- **Head-of-line blocking**: one lost segment stalls all later data on that connection (including independent HTTP/2 streams): a motivation for QUIC/HTTP/3.
- **Bufferbloat**: oversized queues in routers, NICs and qdiscs inflate RTT under load; use `fq_codel`/`cake`, BBR, smaller buffers.
- **Keepalive**: disabled by default; with `SO_KEEPALIVE` the kernel probes idle connections after `tcp_keepalive_time` (7200 s) every `tcp_keepalive_intvl` (75 s) up to `tcp_keepalive_probes` (9), too slow for modern NAT/LB idle timeouts (often 60–350 s): set per-socket `TCP_KEEPIDLE/INTVL/CNT` or application-level heartbeats. `TCP_USER_TIMEOUT` bounds how long unacknowledged data may remain before the connection is aborted.
- **TCP Fast Open** (`tcp_fastopen`=1 client, 3 both) saves a round trip for repeat connections, needing client and server support; middleboxes make it fragile.
- **ECN** (`tcp_ecn`=2: accept but don't request): congestion marks instead of drops.
- **MTU / MSS**: MSS = MTU − 40 (IPv4) − options (timestamps 12 B); PMTUD requires ICMP; `tcp_mtu_probing=1` fallback; `ss -ti` shows `mss`, `pmtu`.

## Reading `ss -ti`

```text
$ ss -ti state established
cubic wscale:7,7 rto:208 rtt:5.325/0.754 ato:40 mss:32768 pmtu:65535 rcvmss:536 advmss:65483 cwnd:10
 bytes_sent:73 bytes_acked:74 bytes_received:15829 segs_out:10244 segs_in:10186 data_segs_out:69
 send 492Mbps pacing_rate 984Mbps delivery_rate 14.6Gbps app_limited busy:364ms rcv_space:65495
 rcv_ssthresh:65495 minrtt:0.018 snd_wnd:65536 rcv_wnd:65536
```

| Field | Meaning |
|---|---|
| `cubic` | congestion control algorithm |
| `rtt:srtt/rttvar` | smoothed round-trip time and variance in ms |
| `rto` | current retransmission timeout (ms) |
| `cwnd`, `ssthresh` | congestion window in segments, slow start threshold |
| `mss`, `pmtu` | segment size and path MTU |
| `retrans:cur/total`, `lost`, `reordering` | loss and reordering evidence (`retrans:0/12`) |
| `send`, `pacing_rate`, `delivery_rate` | estimated capacity |
| `app_limited` | the application isn't supplying data fast enough (not the network's fault) |
| `rwnd_limited`, `sndbuf_limited`, `busy` (with `-i`) | time fractions limited by receiver window, send buffer, or cwnd: which resource is the bottleneck |
| `snd_wnd`, `rcv_wnd`, `rcv_space` | window sizes |
| `unacked`, `notsent` | in-flight and unsent bytes; `Send-Q` on established sockets = bytes not yet acknowledged, `Recv-Q` = bytes not yet read by the application |

Use `ss -tin`, `ss -tinoe`, `ss -o state established '( dport = :443 )'`, `nstat -az | grep -i retrans`. Large `Recv-Q` on an established socket = slow reader; large `Send-Q` = network or peer slow.

## Useful counters (`nstat -az`, `/proc/net/netstat`)

`TcpRetransSegs`, `TcpExtTCPTimeouts` (RTO timer fired), `TcpExtTCPFastRetrans`, `TcpExtTCPLostRetransmit`, `TcpExtListenOverflows`/`ListenDrops`, `TcpExtTCPBacklogDrop`, `TcpExtTCPRcvQDrop`, `TcpExtPruneCalled` (receive buffer pressure), `TcpExtTCPSynRetrans`, `TcpAttemptFails`, `TcpEstabResets`, `TcpExtTCPAbortOnTimeout`, `UdpRcvbufErrors`, `UdpSndbufErrors`. Rates matter more than totals: sample twice.

## Tuning by workload

Change one thing at a time and measure; defaults are good for most cases.

**High-connection-count servers (web, proxies):**

```ini
net.core.somaxconn = 4096                # plus a large listen() backlog in the app
net.ipv4.tcp_max_syn_backlog = 8192
net.ipv4.ip_local_port_range = 10240 65535     # busy clients/proxies
net.ipv4.tcp_tw_reuse = 1                # outgoing connections
fs.file-max = 2097152                    # plus LimitNOFILE= for the service
net.netfilter.nf_conntrack_max = 1048576 # if using conntrack; match hash size
```

**Long-fat networks / bulk transfer (replication, backups, cross-region):**

```ini
net.ipv4.tcp_rmem = 4096 262144 67108864
net.ipv4.tcp_wmem = 4096 262144 67108864
net.core.rmem_max = 67108864
net.core.wmem_max = 67108864
net.core.default_qdisc = fq
net.ipv4.tcp_congestion_control = bbr
net.ipv4.tcp_mtu_probing = 1
```

**Latency-sensitive RPC:** `TCP_NODELAY`, keep connections warm (pools, `tcp_slow_start_after_idle=0`), moderate buffers and `fq_codel`, pin interrupts and threads, disable power-saving states, avoid Nagle/delayed-ACK interaction, monitor p99 and retransmits.

**UDP-heavy (DNS, video, QUIC):** raise `net.core.rmem_max` and set `SO_RCVBUF`, `net.core.netdev_max_backlog`, spread with `SO_REUSEPORT`, check `UdpRcvbufErrors`, use `recvmmsg`/GRO for UDP (`UDP_GRO`), consider XDP/AF_XDP for extreme rates.

Persisting: files in `/etc/sysctl.d/`, applied with `sysctl --system`. Container hosts: many `net.*` keys are per-network-namespace (settable inside), others are global.

## Troubleshooting flow for "TCP is slow"

1. Is it the application (`app_limited`, small writes, `Recv-Q` growing, GC pauses) or the network (`retrans`, `cwnd` stuck low, `rtt` high)?
2. Measure the path: `iperf3 -c host -P 1` and `-P 8` (single flow vs parallel), `mtr`, `ping -M do -s`, `ss -ti` during transfer.
3. Single-flow limited by loss + RTT (Mathis: throughput ≈ MSS/(RTT·√p)): reduce loss, use BBR, or parallel flows. Limited by window: raise buffer max and check scaling/timestamps through middleboxes (`tcp_window_scaling`).
4. Check the receive path (drops, softirq), qdisc drops, NIC errors, MTU/PMTUD, conntrack, offload bugs (toggle GRO/TSO to test).
5. Capture with `tcpdump -w`, analyze in Wireshark (TCP Stream Graphs: Time-Sequence, Window Scaling; Expert Info for retransmissions, zero window, dup ACK).

## Key takeaways

- Two listener queues: SYN (`tcp_max_syn_backlog`, syncookies) and accept (`min(backlog, somaxconn)`); `ss -ltn` Recv-Q/Send-Q shows the accept queue; overflow shows in `nstat`.
- `CLOSE_WAIT` is an application bug; `TIME_WAIT` is protective: fix port exhaustion with connection reuse, ranges and `tcp_tw_reuse`, not by hacks.
- Throughput ≈ min(cwnd, receive window)/RTT: size buffers to the BDP (autotuning), pick the congestion control for the path (CUBIC default, BBR for lossy/long, DCTCP in DCs).
- Small-write latency stalls come from Nagle + delayed ACK: `TCP_NODELAY` or coalesce writes; keepalive defaults are far too slow for NAT/LB timeouts.
- Read `ss -ti` and `nstat`; tune from evidence, one sysctl at a time.
