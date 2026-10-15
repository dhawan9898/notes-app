---
title: Watching TCP and UDP Behave
track: networking
module: Transport and Services
moduleOrder: 4.5
order: 1
level: intermediate
minutes: 35
env: browser
terminal:
  - ip link set lo up
  - ./transport
summary: "Four exercises that make transport-layer behaviour visible with a short C program that reads the kernel's TCP table (/proc/net/tcp, the data `ss` shows): the states of a connection from LISTEN to TIME-WAIT, the difference between connection refused and a timeout, why TCP is a byte stream that merges writes while UDP keeps datagram boundaries, and what UDP does when nobody is listening."
prerequisites:
  - "TCP and UDP: The Transport Layer"
  - "DNS: Names, Resolvers and Records"
  - "TLS, Certificates and QUIC"
sources:
  - label: "RFC 9293 — Transmission Control Protocol (TCP), including the state diagram"
    url: https://www.rfc-editor.org/rfc/rfc9293
  - label: "RFC 768 — User Datagram Protocol"
    url: https://www.rfc-editor.org/rfc/rfc768
  - label: "man 8 ss and man 7 tcp"
    url: https://man7.org/linux/man-pages/man7/tcp.7.html
---

The transport layer is easiest to understand by watching it. The program below uses only the loopback interface (nothing leaves your machine), opens and closes connections, and asks the kernel what state each end is in by reading `/proc/net/tcp` (`ss -tan` shows the same table on a full Linux system). In the in-browser Linux run `ip link set lo up` and then `./transport`. Read the output first, then the questions.

```c
/* Watch TCP and UDP behave on loopback: connection states from /proc/net/tcp, refused connections, byte streams versus datagrams. */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static struct sockaddr_in loop(int port) { struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons((uint16_t)port), .sin_addr.s_addr = htonl(0x7f000001)}; return a; }
static int cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* every TCP socket whose local or remote port is `port`, as "STATE local remote", sorted (the kernel lists them in /proc/net/tcp) */
static void show(const char *label, int port) {
    static const char *names[] = {"?", "ESTAB", "SYN-SENT", "SYN-RECV", "FIN-WAIT-1", "FIN-WAIT-2", "TIME-WAIT", "CLOSE", "CLOSE-WAIT", "LAST-ACK", "LISTEN", "CLOSING"};
    FILE *f = fopen("/proc/net/tcp", "r"); char line[256], *rows[32]; int n = 0;
    fgets(line, sizeof line, f);
    while (fgets(line, sizeof line, f) && n < 32) {
        unsigned la, lp, ra, rp, st; if (sscanf(line, " %*d: %x:%x %x:%x %x", &la, &lp, &ra, &rp, &st) != 5) continue;
        if ((int)lp != port && (int)rp != port) continue;
        char row[96]; snprintf(row, sizeof row, "%s %u.%u.%u.%u:%u %u.%u.%u.%u:%u", names[st < 12 ? st : 0], la & 255, la >> 8 & 255, la >> 16 & 255, la >> 24, lp, ra & 255, ra >> 8 & 255, ra >> 16 & 255, ra >> 24, rp);
        rows[n++] = strdup(row);
    }
    fclose(f); qsort(rows, (size_t)n, sizeof rows[0], cmp);
    printf("%s", label);
    for (int i = 0; i < n; i++) { printf("\n      %s", rows[i]); free(rows[i]); }
    printf("%s\n", n ? "" : " (none)");
}

int main(void) {
    /* 1. the life of a TCP connection */
    int srv = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = loop(9100); bind(srv, (struct sockaddr *)&a, sizeof a); listen(srv, 5);
    show("1. after listen():", 9100);
    int cli = socket(AF_INET, SOCK_STREAM, 0); connect(cli, (struct sockaddr *)&a, sizeof a);
    int conn = accept(srv, NULL, NULL);
    show("   after connect() and accept():", 9100);
    send(cli, "hello", 5, 0); char buf[100]; ssize_t n = recv(conn, buf, sizeof buf, 0);
    printf("   server received: \"%.*s\"\n", (int)n, buf);
    close(cli); usleep(200000);                                                            /* the client closes first */
    show("   client closed first:", 9100);
    close(conn); usleep(200000);
    show("   server closed too:", 9100);
    close(srv);

    /* 2. connection refused versus timeout */
    int c2 = socket(AF_INET, SOCK_STREAM, 0); struct sockaddr_in dead = loop(9101);
    if (connect(c2, (struct sockaddr *)&dead, sizeof dead) < 0) printf("2. nothing listening -> %s (an immediate RST from the other side)\n", strerror(errno));
    close(c2);

    /* 3. TCP is a byte stream: message boundaries are NOT preserved */
    srv = socket(AF_INET, SOCK_STREAM, 0); a = loop(9102); bind(srv, (struct sockaddr *)&a, sizeof a); listen(srv, 1);
    cli = socket(AF_INET, SOCK_STREAM, 0); connect(cli, (struct sockaddr *)&a, sizeof a); conn = accept(srv, NULL, NULL);
    send(cli, "AAAA", 4, 0); send(cli, "BBBB", 4, 0); send(cli, "CCCC", 4, 0); usleep(100000);
    n = recv(conn, buf, sizeof buf, 0);
    printf("3. three send() calls of 4 bytes, one recv() returned: \"%.*s\" (%zd bytes)\n", (int)n, buf, n);
    /* UDP keeps boundaries */
    int u1 = socket(AF_INET, SOCK_DGRAM, 0), u2 = socket(AF_INET, SOCK_DGRAM, 0); struct sockaddr_in ua = loop(9103);
    bind(u1, (struct sockaddr *)&ua, sizeof ua);
    const char *msgs[] = {"AAAA", "BBBB", "CCCC"};
    for (int i = 0; i < 3; i++) sendto(u2, msgs[i], 4, 0, (struct sockaddr *)&ua, sizeof ua);
    usleep(100000);
    printf("   three UDP sendto() calls -> recvfrom() returns:");
    for (int i = 0; i < 3; i++) { n = recvfrom(u1, buf, sizeof buf, 0, NULL, NULL); printf(" \"%.*s\"", (int)n, buf); }
    putchar('\n');
    /* UDP has no connection: sending to a closed port is silently lost */
    struct sockaddr_in nobody = loop(9199); ssize_t sent = sendto(u2, "lost", 4, 0, (struct sockaddr *)&nobody, sizeof nobody);
    printf("   UDP send to a port nobody uses: sendto() returned %zd, no error at the sender\n", sent);
    return 0;
}
```

```text
$ ip link set lo up
$ ./transport
1. after listen():
      LISTEN 127.0.0.1:9100 0.0.0.0:0
   after connect() and accept():
      ESTAB 127.0.0.1:52438 127.0.0.1:9100
      ESTAB 127.0.0.1:9100 127.0.0.1:52438
      LISTEN 127.0.0.1:9100 0.0.0.0:0
   server received: "hello"
   client closed first:
      CLOSE-WAIT 127.0.0.1:9100 127.0.0.1:52438
      FIN-WAIT-2 127.0.0.1:52438 127.0.0.1:9100
      LISTEN 127.0.0.1:9100 0.0.0.0:0
   server closed too:
      LISTEN 127.0.0.1:9100 0.0.0.0:0
      TIME-WAIT 127.0.0.1:52438 127.0.0.1:9100
2. nothing listening -> Connection refused (an immediate RST from the other side)
3. three send() calls of 4 bytes, one recv() returned: "AAAABBBBCCCC" (12 bytes)
   three UDP sendto() calls -> recvfrom() returns: "AAAA" "BBBB" "CCCC"
   UDP send to a port nobody uses: sendto() returned 4, no error at the sender
```

(The client's port number, 46968 here, is chosen at random each run.)

## Exercises

**1. Read the connection states.** List the state of each end after `listen()`, after the connection is established, after the **client** closes, and after the server closes too. Which side ends in `TIME-WAIT`, what is it for, and how long does it last?

**2. Refused or timed out?** Section 2 connects to a port where nothing listens. What error appears and what packet caused it? How does that differ from connecting to an address that drops packets?

**3. Stream versus datagrams.** Section 3 sends three 4-byte messages. What did TCP deliver, what did UDP deliver, and what does this mean for an application protocol built on each?

**4. UDP with nobody listening.** What does a UDP sender learn when it sends to a closed port, and how would an application find out the peer is gone?

## Answers

Try before opening.

<details>
<summary>Show the explanations</summary>

**1. States.**

| Moment | Server side | Client side |
|---|---|---|
| after `listen()` | `LISTEN` on `127.0.0.1:9100` | (no connection yet) |
| after `connect()` and `accept()` | `ESTAB` | `ESTAB` (an ephemeral client port) |
| client closed first | `CLOSE-WAIT` | `FIN-WAIT-2` |
| server closed as well | (gone) | `TIME-WAIT` |

`LISTEN` is a socket waiting for connection requests; the three-way handshake (SYN, SYN-ACK, ACK) has already finished by the time `accept()` returns, and both ends are `ESTAB` (established). The side that closes **first** (here the client) sends a FIN and waits: `FIN-WAIT-2` means "my FIN was acknowledged, now waiting for the peer to close". The peer, having received the FIN, is in **`CLOSE-WAIT`**: it knows the other side is done and the *application must now call `close()`*. A server with many sockets stuck in CLOSE-WAIT has a bug (it forgets to close connections), whereas TIME-WAIT is normal. Once the server also closes, the client sends the last ACK and enters **`TIME-WAIT`**, where it stays for **2 × MSL** (60 seconds on Linux). Its job is to absorb stray delayed packets from the old connection so they cannot be mistaken for part of a new one on the same address pair, and to re-send the final ACK if it was lost. Many TIME-WAIT sockets on a busy *client* or proxy can exhaust ephemeral ports; the cure is connection reuse (keep-alive, pooling), not shortening the timer. `ss -tan state time-wait | wc -l` counts them.

**2.** `connect()` fails with **ECONNREFUSED** ("Connection refused"). The `connect()` sent a **SYN** to a port with no listener, and the kernel on the other end answered immediately with a **RST** ("reset"): there is no service here. That reply proves the host is up and reachable, so refusal points to a stopped service or a wrong port, as in *Troubleshooting a Network Problem Step by Step*. If instead a firewall **drops** the SYN, or the address does not exist, there is **no reply at all** and the client keeps retrying the SYN with growing delays until its timeout (an ETIMEDOUT error: after about two minutes with the kernel's default of six SYN retries, or sooner if the application sets its own limit). Refusal is instant, a timeout takes as long as the timeout: the speed of the failure is a diagnostic.

**3.** TCP delivered `AAAABBBBCCCC` in a single `recv()` (12 bytes): **TCP is a byte stream with no message boundaries**. Three `send()` calls may arrive as one read, or one call may be split across several reads, depending on timing, buffer sizes and the Nagle algorithm; the receiver must find the message limits itself. UDP delivered `AAAA`, `BBBB` and `CCCC` as three separate reads: **one datagram per `sendto()`, each read returning exactly one**, boundaries preserved. Consequences: a protocol on TCP needs **framing**: a length prefix (`4-byte length, then payload`), a delimiter (a newline in HTTP/1 headers) or a fixed size, and reading must loop until a whole message has arrived (see the *Bits, I/O and Byte Order* lab for the loop). A protocol on UDP gets framing for free but must handle what TCP hides: **loss, duplication, reordering**, a maximum datagram size (about 1,472 bytes of payload on a standard Ethernet path before IP fragmentation) and the lack of congestion control. That is why DNS (small request/response) uses UDP, and why QUIC rebuilds reliability, ordering and congestion control on top of UDP (*TLS, Certificates and QUIC*).

**4.** Nothing: the `sendto()` returned the number of bytes and no error was reported, although the datagram went nowhere. UDP is **connectionless**: there is no handshake, no acknowledgement and no connection state. (When the destination host is up but the port is closed, it usually returns an ICMP "port unreachable" message; on a Linux socket that has been `connect()`ed to the peer this surfaces as an error on a *later* call, but an unconnected socket like this one never hears about it, and firewalls often suppress the ICMP message entirely.) An application using UDP must supply its own liveness logic: **timeouts and retransmission** (DNS clients retry after a few seconds), **sequence numbers** to detect loss and reordering, and **heartbeats** to notice a vanished peer. If you need reliability without writing it yourself, use TCP; if you need low latency with application-controlled recovery (voice, games, DNS, QUIC), use UDP.

</details>

## Key takeaways

1. A connection moves through LISTEN, ESTAB, FIN-WAIT, CLOSE-WAIT and TIME-WAIT; many CLOSE-WAIT sockets mean the application forgot to close, while TIME-WAIT (60 s on Linux, on the side that closed first) is normal.
2. Refused is an instant RST from a reachable host; a timeout means packets are being dropped or the address is dead.
3. TCP is a byte stream (applications must frame messages); UDP preserves datagram boundaries but gives no reliability or order.
4. UDP senders get no error for lost datagrams; applications must add timeouts, retries and heartbeats.
