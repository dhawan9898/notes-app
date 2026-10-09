---
title: Sockets Programming in C
track: c
module: Systems-Level C
moduleOrder: 7
order: 3
level: intermediate
summary: Writing correct TCP and UDP clients and servers — the socket call sequence, getaddrinfo, partial sends and receives, message framing, SIGPIPE, SO_REUSEADDR, concurrency models, and a measured 1,500× latency difference from Nagle's algorithm.
sources:
  - label: "POSIX.1-2017 — sys/socket.h, netdb.h: socket(), bind(), listen(), accept(), connect(), send(), recv(), getaddrinfo()"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/
  - label: man 7 socket, man 7 tcp, man 7 udp, man 7 ip, man 3 getaddrinfo, man 2 accept4
    url: https://man7.org/linux/man-pages/man7/socket.7.html
  - label: Stevens, Fenner, Rudoff — UNIX Network Programming, Vol. 1 (The Sockets Networking API)
  - label: Beej's Guide to Network Programming
    url: https://beej.us/guide/bgnet/
  - label: RFC 9293 (TCP), RFC 768 (UDP), RFC 896 (Nagle), RFC 1122 §4.2.3.2 (delayed ACK)
    url: https://www.rfc-editor.org/rfc/rfc896
---

The Berkeley sockets API (1983) is the universal interface to TCP/IP. A socket is a file descriptor that you `read`/`write` (or `send`/`recv`), but its semantics differ from files in ways that cause most beginner bugs: **TCP is a byte stream with no message boundaries**, calls transfer *fewer* bytes than asked, peers disappear without notice, and the network adds latency and loss.

## The call sequences

```text
TCP server                         TCP client
socket()                           socket()
setsockopt(SO_REUSEADDR)
bind(addr:port)
listen(backlog)
accept()  <------ handshake ------ connect()
read/recv  <----- data ----------> write/send
close()                            close()

UDP: socket(SOCK_DGRAM); bind() [server]; sendto()/recvfrom() — no connection, no accept
```

| Call | Notes |
|---|---|
| `socket(domain, type, protocol)` | `AF_INET`/`AF_INET6`/`AF_UNIX`; `SOCK_STREAM` (TCP), `SOCK_DGRAM` (UDP), `SOCK_SEQPACKET`; OR with `SOCK_CLOEXEC \| SOCK_NONBLOCK` |
| `bind` | choose the local address/port; port 0 = kernel picks an ephemeral port (read it back with `getsockname`) |
| `listen(fd, backlog)` | marks passive; backlog bounds the accept queue (capped by `net.core.somaxconn`) |
| `accept4(fd, &peer, &len, flags)` | returns a *new* connected socket; blocks unless non-blocking |
| `connect` | active open; blocks until established or fails (`ECONNREFUSED`, `ETIMEDOUT`, `ENETUNREACH`) |
| `send/recv`, `sendto/recvfrom`, `sendmsg/recvmsg` | with flags (`MSG_NOSIGNAL`, `MSG_PEEK`, `MSG_DONTWAIT`, `MSG_TRUNC`, `MSG_WAITALL`) |
| `shutdown(fd, SHUT_WR)` | half-close: tell the peer you're done sending but still read |
| `getsockopt/setsockopt` | `SO_REUSEADDR`, `SO_REUSEPORT`, `SO_KEEPALIVE`, `SO_RCVBUF/SNDBUF`, `SO_ERROR`, `TCP_NODELAY`, `SO_LINGER`, `IPV6_V6ONLY` |
| `getpeername/getsockname` | addresses of the connection |
| `poll/epoll/select` | readiness multiplexing (see the event-driven I/O note) |

## Names and addresses: use `getaddrinfo`

Never hard-code `AF_INET` structs and `gethostbyname` (deprecated, IPv4-only, not thread-safe). `getaddrinfo(host, service, &hints, &res)` resolves names and service names to a **list** of addresses (IPv6 and IPv4, multiple A/AAAA records); loop over them until one works. `AI_PASSIVE` yields a wildcard address for servers; `AI_NUMERICHOST`/`AI_NUMERICSERV` avoid DNS lookups; free with `freeaddrinfo`; errors use `gai_strerror`. Convert addresses with `inet_ntop`/`inet_pton`; ports and addresses in sockaddr structs are in **network byte order** (`htons`, `htonl`). Use `struct sockaddr_storage` to hold any address. `getnameinfo` goes the other way (and does blocking DNS: avoid it in the request path).

Be aware that DNS resolution blocks (use a thread, `getaddrinfo_a`, or `c-ares` in event loops), and "happy eyeballs" connects to IPv6 and IPv4 in parallel.

## Shared helpers: send/receive everything, with framing

TCP gives a **stream**: one `send` of 100 bytes may arrive as two `recv`s of 40 and 60, or merged with the next message. Likewise `send` may accept only part of your buffer (full socket buffers, signals). Therefore (`common.h`):

{{include c/net/common.h}}

- `send_all`/`recv_all` loop until the requested byte count is transferred, retrying on `EINTR`. `MSG_NOSIGNAL` turns `SIGPIPE` into an `EPIPE` error (or ignore `SIGPIPE` globally in servers).
- **Framing**: the application must define message boundaries: a length prefix (as above), delimiters (HTTP headers end with `\r\n\r\n`, Redis RESP lines), fixed-size records, or self-describing formats (TLV, protobuf with a length). The code reads the 4-byte header, **checks the length against the buffer capacity before reading** (never trust the peer's number), then reads exactly that many bytes.
- `recv` returns `0` when the peer closed its sending side (EOF), `-1` for an error (`ECONNRESET` when RST received).

## A threaded echo server

{{include c/net/server.c}}

The server (thread per connection, for clarity) resolves the bind address with `getaddrinfo`, sets `SO_REUSEADDR` so it can restart immediately even if old connections linger in TIME_WAIT, listens with a backlog of 128, accepts with `accept4(SOCK_CLOEXEC)`, spawns a detached thread per client, and echoes each framed message upper-cased. `ss` confirms the listener:

{{run c/net :: gcc -std=gnu11 -O1 -Wall -Wextra -pthread server.c -o server && (./server 9100 2>/dev/null & echo $! > srv.pid); sleep 0.3; ss -ltnp 'sport = :9100' 2>/dev/null | cut -c1-100; kill $(cat srv.pid); rm -f srv.pid}}

(`Recv-Q` on a listening socket is the current accept queue length, `Send-Q` its maximum, the backlog.)

## The client, and a 1,500× latency lesson

{{include c/net/client.c}}

The client connects by trying each `getaddrinfo` result, sends one message and prints the reply, then measures a request pattern many protocols produce: it writes a **header** and a **body** with two separate `send` calls, then waits for the reply.

{{run c/net :: gcc -std=gnu11 -O1 -Wall -Wextra client.c -o client && gcc -std=gnu11 -O1 -Wall -Wextra -pthread server.c -o server && (./server 9100 2>/dev/null & echo $! > srv.pid); sleep 0.3; ./client 9100; kill $(cat srv.pid); rm -f srv.pid}}

Results: **~46 ms** per request with default settings versus **0.03 ms** with `TCP_NODELAY`, a 1,500× difference caused by two TCP mechanisms interacting:

1. **Nagle's algorithm** (RFC 896): while there is unacknowledged data in flight, hold back small writes and coalesce them until an ACK arrives (or a full segment accumulates). The 4-byte header is sent immediately; the 5-byte body is *held* because the header isn't yet ACKed.
2. **Delayed ACK** (RFC 1122): the receiver waits up to ~40 ms hoping to piggyback the ACK on outgoing data. The server can't reply (it has only the header), so it doesn't send data and waits for the delayed-ACK timer.

Deadlock until the timer fires (~40 ms) then the body goes out. Remedies, in order of preference: **write the whole message in one call** (`writev`/`sendmsg` or assemble header+body in one buffer: also fewer syscalls and packets), set **`TCP_NODELAY`** on latency-sensitive sockets (RPC, interactive protocols; costs more small packets), or use `TCP_CORK`/`MSG_MORE` to batch deliberately. `TCP_QUICKACK` changes the receiver side. Bulk-transfer protocols leave Nagle on.

## UDP: datagrams, not streams

UDP preserves message boundaries and has no connection, retransmission, ordering or congestion control. One `recvfrom` returns exactly one datagram; if your buffer is smaller, the excess is **discarded** (`MSG_TRUNC` reports the real length). Zero-length datagrams are legal (unlike TCP, `0` is not EOF):

{{include c/net/udp.c}}

{{run c/net :: gcc -std=gnu11 -O1 -Wall -Wextra udp.c -o udp && ./udp}}

Design rules for UDP protocols (DNS, DHCP, NTP, QUIC, game state): keep datagrams below the path MTU (~1200 bytes for safety, IPv6 minimum) to avoid fragmentation; handle loss, duplication and reordering yourself (sequence numbers, ACKs, retries with backoff); validate source addresses (spoofable: reflection/amplification attacks); `connect()` a UDP socket to filter by peer and receive ICMP errors; size `SO_RCVBUF` for bursts; use `recvmmsg`/GRO for high rates. Broadcast needs `SO_BROADCAST`; multicast uses `IP_ADD_MEMBERSHIP`.

## Concurrency models for servers

| Model | How | Good for | Limits |
|---|---|---|---|
| Iterative | one connection at a time | trivial tools | blocks everyone |
| Process per connection (`fork`) | isolation, simple | security boundaries (sshd, old Apache) | memory, spawn cost |
| **Thread per connection** | as above | hundreds–low thousands of connections | stack memory, scheduling, locks |
| Thread pool + queue | bounded workers | CPU-bound handlers | head-of-line blocking |
| **Event loop** (`epoll`) | one thread multiplexes non-blocking sockets | 10k+ idle connections, proxies | callbacks/state machines; don't block |
| Multi-loop + `SO_REUSEPORT` | one loop per core, kernel balances accept | high-performance servers (nginx, Envoy) | per-core state |
| `io_uring` / DPDK | completion-based / kernel bypass | extreme throughput | complexity |

## Robustness checklist

1. Check **every** return value; handle `EINTR`, `EAGAIN`, `ECONNRESET`, `EPIPE`, `ETIMEDOUT`; close the fd on all paths.
2. **Timeouts everywhere**: `SO_RCVTIMEO/SO_SNDTIMEO`, `poll` with deadlines, non-blocking `connect` + `poll(POLLOUT)` + `SO_ERROR`, application-level heartbeats (TCP keepalive defaults to 2 hours). A server without read timeouts is vulnerable to slowloris.
3. **Bound everything**: maximum message size, connection count, per-client buffers; reject oversized length fields before allocating.
4. **Validate and authenticate** peers (TLS via OpenSSL/BoringSSL/mbedTLS: verify certificate chain and hostname, use modern protocol versions; never roll your own crypto); treat network data as hostile (secure coding note).
5. `SO_REUSEADDR` (server) for restarts; `SO_REUSEPORT` for load balancing; `IPV6_V6ONLY=0` for dual-stack sockets; bind to specific interfaces when appropriate.
6. Graceful close: `shutdown(SHUT_WR)`, drain, then `close`; `SO_LINGER` and RST semantics (unread data on close triggers RST).
7. Observability: `ss -tnpi`, `tcpdump -nn -i lo port 9100`, `strace -f -e trace=network`, `nstat`, application metrics (connections, bytes, latencies).
8. Test the unhappy paths: slow peers, half-open connections, killed clients, resets, large and zero-length messages, fragmented writes (a test client that sends one byte at a time), IPv6.

## Unix-domain sockets and socketpair

`AF_UNIX` sockets (`SOCK_STREAM`/`SOCK_DGRAM`/`SOCK_SEQPACKET`) use filesystem paths or abstract names, faster than loopback TCP, with peer credentials (`SO_PEERCRED`) and fd passing (`SCM_RIGHTS`). `socketpair(AF_UNIX, SOCK_STREAM, 0, sv)` creates a connected pair for parent/child communication. See the IPC note for details.

## Key takeaways

- TCP is a byte stream: loop on partial `send`/`recv`, frame messages (length prefix), cap lengths, and treat `recv == 0` as EOF.
- Use `getaddrinfo` (IPv4/IPv6, iterate results), `SO_REUSEADDR`, `accept4` with `SOCK_CLOEXEC`, `MSG_NOSIGNAL`/ignore `SIGPIPE`, and timeouts for every blocking operation.
- Writing header and body separately triggered Nagle + delayed-ACK: 46 ms vs 0.03 ms per request; send messages in one call or set `TCP_NODELAY`.
- UDP preserves boundaries but not reliability; mind MTU, truncation, spoofing and amplification.
- Choose a concurrency model (threads, epoll loops, `SO_REUSEPORT`) deliberately; bound resources and test failure modes.
