---
title: "How the Internet Works: From a Browser Click to a Web Page"
track: networking
module: Networking Essentials
moduleOrder: 0.5
order: 1
level: beginner
summary: "Following one web request end to end: the URL, DNS lookup, the TCP handshake, the TLS handshake, the HTTP request and response with status codes, and the layers each step belongs to, using real dig and curl output so that every later networking note has a place to attach."
prerequisites:
  - What Is a Network?
sources:
  - label: "RFC 9110 — HTTP Semantics"
    url: https://www.rfc-editor.org/rfc/rfc9110
  - label: "RFC 1034 — Domain Names: Concepts and Facilities"
    url: https://www.rfc-editor.org/rfc/rfc1034
  - label: "RFC 9293 — Transmission Control Protocol (TCP)"
    url: https://www.rfc-editor.org/rfc/rfc9293
  - label: "RFC 8446 — The Transport Layer Security (TLS) Protocol Version 1.3"
    url: https://www.rfc-editor.org/rfc/rfc8446
  - label: "Kurose & Ross — Computer Networking: A Top-Down Approach, Ch. 1-2"
---

You type `https://example.com/` and a page appears a fraction of a second later. In between, your computer and several others exchange dozens of messages across a handful of protocols, each doing one job. This note follows that one request from start to finish, so you can attach each later note (addressing, routing, TCP, DNS, TLS) to a place in the story. Every number below comes from a real run against `example.com`.

## The cast of characters

- **Client**: your browser or `curl`, on your computer.
- **Server**: a computer somewhere that holds the page and answers requests.
- **Router**: a device that forwards packets between networks (your home router is the first one).
- **DNS resolver**: a server that turns names into addresses (usually run by your network or ISP).
- **Packets**: all data is chopped into small chunks (a typical limit is about 1,500 bytes) and each travels independently; the pieces are reassembled at the other end.

## The URL

`https://example.com:443/index.html?q=1` has parts: a **scheme** (`https`, which also picks the protocol and the default port), a **host** (`example.com`), an optional **port** (`:443`, implied by `https`), a **path** (`/index.html`) and a **query** (`?q=1`). The host is a *name*; the network needs an *address*. So the first step is not talking to the server at all.

## Step 1: DNS, from name to address

The client asks its DNS resolver "what is the address of `example.com`?". The resolver, if it does not already have the answer cached, asks the servers responsible for that name and returns the result. You can ask directly with `dig`:

```text
$ dig example.com A +noall +answer
example.com.		294	IN	A	104.20.23.154
example.com.		294	IN	A	172.66.147.243
```

Reading a line: the name, a **TTL** of 294 seconds (how long the answer may be cached; this entry has about five minutes left), the class `IN` (Internet), the record type `A` (an IPv4 address) and the address. There are two addresses, so the site can be served from more than one machine, and clients pick one. The name also has `AAAA` records for IPv6 addresses (curl reported `2606:4700:...` ones). Other record types include `MX` (mail servers) and `NS` (the servers responsible for the name):

```text
$ dig +short NS example.com
hera.ns.cloudflare.com.
elliott.ns.cloudflare.com.
$ dig +short MX gmail.com | head -2
30 alt3.gmail-smtp-in.l.google.com.
10 alt1.gmail-smtp-in.l.google.com.
```

DNS normally uses UDP port 53 and is covered in *DNS: Names, Resolvers and Records*.

## Step 2: finding the way, IP addressing and routing

The client now has a destination **IP address** (say `104.20.23.154`). It compares it with its own network (see *IPv4 Addressing, Subnetting, VLSM and CIDR*): this address is outside the local network, so the packet is handed to the **default gateway**, the home router. The router looks at the destination address and forwards the packet towards the next router, which does the same, hop by hop, until it arrives (*Forwarding: Routing Table, Longest-Prefix Match, Policy Routing and ECMP*). On your local link the packet travels inside an **Ethernet frame** addressed to the next device's MAC address, found with ARP (*Ethernet Frames and MAC Learning*, *ARP and IPv4 Address Conflict Detection*). Addresses (IP) say *where the packet is going end to end*; MAC addresses say *which neighbour gets it next*.

## Step 3: TCP, a reliable connection

IP only delivers individual packets: they may arrive late, out of order, duplicated or not at all. Web pages need all the bytes in order, so the client opens a **TCP** connection to the server's port 443 (the standard port for HTTPS). Opening takes a **three-way handshake**:

1. Client → server: **SYN** ("I want to talk; my sequence numbers start at ...").
2. Server → client: **SYN-ACK** ("agreed; mine start at ...").
3. Client → server: **ACK** ("good, we are connected").

From then on TCP numbers every byte, acknowledges what arrives, retransmits what is lost and slows down when the network is congested (details in *TCP and UDP: The Transport Layer*). The **port number** identifies which program on the machine should receive the data, as the address identifies the machine: `443` is a web server, `22` is SSH, `53` is DNS.

## Step 4: TLS, making it private

With `https`, before any web data is sent the two sides run a **TLS handshake** to agree on keys and to let the client **verify the server's identity** using its certificate (signed by an authority your system trusts). Afterwards everything is encrypted, so someone on the Wi-Fi or at the ISP sees only that you talked to this server, not the pages or passwords. `curl -v` shows the handshake:

```text
$ curl -sv -o /dev/null https://example.com 2>&1 | grep -E "^\* (Host|IPv|ALPN|TLSv|SSL connection|Connected)"
* Host example.com:443 was resolved.
* IPv4: 172.66.147.243, 104.20.23.154
* ALPN: curl offers h2,http/1.1
* TLSv1.3 (OUT), TLS handshake, Client hello (1):
* TLSv1.3 (IN), TLS handshake, Server hello (2):
* TLSv1.3 (IN), TLS handshake, Encrypted Extensions (8):
* TLSv1.3 (IN), TLS handshake, Certificate (11):
* TLSv1.3 (IN), TLS handshake, CERT verify (15):
* TLSv1.3 (IN), TLS handshake, Finished (20):
* TLSv1.3 (OUT), TLS handshake, Finished (20):
* SSL connection using TLSv1.3 / TLS_AES_256_GCM_SHA384 / X25519MLKEM768 / id-ecPublicKey
* ALPN: server accepted h2
* Connected to example.com (172.66.147.243) port 443
```

`Certificate` carries the server's identity; `CERT verify` proves it owns the matching key; `Finished` confirms nothing was tampered with. **ALPN** negotiated the HTTP version (`h2` is HTTP/2). More in *TLS, Certificates and QUIC*.

## Step 5: HTTP, the actual request

Now the client sends an **HTTP request**: a method, a path and headers. The server answers with a **status code**, headers and the body (the HTML). Over plain HTTP you can read it literally:

```text
$ curl -sI http://example.com | head -3
HTTP/1.1 200 OK
Date: Wed, 14 Oct 2026 05:20:16 GMT
Content-Type: text/html; charset=utf-8
```

The **status code** families: `2xx` success (`200 OK`), `3xx` redirect (`301`/`302`: look somewhere else; for example `http://github.com` answers by redirecting to `https://github.com/`), `4xx` the client erred (`404 Not Found`, `403 Forbidden`), `5xx` the server failed (`500`, `503`). The **method** says what to do: `GET` fetch, `POST` submit, `PUT`/`DELETE` change or remove. The browser then reads the HTML, discovers it needs images, scripts and stylesheets, and repeats steps 1-5 (reusing connections where possible) for each.

## How long does each step take?

`curl` can report cumulative timing for each phase:

```text
$ curl -sS -o /dev/null -w "dns=%{time_namelookup} connect=%{time_connect} tls=%{time_appconnect} total=%{time_total}\n" https://example.com
dns=0.001669 connect=0.040252 tls=0.089515 total=0.135651
```

The numbers are cumulative seconds. DNS finished after 2 ms because the answer was already cached (an uncached lookup took about 40 ms in an earlier run); the TCP connection completed at 40 ms, which is one round trip to the server; TLS finished at 90 ms, about one more round trip plus computation; and the whole request took 136 ms. **Latency**, the round-trip delay, dominates: every handshake costs at least one round trip, which is why connection reuse, caching and nearby servers (CDNs) matter more for page speed than raw bandwidth. (Your numbers will differ from run to run.)

## The layers, tied together

| Step | Layer | Protocol | Addressing | Later note |
|---|---|---|---|---|
| Name to address | Application | DNS (UDP 53) | names | *DNS: Names, Resolvers and Records* |
| Encryption and identity | Application/session | TLS | certificates | *TLS, Certificates and QUIC* |
| Page request | Application | HTTP | URLs | (this note) |
| Reliable delivery | Transport | TCP (or QUIC over UDP) | ports | *TCP and UDP: The Transport Layer* |
| End-to-end delivery | Network | IP | IP addresses | *IPv4 Header, Fragmentation and MTU* |
| Next-hop delivery | Link | Ethernet / Wi-Fi | MAC addresses | *Ethernet Frames and MAC Learning* |

Each layer wraps the layer above it in its own header (**encapsulation**), as described in *Network Models, Encapsulation and Device Roles*. When troubleshooting, ask "which step failed?": no address (DNS), no route (IP), connection refused or timed out (TCP, firewall), certificate warning (TLS), a `4xx`/`5xx` code (HTTP).

## Pitfalls

- **Blaming "the internet" for a DNS problem.** If an IP address works and the name does not, the resolver is at fault.
- **Assuming no reply means the server is down.** Firewalls drop packets silently; a timeout and a "connection refused" mean different things.
- **Confusing HTTPS with "safe"**: it means the connection is private and the server's name was verified, not that the site is trustworthy.
- **Ignoring caching**: a changed DNS record can take up to its TTL to reach everyone.
- **Mixing up IP and MAC addresses**: IP identifies the destination across networks; MAC only the next neighbour.

## Key takeaways

1. A web request is a sequence: DNS (name to address), routing (hop by hop), TCP handshake (reliable connection), TLS handshake (privacy and identity), HTTP (request and response).
2. IP addresses say where a packet goes end to end; MAC addresses say the next hop; ports say which program receives the data.
3. Each handshake costs at least one round trip, so latency matters as much as bandwidth.
4. HTTP status codes group into 2xx success, 3xx redirect, 4xx client error and 5xx server error.
5. When something fails, find the step that failed (DNS, route, TCP, TLS or HTTP) instead of guessing.
