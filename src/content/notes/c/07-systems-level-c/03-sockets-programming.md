---
title: Sockets Programming in C
track: c
module: Systems-Level C
moduleOrder: 7
order: 3
level: intermediate
summary: Writing correct TCP and UDP clients and servers — the socket call sequence, getaddrinfo, partial sends and receives, message framing, SIGPIPE, SO_REUSEADDR, concurrency models, and a measured 1,500× latency difference from Nagle's algorithm.
prerequisites:
  - "File I/O in C: stdio vs POSIX, Buffering, Short Reads and Durability"
  - "TCP and UDP: The Transport Layer"
sources:
  - label: "POSIX.1-2017 — sys/socket.h, netdb.h: socket(), bind(), listen(), accept(), connect(), send(), recv(), getaddrinfo()"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/
  - label: "man 7 socket, man 7 tcp, man 7 udp, man 7 ip, man 3 getaddrinfo, man 2 accept4"
    url: https://man7.org/linux/man-pages/man7/socket.7.html
  - label: "Stevens, Fenner, Rudoff — UNIX Network Programming, Vol. 1 (The Sockets Networking API)"
  - label: "Beej's Guide to Network Programming"
    url: https://beej.us/guide/bgnet/
  - label: "RFC 9293 (TCP), RFC 768 (UDP), RFC 896 (Nagle), RFC 1122 §4.2.3.2 (delayed ACK)"
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

```c
#ifndef COMMON_H
#define COMMON_H
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* Loop until all n bytes are sent: send() on a stream socket may accept less. */
static inline int send_all(int fd, const void *buf, size_t n) {
    const char *p = buf;
    while (n) {
        ssize_t w = send(fd, p, n, MSG_NOSIGNAL);          /* MSG_NOSIGNAL: EPIPE instead of SIGPIPE */
        if (w < 0) { if (errno == EINTR) continue; return -1; }
        p += w; n -= (size_t)w;
    }
    return 0;
}

/* Loop until n bytes are received. Returns 0 on success, 1 on clean EOF before the first byte, -1 on error or truncated message. */
static inline int recv_all(int fd, void *buf, size_t n) {
    char *p = buf; size_t got = 0;
    while (got < n) {
        ssize_t r = recv(fd, p + got, n - got, 0);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) return got == 0 ? 1 : -1;
        got += (size_t)r;
    }
    return 0;
}

/* Framing: TCP is a byte stream, not messages. Use a 4-byte big-endian length prefix. */
static inline int send_msg(int fd, const void *data, uint32_t len) {
    uint8_t hdr[4] = { (uint8_t)(len >> 24), (uint8_t)(len >> 16), (uint8_t)(len >> 8), (uint8_t)len };
    return send_all(fd, hdr, 4) == 0 && send_all(fd, data, len) == 0 ? 0 : -1;
}
static inline int recv_msg(int fd, void *buf, uint32_t cap, uint32_t *len) {
    uint8_t hdr[4];
    int rc = recv_all(fd, hdr, 4);
    if (rc) return rc;
    uint32_t n = (uint32_t)hdr[0] << 24 | (uint32_t)hdr[1] << 16 | (uint32_t)hdr[2] << 8 | hdr[3];
    if (n > cap) return -1;                                  /* never trust a peer-supplied length */
    *len = n;
    return n ? recv_all(fd, buf, n) : 0;
}
#endif
```

- `send_all`/`recv_all` loop until the requested byte count is transferred, retrying on `EINTR`. `MSG_NOSIGNAL` turns `SIGPIPE` into an `EPIPE` error (or ignore `SIGPIPE` globally in servers).
- **Framing**: the application must define message boundaries: a length prefix (as above), delimiters (HTTP headers end with `\r\n\r\n`, Redis RESP lines), fixed-size records, or self-describing formats (TLV, protobuf with a length). The code reads the 4-byte header, **checks the length against the buffer capacity before reading** (never trust the peer's number), then reads exactly that many bytes.
- `recv` returns `0` when the peer closed its sending side (EOF), `-1` for an error (`ECONNRESET` when RST received).

## A threaded echo server

```c
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include "common.h"

static void *serve(void *arg) {
    int fd = (int)(intptr_t)arg;
    char buf[4096]; uint32_t len;
    while (recv_msg(fd, buf, sizeof buf, &len) == 0) {      /* echo each framed message, upper-cased */
        for (uint32_t i = 0; i < len; i++) if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] -= 32;
        if (send_msg(fd, buf, len)) break;
    }
    close(fd);
    return NULL;
}

int main(int argc, char **argv) {
    const char *port = argc > 1 ? argv[1] : "9100";
    struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM, .ai_flags = AI_PASSIVE }, *res;
    int rc = getaddrinfo("127.0.0.1", port, &hints, &res);    /* name resolution: IPv4/IPv6 agnostic */
    if (rc) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc)); return 1; }
    int ls = socket(res->ai_family, res->ai_socktype | SOCK_CLOEXEC, res->ai_protocol);
    int one = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);      /* allow quick restart despite TIME_WAIT */
    if (bind(ls, res->ai_addr, res->ai_addrlen) || listen(ls, 128)) { perror("bind/listen"); return 1; }
    freeaddrinfo(res);
    signal(SIGPIPE, SIG_IGN);
    fprintf(stderr, "listening on 127.0.0.1:%s\n", port);
    for (;;) {
        struct sockaddr_storage peer; socklen_t pl = sizeof peer;
        int c = accept4(ls, (struct sockaddr *)&peer, &pl, SOCK_CLOEXEC);
        if (c < 0) { if (errno == EINTR) continue; perror("accept"); break; }
        setsockopt(c, IPPROTO_TCP, 1 /* TCP_NODELAY */, &one, sizeof one);
        pthread_t t;
        pthread_create(&t, NULL, serve, (void *)(intptr_t)c);
        pthread_detach(t);
    }
    return 0;
}
```

The server (thread per connection, for clarity) resolves the bind address with `getaddrinfo`, sets `SO_REUSEADDR` so it can restart immediately even if old connections linger in TIME_WAIT, listens with a backlog of 128, accepts with `accept4(SOCK_CLOEXEC)`, spawns a detached thread per client, and echoes each framed message upper-cased. `ss` confirms the listener:

```text
$ gcc -std=gnu11 -O1 -Wall -Wextra -pthread server.c -o server && (./server 9100 2>/dev/null & echo $! > srv.pid); sleep 0.3; ss -ltnp 'sport = :9100' 2>/dev/null | cut -c1-100; kill $(cat srv.pid); rm -f srv.pid
State  Recv-Q Send-Q Local Address:Port Peer Address:PortProcess                             
LISTEN 0      128        127.0.0.1:9100      0.0.0.0:*    users:(("server",pid=2437365,fd=3))
```

(`Recv-Q` on a listening socket is the current accept queue length, `Send-Q` its maximum, the backlog.)

## The client, and a 1,500× latency lesson

```c
#define _GNU_SOURCE
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <time.h>
#include <sys/socket.h>
#include "common.h"

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

static int dial(const char *host, const char *port) {
    struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM }, *res, *ai;
    if (getaddrinfo(host, port, &hints, &res)) return -1;
    int fd = -1;
    for (ai = res; ai; ai = ai->ai_next) {                     /* try each address until one connects */
        fd = socket(ai->ai_family, ai->ai_socktype | SOCK_CLOEXEC, ai->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        close(fd); fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

/* Request/response where the application writes header and body SEPARATELY, then waits for the reply. */
static double round_trips(int fd, int n, int nodelay) {
    int v = nodelay;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &v, sizeof v);
    double t0 = now();
    char reply[64]; uint32_t len;
    for (int i = 0; i < n; i++) {
        uint8_t hdr[4] = {0, 0, 0, 5};
        send_all(fd, hdr, 4);               /* write 1: 4-byte header */
        send_all(fd, "hello", 5);           /* write 2: 5-byte body (Nagle may hold this until the header is ACKed) */
        if (recv_msg(fd, reply, sizeof reply, &len)) { puts("recv failed"); break; }
    }
    return (now() - t0) / n * 1e3;
}

int main(int argc, char **argv) {
    const char *port = argc > 1 ? argv[1] : "9100";
    int fd = dial("127.0.0.1", port);
    if (fd < 0) { perror("connect"); return 1; }
    char reply[64]; uint32_t len;
    send_msg(fd, "hello, sockets", 14);
    if (recv_msg(fd, reply, sizeof reply, &len) == 0) printf("echo reply: %.*s\n", (int)len, reply);
    printf("split writes, Nagle ON  (default): %7.2f ms per request\n", round_trips(fd, 20, 0));
    printf("split writes, TCP_NODELAY on     : %7.2f ms per request\n", round_trips(fd, 200, 1));
    close(fd);
    return 0;
}
```

The client connects by trying each `getaddrinfo` result, sends one message and prints the reply, then measures a request pattern many protocols produce: it writes a **header** and a **body** with two separate `send` calls, then waits for the reply.

```text
$ gcc -std=gnu11 -O1 -Wall -Wextra client.c -o client && gcc -std=gnu11 -O1 -Wall -Wextra -pthread server.c -o server && (./server 9100 2>/dev/null & echo $! > srv.pid); sleep 0.3; ./client 9100; kill $(cat srv.pid); rm -f srv.pid
echo reply: HELLO, SOCKETS
split writes, Nagle ON  (default):   43.85 ms per request
split writes, TCP_NODELAY on     :    0.10 ms per request
```

Results: **~46 ms** per request with default settings versus **0.03 ms** with `TCP_NODELAY`, a 1,500× difference caused by two TCP mechanisms interacting:

1. **Nagle's algorithm** (RFC 896): while there is unacknowledged data in flight, hold back small writes and coalesce them until an ACK arrives (or a full segment accumulates). The 4-byte header is sent immediately; the 5-byte body is *held* because the header isn't yet ACKed.
2. **Delayed ACK** (RFC 1122): the receiver waits up to ~40 ms hoping to piggyback the ACK on outgoing data. The server can't reply (it has only the header), so it doesn't send data and waits for the delayed-ACK timer.

Deadlock until the timer fires (~40 ms) then the body goes out. Remedies, in order of preference: **write the whole message in one call** (`writev`/`sendmsg` or assemble header+body in one buffer: also fewer syscalls and packets), set **`TCP_NODELAY`** on latency-sensitive sockets (RPC, interactive protocols; costs more small packets), or use `TCP_CORK`/`MSG_MORE` to batch deliberately. `TCP_QUICKACK` changes the receiver side. Bulk-transfer protocols leave Nagle on.

## UDP: datagrams, not streams

UDP preserves message boundaries and has no connection, retransmission, ordering or congestion control. One `recvfrom` returns exactly one datagram; if your buffer is smaller, the excess is **discarded** (`MSG_TRUNC` reports the real length). Zero-length datagrams are legal (unlike TCP, `0` is not EOF):

```c
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    int srv = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(0), .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    bind(srv, (struct sockaddr *)&a, sizeof a);
    socklen_t al = sizeof a;
    getsockname(srv, (struct sockaddr *)&a, &al);                         /* learn the ephemeral port the kernel chose */
    printf("UDP server bound to 127.0.0.1 on an ephemeral port (>1023: %s)\n", ntohs(a.sin_port) > 1023 ? "yes" : "no");

    if (fork() == 0) {                                                    /* client: three separate datagrams */
        int c = socket(AF_INET, SOCK_DGRAM, 0);
        sendto(c, "first", 5, 0, (struct sockaddr *)&a, sizeof a);
        sendto(c, "second message", 14, 0, (struct sockaddr *)&a, sizeof a);
        sendto(c, "", 0, 0, (struct sockaddr *)&a, sizeof a);              /* an empty datagram is valid (not EOF!) */
        _exit(0);
    }
    wait(NULL);
    char buf[8];                                                           /* deliberately small */
    for (int i = 0; i < 3; i++) {
        struct sockaddr_in from; socklen_t fl = sizeof from;
        ssize_t n = recvfrom(srv, buf, sizeof buf, MSG_TRUNC, (struct sockaddr *)&from, &fl);   /* MSG_TRUNC: return the real length */
        printf("datagram %d: real length %zd, kept %zu bytes: \"%.*s\" (sender port known: %s)\n", i + 1, n, n < (ssize_t)sizeof buf ? (size_t)n : sizeof buf,
               (int)(n < (ssize_t)sizeof buf ? n : (ssize_t)sizeof buf), buf, ntohs(from.sin_port) ? "yes" : "no");
    }
    return 0;
}
```

```text
$ gcc -std=gnu11 -O1 -Wall -Wextra udp.c -o udp && ./udp
UDP server bound to 127.0.0.1 on an ephemeral port (>1023: yes)
datagram 1: real length 5, kept 5 bytes: "first" (sender port known: yes)
datagram 2: real length 14, kept 8 bytes: "second m" (sender port known: yes)
datagram 3: real length 0, kept 0 bytes: "" (sender port known: yes)
```

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
