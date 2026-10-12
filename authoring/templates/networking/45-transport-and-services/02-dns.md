---
title: "DNS: Names, Resolvers and Records"
track: networking
module: Transport and Services
moduleOrder: 4.5
order: 2
level: intermediate
summary: "How a name becomes an address: the hierarchy of root, TLD and authoritative servers, stub and recursive resolvers, record types, TTL caching and negative caching, the wire format byte by byte, a working root-to-authoritative lab queried with a real resolver and with dig, plus DNSSEC, DoT/DoH, UDP/TCP behavior and the Linux resolver stack."
prerequisites:
  - What Is a Network?
  - "TCP and UDP: The Transport Layer"
sources:
  - label: RFC 1034 — Domain Names, Concepts and Facilities
    url: https://www.rfc-editor.org/rfc/rfc1034
  - label: RFC 1035 — Domain Names, Implementation and Specification
    url: https://www.rfc-editor.org/rfc/rfc1035
  - label: RFC 2308 — Negative Caching of DNS Queries (DNS NCACHE)
    url: https://www.rfc-editor.org/rfc/rfc2308
  - label: RFC 6891 — Extension Mechanisms for DNS (EDNS(0))
    url: https://www.rfc-editor.org/rfc/rfc6891
  - label: RFC 4033 — DNS Security Introduction and Requirements
    url: https://www.rfc-editor.org/rfc/rfc4033
  - label: RFC 7858 — DNS over TLS; RFC 8484 — DNS over HTTPS
    url: https://www.rfc-editor.org/rfc/rfc7858
  - label: RFC 9499 — DNS Terminology
    url: https://www.rfc-editor.org/rfc/rfc9499
  - label: RFC 6761 — Special-Use Domain Names (.test, .invalid, .localhost, .example)
    url: https://www.rfc-editor.org/rfc/rfc6761
  - label: nsswitch.conf(5), resolv.conf(5), getaddrinfo(3) — Linux manual pages
    url: https://man7.org/linux/man-pages/man5/resolv.conf.5.html
---

People use names; packets use addresses. The **Domain Name System** is the distributed, hierarchical database that maps one to the other (and holds a lot more: mail routing, service locations, verification tokens). Almost every connection starts with a DNS lookup, so DNS failures look like "the network is down", and DNS is the first thing to check when a name does not work but an address does.

## The hierarchy

A name such as `www.example.test.` is read right to left: the final dot is the **root**; `test` is a **top-level domain (TLD)**; `example.test` is a **zone** an organisation controls; `www` is a host in it. The tree is divided into **zones**, each served by **authoritative name servers** that hold the real data for their part of the tree and **delegate** the rest by naming the servers of the child zone (an `NS` record). No server holds everything:

- the **root servers** know where to find each TLD's servers;
- the **TLD servers** (`com`, `org`, `uk`...) know where to find the servers of each registered domain;
- the domain's **authoritative servers** answer for names inside it.

## The three roles

| Role | What it does | Example |
|---|---|---|
| **Stub resolver** | the tiny client in your operating system; asks one configured resolver and trusts the answer | `getaddrinfo()` in libc, systemd-resolved |
| **Recursive resolver** | does the work: walks the hierarchy, caches results, returns the final answer | your ISP's or company's resolver, `8.8.8.8`, `unbound`, `bind` |
| **Authoritative server** | the source of truth for a zone; does not recurse | the servers named in the zone's `NS` records |

A lookup from a laptop: the application calls `getaddrinfo("www.example.com")`; the stub resolver sends one UDP query to the recursive resolver (flag `RD`, "recursion desired"); if the answer is not in its cache, the recursive resolver queries a root server, follows the **referral** to the TLD servers, follows another referral to the domain's servers, gets the answer, caches it, and replies.

## A working lab: root, TLD, authoritative, resolver

Reading about referrals is less convincing than watching them. The lab starts three authoritative servers on separate addresses in a throwaway namespace: a root that knows only the `test.` TLD, a TLD server that knows only the delegation of `example.test.`, and the authoritative server for `example.test.` with records (a `CNAME`, `A` and `AAAA` records, `MX` and `TXT`). The `.test` TLD is reserved for exactly this purpose (RFC 6761), so nothing can collide with a real domain. It uses a small Python implementation of the DNS wire format from RFC 1035 (about 60 lines), and then the real `dig` tool is pointed at it as an independent check of the protocol.

{{include networking/dns/dnslib_mini.py}}

{{include networking/dns/servers.py}}

{{include networking/dns/resolver.py}}

{{include networking/dns/lab.sh}}

{{run networking/dns :: unshare -Urnm sh lab.sh 2>&1}}

### The wire format

DNS is a binary protocol, normally one UDP datagram per message. A message is a 12-byte **header** followed by the question and the answer, authority and additional sections.

- **Header**: `12 34` is the query ID (a random number copied into the reply so answers match questions); then a 16-bit **flags** word. The query has flags `0x0000` (a query, no recursion requested, because we are talking to an authoritative server). The response has `0x8400`: bit 15 `QR` = response, bit 10 `AA` = authoritative answer, `RCODE` (low 4 bits) = 0, no error. Then four counts: questions, answers, authority records, additional records.
- **Names** are sequences of **labels**, each a length byte followed by that many characters, ending with a zero byte: `web.example.test.` is `03 web 07 example 04 test 00`. A label is limited to 63 bytes, a whole name to 255. A pointer byte starting with `11` (`c0 xx`) lets a name refer to an earlier part of the message (compression), saving space.
- **Question**: name, type (1 = `A`) and class (1 = `IN`, the Internet).
- **Resource records** (answers): name, type, class, **TTL** (seconds the answer may be cached), a length, and the data (4 bytes for an `A` record).

### What the resolver does

The five lookups show the whole life of an iterative resolver:

1. **Cold cache**: the resolver asks the root, which does not know `www.example.test` but **refers** it to the servers of `test.` (an `NS` record plus a **glue** `A` record giving that server's address, so there is no chicken-and-egg problem finding it); the TLD server refers it to `example.test.`; the authoritative server answers (`aa=1`). Three network queries. The answer includes a `CNAME` (an **alias**: `www` is really `web`) followed by the address records of the target, which the server added in the same answer.
2. **Same name again**: answered from the cache with **no network traffic**. The cache entry lives for the smallest TTL in the answer (60 s here).
3. **Later, after the 60 s TTL expired**: the answer must be fetched again, but the *delegation* (the `NS` record for `example.test.` with TTL 172,800 s = 2 days) is still cached, so the resolver goes **straight to the authoritative server**: 1 query instead of 3. This is why the root servers see far less traffic than the number of users suggests: nearly all lookups need only the last step.
4. **A different record type in the same zone** also goes straight to the authoritative server.
5. **A name that does not exist** returns `NXDOMAIN` (the name is not in the zone at all, different from "the name exists but has no record of that type", which is `NOERROR` with an empty answer, called NODATA). Resolvers cache negative answers too (RFC 2308), bounded by the zone's SOA minimum, so a typo is not re-asked a thousand times.

The final block runs the real `dig`: against the authoritative server it shows `status: NOERROR`, flags `qr aa` (response, authoritative) and the two `A` records; against the root server it shows no answer but an **authority** section with the `NS` referral and an **additional** section with the glue address, exactly the structure our resolver followed. That `dig` parsed our hand-built packets without complaint confirms the encoder follows RFC 1035.

## Record types

| Type | Holds | Notes |
|---|---|---|
| `A` / `AAAA` | an IPv4 / IPv6 address | several records = round-robin DNS |
| `CNAME` | alias to another name | cannot coexist with other records at the same name, so not allowed at the zone apex |
| `NS` | the name servers of a zone | the delegation |
| `SOA` | zone metadata: primary server, serial, refresh/retry/expire, negative-cache TTL | one per zone |
| `MX` | mail servers with preference (lower first) | `10 mail.example.test.` |
| `TXT` | free text | SPF, DKIM, domain verification |
| `PTR` | name for an address (reverse DNS) | under `in-addr.arpa.` (`10.20.30.40` → `40.30.20.10.in-addr.arpa.`) and `ip6.arpa.` |
| `SRV` | host and port of a service | `_sip._tcp.example.com.` |
| `CAA` | which certificate authorities may issue | |
| `DS`, `DNSKEY`, `RRSIG`, `NSEC` | DNSSEC data | below |

## TTL and caching: the trade-off

Every record carries a **TTL**. A long TTL means fewer queries and faster lookups but slow changes (clients keep the old address until it expires); a short TTL allows quick failover but multiplies query load. Operators lower the TTL *before* a planned migration (so caches empty quickly) and raise it afterwards. Caches exist at every level: the application (browsers), the stub resolver, the recursive resolver. A change you make "propagates" only as old cached copies expire; there is no way to force remote caches to forget.

## UDP, TCP, EDNS and size

DNS runs over **UDP port 53** for ordinary queries (one request, one response, no connection set-up) and over **TCP port 53** when a response is too large: the original limit was **512 bytes**; a server sets the **TC (truncated)** flag and the client retries over TCP. **EDNS(0)** (RFC 6891) lets clients advertise a larger UDP size (typically about 1232 bytes to avoid IP fragmentation) and carries options such as DNSSEC support. Zone transfers (`AXFR`) use TCP. Firewalls that allow only UDP/53 break large responses; firewalls that block fragments break big UDP answers; both appear as "DNS works for some names but not others".

## Security and privacy

- **Spoofing and DNSSEC.** A plain DNS answer can be forged by anyone who can inject packets, and caches could then serve poisoned data. **DNSSEC** (RFC 4033 family) signs records (`RRSIG`) with zone keys (`DNSKEY`) chained from the root through `DS` records in each parent: a validating resolver can prove an answer is genuine and that a name does not exist. It authenticates data but does not encrypt it.
- **Randomisation**: random query IDs and random source ports (the Kaminsky attack of 2008 made source-port randomisation standard) make blind forgery hard.
- **Privacy.** Classic DNS queries are visible on the path. **DNS over TLS** (port 853) and **DNS over HTTPS** encrypt the stub-to-resolver hop; **QUIC**-based variants exist too. They hide queries from the network but not from the resolver.
- **Amplification.** Small spoofed UDP queries can trigger large responses toward a victim; authoritative servers apply response-rate limiting, and operators should not run open recursive resolvers.
- **Anycast**: the root servers and large resolvers use the same IP address announced from many sites by BGP, so queries reach the nearest one (see *BGP at Scale*).

## The Linux resolver stack

When a program calls `getaddrinfo()`, glibc consults `/etc/nsswitch.conf` to decide the order of name sources:

{{run networking/dns :: grep -E "^hosts" /etc/nsswitch.conf}}

`files` means `/etc/hosts` first; then the others in order (here mDNS and finally `dns`). The `dns` source reads `/etc/resolv.conf`, which lists up to three `nameserver` addresses, a `search` list of domains appended to short names, and options such as `ndots` (how many dots a name needs before it is tried as-is first; a high `ndots` multiplies lookups, a known cause of slow DNS inside Kubernetes). On modern distributions `systemd-resolved` or NetworkManager manages this file and runs a local caching stub at `127.0.0.53`; `resolvectl status` shows the real upstream servers per interface. Tools to know:

| Tool | Does | Notes |
|---|---|---|
| `getent hosts name` | asks exactly like a program would (nsswitch, `/etc/hosts`, DNS) | the right test of "does the system resolve this?" |
| `dig name TYPE [@server]` | sends a DNS query and prints the raw sections | bypasses nsswitch; `+short`, `+trace`, `+norecurse` |
| `host`, `nslookup` | simpler lookups | |
| `resolvectl query name` | systemd-resolved's view, with cache and DNSSEC state | |

The classic confusion: **`dig` can succeed while the application fails**, because the application uses `nsswitch.conf`, search domains, a local cache, or a stale `/etc/hosts` entry. Always compare `getent hosts` and `dig`.

## Failure modes and troubleshooting

| Symptom | Likely cause | Check |
|---|---|---|
| "Temporary failure in name resolution" | no reachable resolver; wrong `resolv.conf`; firewall blocking UDP/TCP 53 | `resolvectl status`, `dig @IP example.com`, ping the resolver |
| Works with an IP address, not a name | DNS only | `getent hosts`; test another resolver |
| Intermittent slowness (seconds) | first nameserver dead (timeouts, then fallback); `ndots`/search list causing extra queries; AAAA lookup waits | `strace -e connect`, `dig` against each server |
| Old address after a change | cached by a resolver or the application until the TTL expires | `dig +norecurse @auth`, check TTL, flush local caches |
| `NXDOMAIN` for a name that should exist | typo; missing record; split-horizon DNS giving different answers inside and outside; negative caching | query the authoritative servers directly |
| `SERVFAIL` | the resolver could not get an answer: broken delegation, unreachable authoritative servers, DNSSEC validation failure | `dig +cd` (checking disabled) to test DNSSEC, `dig +trace` |
| Large answers fail | UDP fragments blocked, TCP/53 blocked | `dig +tcp`, `+bufsize=1232` |

## Key takeaways

1. DNS is a hierarchy of zones; delegation (`NS` plus glue) links them, and no server knows everything.
2. A stub resolver asks a recursive resolver, which walks root → TLD → authoritative and caches; caching with TTLs is what makes DNS scale.
3. Cached delegations mean most lookups need one query to the authoritative server, not three; negative answers are cached too.
4. The wire format is small and regular: 12-byte header, length-prefixed labels, resource records with TTLs; UDP/53 normally, TCP when answers are large.
5. DNSSEC authenticates answers but does not encrypt; DoT/DoH encrypt the client-to-resolver hop.
6. On Linux, applications resolve through nsswitch, `/etc/hosts` and `resolv.conf`; use `getent hosts` for the application's view and `dig` for the DNS view.
