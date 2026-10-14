---
title: "Practice: Watching TCP and UDP Behave"
track: networking
module: Transport and Services
moduleOrder: 4.5
order: 4
level: intermediate
summary: "Four exercises that make transport-layer behaviour visible with a short Python program and ss: the states of a connection from LISTEN to TIME-WAIT, the difference between connection refused and a timeout, why TCP is a byte stream that merges writes while UDP keeps datagram boundaries, and what UDP does when nobody is listening."
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

The transport layer is easiest to understand by watching it. The program below uses only the loopback interface (nothing leaves your machine), opens and closes connections, and asks the kernel what state each end is in with `ss`. Read the output first, then the questions.

```python
import socket, subprocess, threading, time

def ss(filter_):
    out = subprocess.run(["ss", "-tan", filter_], capture_output=True, text=True).stdout.strip().splitlines()
    return [" ".join(l.split()[:1] + l.split()[3:5]) for l in out[1:]]

# 1. the life of a TCP connection as seen by ss
srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", 9100)); srv.listen(5)
print("1. after listen():                     ", ss("sport = :9100"))
cli = socket.create_connection(("127.0.0.1", 9100))
conn, _ = srv.accept()
print("   after connect() and accept():       ", sorted(ss("sport = :9100 or dport = :9100")))
cli.sendall(b"hello"); print("   server received:", conn.recv(100))
cli.close()                                           # the client closes first
time.sleep(0.2)
print("   client closed first:                ", sorted(ss("sport = :9100 or dport = :9100")))
conn.close(); time.sleep(0.2)
print("   server closed too:                  ", sorted(ss("sport = :9100 or dport = :9100")))
srv.close()

# 2. connection refused versus timeout
try: socket.create_connection(("127.0.0.1", 9101), timeout=1)
except ConnectionRefusedError as e: print("2. nothing listening ->", type(e).__name__, "(an immediate RST from the other side)")

# 3. TCP is a byte stream: message boundaries are NOT preserved
srv = socket.socket(); srv.bind(("127.0.0.1", 9102)); srv.listen(1)
cli = socket.create_connection(("127.0.0.1", 9102)); conn, _ = srv.accept()
cli.sendall(b"AAAA"); cli.sendall(b"BBBB"); cli.sendall(b"CCCC"); time.sleep(0.1)
print("3. three send() calls of 4 bytes, one recv() returned:", conn.recv(100))
# UDP keeps boundaries
u1 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); u1.bind(("127.0.0.1", 9103))
u2 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
for m in (b"AAAA", b"BBBB", b"CCCC"): u2.sendto(m, ("127.0.0.1", 9103))
time.sleep(0.1)
print("   three UDP sendto() calls -> recvfrom() returns:", [u1.recvfrom(100)[0] for _ in range(3)])
# UDP has no connection: sending to a closed port is silently lost, and a later recv would just wait
u2.sendto(b"lost", ("127.0.0.1", 9199)); print("   UDP send to a port nobody uses: no error at the sender")
```

```text
$ python3 transport.py
1. after listen():                      ['LISTEN 127.0.0.1:9100 0.0.0.0:*']
   after connect() and accept():        ['ESTAB 127.0.0.1:36490 127.0.0.1:9100', 'ESTAB 127.0.0.1:9100 127.0.0.1:36490', 'LISTEN 127.0.0.1:9100 0.0.0.0:*', 'TIME-WAIT 127.0.0.1:46968 127.0.0.1:9100']
   server received: b'hello'
   client closed first:                 ['CLOSE-WAIT 127.0.0.1:9100 127.0.0.1:36490', 'FIN-WAIT-2 127.0.0.1:36490 127.0.0.1:9100', 'LISTEN 127.0.0.1:9100 0.0.0.0:*', 'TIME-WAIT 127.0.0.1:46968 127.0.0.1:9100']
   server closed too:                   ['LISTEN 127.0.0.1:9100 0.0.0.0:*', 'TIME-WAIT 127.0.0.1:36490 127.0.0.1:9100', 'TIME-WAIT 127.0.0.1:46968 127.0.0.1:9100']
2. nothing listening -> ConnectionRefusedError (an immediate RST from the other side)
3. three send() calls of 4 bytes, one recv() returned: b'AAAABBBBCCCC'
   three UDP sendto() calls -> recvfrom() returns: [b'AAAA', b'BBBB', b'CCCC']
   UDP send to a port nobody uses: no error at the sender
```

(The client's port number, 46968 here, is chosen at random each run.)

## Exercises

**1. Read the connection states.** List the state of each end after `listen()`, after the connection is established, after the **client** closes, and after the server closes too. Which side ends in `TIME-WAIT`, what is it for, and how long does it last?

**2. Refused or timed out?** Section 2 connects to a port where nothing listens. What exception appears and what packet caused it? How does that differ from connecting to an address that drops packets?

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
| after `connect()` and `accept()` | `ESTAB` | `ESTAB` (client port 46968) |
| client closed first | `CLOSE-WAIT` | `FIN-WAIT-2` |
| server closed as well | (gone) | `TIME-WAIT` |

`LISTEN` is a socket waiting for connection requests; the three-way handshake (SYN, SYN-ACK, ACK) has already finished by the time `accept()` returns, and both ends are `ESTAB` (established). The side that closes **first** (here the client) sends a FIN and waits: `FIN-WAIT-2` means "my FIN was acknowledged, now waiting for the peer to close". The peer, having received the FIN, is in **`CLOSE-WAIT`**: it knows the other side is done and the *application must now call `close()`*. A server with many sockets stuck in CLOSE-WAIT has a bug (it forgets to close connections), whereas TIME-WAIT is normal. Once the server also closes, the client sends the last ACK and enters **`TIME-WAIT`**, where it stays for **2 × MSL** (60 seconds on Linux). Its job is to absorb stray delayed packets from the old connection so they cannot be mistaken for part of a new one on the same address pair, and to re-send the final ACK if it was lost. Many TIME-WAIT sockets on a busy *client* or proxy can exhaust ephemeral ports; the cure is connection reuse (keep-alive, pooling), not shortening the timer. `ss -tan state time-wait | wc -l` counts them.

**2.** `ConnectionRefusedError`. The `connect()` sent a **SYN** to a port with no listener, and the kernel on the other end answered immediately with a **RST** ("reset"): there is no service here. That reply proves the host is up and reachable, so refusal points to a stopped service or a wrong port, as in *Troubleshooting a Network Problem Step by Step*. If instead a firewall **drops** the SYN, or the address does not exist, there is **no reply at all** and the client keeps retrying the SYN with growing delays until its timeout (a timeout error: after about two minutes with the kernel's default of six SYN retries, or sooner if the application sets its own limit). Refusal is instant, a timeout takes as long as the timeout: the speed of the failure is a diagnostic.

**3.** TCP delivered `b'AAAABBBBCCCC'` in a single `recv()`: **TCP is a byte stream with no message boundaries**. Three `send()` calls may arrive as one read, or one call may be split across several reads, depending on timing, buffer sizes and the Nagle algorithm; the receiver must find the message limits itself. UDP delivered `[b'AAAA', b'BBBB', b'CCCC']`: **one datagram per `sendto()`, each read returning exactly one**, boundaries preserved. Consequences: a protocol on TCP needs **framing**: a length prefix (`4-byte length, then payload`), a delimiter (a newline in HTTP/1 headers) or a fixed size, and reading must loop until a whole message has arrived (see *Practice: Bits, I/O and Byte Order* for the loop). A protocol on UDP gets framing for free but must handle what TCP hides: **loss, duplication, reordering**, a maximum datagram size (about 1,472 bytes of payload on a standard Ethernet path before IP fragmentation) and the lack of congestion control. That is why DNS (small request/response) uses UDP, and why QUIC rebuilds reliability, ordering and congestion control on top of UDP (*TLS, Certificates and QUIC*).

**4.** Nothing: the `sendto()` returned normally and no exception appeared, although the datagram went nowhere. UDP is **connectionless**: there is no handshake, no acknowledgement and no connection state. (When the destination host is up but the port is closed, it usually returns an ICMP "port unreachable" message; on a Linux socket that has been `connect()`ed to the peer this surfaces as an error on a *later* call, but an unconnected socket like this one never hears about it, and firewalls often suppress the ICMP message entirely.) An application using UDP must supply its own liveness logic: **timeouts and retransmission** (DNS clients retry after a few seconds), **sequence numbers** to detect loss and reordering, and **heartbeats** to notice a vanished peer. If you need reliability without writing it yourself, use TCP; if you need low latency with application-controlled recovery (voice, games, DNS, QUIC), use UDP.

</details>

## Key takeaways

1. A connection moves through LISTEN, ESTAB, FIN-WAIT, CLOSE-WAIT and TIME-WAIT; many CLOSE-WAIT sockets mean the application forgot to close, while TIME-WAIT (60 s on Linux, on the side that closed first) is normal.
2. Refused is an instant RST from a reachable host; a timeout means packets are being dropped or the address is dead.
3. TCP is a byte stream (applications must frame messages); UDP preserves datagram boundaries but gives no reliability or order.
4. UDP senders get no error for lost datagrams; applications must add timeouts, retries and heartbeats.
