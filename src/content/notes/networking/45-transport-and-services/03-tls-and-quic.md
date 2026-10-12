---
title: "TLS, Certificates and QUIC"
track: networking
module: Transport and Services
moduleOrder: 4.5
order: 3
level: advanced
summary: "What TLS provides and how the handshake works, certificate chains inspected with openssl, forward secrecy, TLS 1.2 versus 1.3 versus resumption measured in round trips over a 40 ms path (and a Nagle trap), ALPN and SNI, mutual TLS, QUIC and HTTP/3, and a failure-mode table."
prerequisites:
  - "TCP and UDP: The Transport Layer"
  - "DNS: Names, Resolvers and Records"
sources:
  - label: "RFC 8446 — The Transport Layer Security (TLS) Protocol Version 1.3"
    url: https://www.rfc-editor.org/rfc/rfc8446
  - label: "RFC 5246 — TLS Version 1.2"
    url: https://www.rfc-editor.org/rfc/rfc5246
  - label: "RFC 5280 — Internet X.509 Public Key Infrastructure Certificate and CRL Profile"
    url: https://www.rfc-editor.org/rfc/rfc5280
  - label: "RFC 6066 — TLS Extensions (Server Name Indication); RFC 7301 — ALPN"
    url: https://www.rfc-editor.org/rfc/rfc6066
  - label: "RFC 9000 — QUIC, A UDP-Based Multiplexed and Secure Transport"
    url: https://www.rfc-editor.org/rfc/rfc9000
  - label: "RFC 9001 — Using TLS to Secure QUIC"
    url: https://www.rfc-editor.org/rfc/rfc9001
  - label: "RFC 9114 — HTTP/3"
    url: https://www.rfc-editor.org/rfc/rfc9114
  - label: "RFC 9325 (BCP 195) — Recommendations for Secure Use of TLS and DTLS"
    url: https://www.rfc-editor.org/rfc/rfc9325
  - label: "RFC 6962 — Certificate Transparency"
    url: https://www.rfc-editor.org/rfc/rfc6962
---

TCP delivers bytes reliably but to anyone who can read the path: every router, Wi-Fi access point and ISP in between can read or alter them. **Transport Layer Security (TLS)** wraps a TCP stream in cryptography so that two programs can communicate with three guarantees:

- **Confidentiality**: eavesdroppers see only ciphertext.
- **Integrity**: any modification in transit is detected.
- **Authentication**: the client knows it is talking to the genuine server (and, optionally, the server knows who the client is).

HTTPS is HTTP over TLS; the same layer protects SMTP, IMAP, MQTT, database connections, and, as DTLS and QUIC, UDP traffic. TLS is the successor to SSL (the old name still appears everywhere; SSL 2.0 and 3.0 are obsolete and broken).

## The pieces

| Piece | Job |
|---|---|
| **Key exchange** | the two sides agree on a shared secret without sending it: today **ECDHE** (ephemeral elliptic-curve Diffie–Hellman) |
| **Certificate** | binds a public key to a name (`www.example.test`), signed by a certificate authority (CA) so the client can check it |
| **Signature** | the server proves it holds the certificate's private key by signing the handshake |
| **Symmetric cipher (AEAD)** | encrypts and authenticates the data with the agreed key: AES-GCM or ChaCha20-Poly1305 |
| **Extensions** | SNI (which hostname), ALPN (which application protocol), session tickets, key shares... |

**Forward secrecy** comes from the *ephemeral* in ECDHE: each connection uses a fresh key pair that is thrown away afterwards, so even if the server's long-term private key is stolen later, recorded old traffic cannot be decrypted. (Old RSA key transport lacked this and was removed in TLS 1.3.)

## Certificates and trust

A certificate (X.509, RFC 5280) contains the subject's name(s), public key, validity dates, allowed uses, and the **issuer's signature**. To trust it, the client builds a **chain**: leaf certificate → (intermediate CA certificates) → a **root CA** already in the client's **trust store** (shipped with the operating system or browser). Each link is a signature check, and the client also verifies: the name matches the **Subject Alternative Name** (the old Common Name is not used for matching), the dates are valid, the key usage allows server authentication, and the certificate has not been revoked (OCSP/CRLs, or short lifetimes: public certificates now last at most about a year, and Let's Encrypt issues 90-day ones). **Certificate Transparency** logs make mis-issuance by a CA publicly detectable.

The lab below creates a throwaway CA and a server certificate with `openssl` and inspects them. The same steps are what a private PKI (service mesh, internal services) does.

```bash
#!/bin/sh
# Make a throwaway CA and a server certificate (ECDSA P-256), then inspect them. Files go in ./pki (ignored by git).
set -e
rm -rf pki; mkdir pki; cd pki
openssl ecparam -name prime256v1 -genkey -noout -out ca.key
openssl req -x509 -new -key ca.key -sha256 -days 30 -subj "/CN=Demo Root CA" -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign" -out ca.pem
openssl ecparam -name prime256v1 -genkey -noout -out server.key
openssl req -new -key server.key -subj "/CN=www.example.test" -out server.csr
printf "subjectAltName=DNS:www.example.test\nbasicConstraints=CA:FALSE\nkeyUsage=digitalSignature\nextendedKeyUsage=serverAuth\n" > ext.cnf
openssl x509 -req -in server.csr -CA ca.pem -CAkey ca.key -CAcreateserial -days 7 -sha256 -extfile ext.cnf -out server.pem 2>/dev/null
echo '$ openssl x509 -in server.pem -noout -subject -issuer -ext subjectAltName,keyUsage,extendedKeyUsage'
openssl x509 -in server.pem -noout -subject -issuer -ext subjectAltName,keyUsage,extendedKeyUsage | sed 's/^/  /'
echo '$ openssl verify -CAfile ca.pem server.pem'; openssl verify -CAfile ca.pem server.pem
echo '$ openssl verify server.pem   (no trust anchor given)'; openssl verify server.pem 2>&1 | head -3 | sed 's/^/  /'
echo '$ openssl x509 -in server.pem -noout -text | grep -E "Public Key Algorithm|ASN1 OID|Signature Algorithm" | sort -u'
openssl x509 -in server.pem -noout -text | grep -E "Public Key Algorithm|ASN1 OID|Signature Algorithm" | sort -u | sed 's/^ */  /'
```

```text
$ sh certs.sh
$ openssl x509 -in server.pem -noout -subject -issuer -ext subjectAltName,keyUsage,extendedKeyUsage
  subject=CN=www.example.test
  issuer=CN=Demo Root CA
  X509v3 Subject Alternative Name: 
      DNS:www.example.test
  X509v3 Key Usage: 
      Digital Signature
  X509v3 Extended Key Usage: 
      TLS Web Server Authentication
$ openssl verify -CAfile ca.pem server.pem
server.pem: OK
$ openssl verify server.pem   (no trust anchor given)
  CN=www.example.test
  error 20 at 0 depth lookup: unable to get local issuer certificate
  error server.pem: verification failed
$ openssl x509 -in server.pem -noout -text | grep -E "Public Key Algorithm|ASN1 OID|Signature Algorithm" | sort -u
  ASN1 OID: prime256v1
  Public Key Algorithm: id-ecPublicKey
  Signature Algorithm: ecdsa-with-SHA256
  Signature Algorithm: ecdsa-with-SHA256
```

- The leaf names `www.example.test` in the **Subject Alternative Name** and is only allowed for `Digital Signature` and `TLS Web Server Authentication`; it is a leaf (`CA:FALSE`) so it cannot be used to sign other certificates.
- `openssl verify -CAfile ca.pem server.pem` succeeds because we told it which root to trust. Without a trust anchor it fails with *unable to get local issuer certificate*: **trust is not in the certificate; it is in the verifier's trust store.** This is why a self-signed or private-CA certificate gives a browser warning until the CA is installed on the client.

## The handshake, message by message

`openssl s_client -msg` prints each handshake message. For one connection with each protocol version:

```bash
#!/bin/sh
# The TLS 1.3 and 1.2 handshake messages, as listed by openssl s_client -msg (client sends >>>, server sends <<<).
sh certs.sh > /dev/null
for v in tls1_3 tls1_2; do
  openssl s_server -accept 4434 -cert pki/server.pem -key pki/server.key -$v -quiet -naccept 1 > /dev/null 2>&1 &
  sleep 0.5
  echo "== $v =="
  echo | openssl s_client -connect 127.0.0.1:4434 -$v -CAfile pki/ca.pem -servername www.example.test -msg 2>&1 | grep -E "^(>>>|<<<)" | sed -E 's/\[length [0-9a-f]+\],? ?//; s/^(>>>|<<<) /\1 /; s/ +$//' | grep -vE "^(>>>|<<<) (TLS 1.[23] Record Layer|ignore)" | awk '{ if ($1 == ">>>") d="client -> server"; else d="server -> client"; $1=""; print "  " d ":" $0 }' | grep -vE "RecordHeader|InnerContent" | head -14
  wait
done
```

```text
$ sh messages.sh
== tls1_3 ==
  client -> server: TLS 1.3, Handshake ClientHello
  server -> client: TLS 1.3, Handshake ServerHello
  server -> client: TLS 1.3, ChangeCipherSpec
  server -> client: TLS 1.3, Handshake EncryptedExtensions
  server -> client: TLS 1.3, Handshake Certificate
  server -> client: TLS 1.3, Handshake CertificateVerify
  server -> client: TLS 1.3, Handshake Finished
  client -> server: TLS 1.3, ChangeCipherSpec
  client -> server: TLS 1.3, Handshake Finished
  client -> server: TLS 1.3, Alert warning close_notify
== tls1_2 ==
  client -> server: TLS 1.2, Handshake ClientHello
  server -> client: TLS 1.2, Handshake ServerHello
  server -> client: TLS 1.2, Handshake Certificate
  server -> client: TLS 1.2, Handshake ServerKeyExchange
  server -> client: TLS 1.2, Handshake ServerHelloDone
  client -> server: TLS 1.2, Handshake ClientKeyExchange
  client -> server: TLS 1.2, ChangeCipherSpec
  client -> server: TLS 1.2, Handshake Finished
  server -> client: TLS 1.2, Handshake NewSessionTicket
  server -> client: TLS 1.2, Handshake Finished
  server -> client: TLS 1.2, Alert warning close_notify
  client -> server: TLS 1.2, Alert warning close_notify
```

**TLS 1.2** needs two round trips after the TCP connection:

1. Client → `ClientHello` (supported versions and ciphers, a random value, extensions: SNI, ALPN).
2. Server → `ServerHello` (the choice), `Certificate`, `ServerKeyExchange` (its ephemeral key, signed), `ServerHelloDone`.
3. Client → `ClientKeyExchange` (its ephemeral key), `ChangeCipherSpec`, `Finished` (a MAC over the whole handshake, now encrypted). *Application data cannot be sent before this point.*
4. Server → `NewSessionTicket`, `ChangeCipherSpec`, `Finished`.

**TLS 1.3** cuts it to **one** round trip by having the client guess the key-exchange group and send its key share *in the first message*: `ClientHello` already contains the ephemeral public key; the server replies with `ServerHello` (its share, so both sides can now derive keys) and sends everything else **already encrypted** (`EncryptedExtensions`, `Certificate`, `CertificateVerify`, `Finished`), so even the certificate is hidden from eavesdroppers. The client's `Finished` can be followed immediately by application data. (The `ChangeCipherSpec` messages in a TLS 1.3 trace are meaningless compatibility padding for middleboxes.) TLS 1.3 also removed everything weak: static RSA key exchange, CBC ciphers, SHA-1, compression, renegotiation.

## Measuring the round trips

The next experiment puts a client and server on a path with a 40 ms round-trip time (20 ms each way) and times the TCP connect, the TLS handshake and the arrival of the first response byte for three cases: a full TLS 1.2 handshake, a full TLS 1.3 handshake, and a TLS 1.3 handshake resuming a session ticket.

```python
#!/usr/bin/env python3
"""TLS server and timing client. Usage: tls_lab.py server | tls_lab.py client"""
import os, socket, ssl, sys, threading, time

HOST, PORT = "10.0.0.2", 4433

def server():
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); ctx.load_cert_chain("pki/server.pem", "pki/server.key"); ctx.set_alpn_protocols(["h2", "http/1.1"])
    ls = socket.socket(); ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); ls.bind((HOST, PORT)); ls.listen(16)
    def handle(c):
        try:
            with ctx.wrap_socket(c, server_side=True) as t: t.recv(100); t.sendall(b"ok")
        except Exception: pass
    while True:
        c, _ = ls.accept(); threading.Thread(target=handle, args=(c,), daemon=True).start()

def make_ctx(version):
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT); ctx.load_verify_locations("pki/ca.pem")
    ctx.minimum_version = ctx.maximum_version = version; ctx.set_alpn_protocols(["h2", "http/1.1"]); return ctx

def one(ctx, session=None):
    t0 = time.perf_counter(); raw = socket.create_connection((HOST, PORT)); raw.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, int(os.environ.get("NODELAY", "1"))); t1 = time.perf_counter()   # NODELAY: otherwise Nagle can hold the request back behind the TLS Finished message
    s = ctx.wrap_socket(raw, server_hostname="www.example.test", session=session); t2 = time.perf_counter()   # wrap_socket performs the handshake
    s.sendall(b"GET /"); s.recv(10); t3 = time.perf_counter()
    info = (s.version(), s.cipher()[0], s.selected_alpn_protocol(), s.session_reused); sess = s.session; s.close()
    return (t1 - t0, t2 - t1, t3 - t0), info, sess

def client():
    one(make_ctx(ssl.TLSVersion.TLSv1_3))                          # warm-up: the very first packets also pay for ARP resolution
    print(f"{'case':<26} {'TCP connect':>12} {'TLS handshake':>14} {'first byte':>11}   protocol / cipher / ALPN / resumed")
    for label, ver in (("TLS 1.2, full handshake", ssl.TLSVersion.TLSv1_2), ("TLS 1.3, full handshake", ssl.TLSVersion.TLSv1_3)):
        ctx = make_ctx(ver); (a, b, c), info, sess = one(ctx)
        print(f"{label:<26} {a * 1e3:9.0f} ms {b * 1e3:11.0f} ms {c * 1e3:8.0f} ms   {info[0]} / {info[1]} / {info[2]} / {info[3]}")
        if ver == ssl.TLSVersion.TLSv1_3:
            time.sleep(0.2)                                          # TLS 1.3 session tickets arrive after the handshake; reuse one (same context)
            (a, b, c), info, _ = one(ctx, sess)
            print(f"{'TLS 1.3, resumed session':<26} {a * 1e3:9.0f} ms {b * 1e3:11.0f} ms {c * 1e3:8.0f} ms   {info[0]} / {info[1]} / {info[2]} / {info[3]}")

(server if sys.argv[1] == "server" else client)()
```

```bash
#!/bin/sh
# Client and server namespaces, 40 ms round-trip time (20 ms each way), then time TCP connect, the TLS handshake and the first response byte.
sh certs.sh > /dev/null
mount -t tmpfs tmpfs /run; mkdir -p /run/netns
ip netns add client; ip netns add server; ip link add vc type veth peer name vs; ip link set vc netns client; ip link set vs netns server
ip -n client addr add 10.0.0.1/24 dev vc; ip -n server addr add 10.0.0.2/24 dev vs
for x in "client vc" "server vs"; do set -- $x; ip -n $1 link set $2 up; ip -n $1 link set lo up; ip netns exec $1 tc qdisc add dev $2 root netem delay 20ms; done
ip netns exec server python3 tls_lab.py server &
SPID=$!; sleep 0.7
ip netns exec client python3 tls_lab.py client
echo; echo "Same test with TCP_NODELAY off (Nagle's algorithm enabled on the client):"
NODELAY=0 ip netns exec client python3 tls_lab.py client | sed -n '1p;4p'
kill $SPID 2>/dev/null
```

```text
$ unshare -Urnm sh lab.sh 2>&1
case                        TCP connect  TLS handshake  first byte   protocol / cipher / ALPN / resumed
TLS 1.2, full handshake           40 ms          82 ms      163 ms   TLSv1.2 / ECDHE-ECDSA-AES256-GCM-SHA384 / h2 / False
TLS 1.3, full handshake           40 ms          42 ms      123 ms   TLSv1.3 / TLS_AES_256_GCM_SHA384 / h2 / False
TLS 1.3, resumed session          40 ms          42 ms      122 ms   TLSv1.3 / TLS_AES_256_GCM_SHA384 / h2 / True

Same test with TCP_NODELAY off (Nagle's algorithm enabled on the client):
case                        TCP connect  TLS handshake  first byte   protocol / cipher / ALPN / resumed
TLS 1.3, resumed session          40 ms          42 ms      163 ms   TLSv1.3 / TLS_AES_256_GCM_SHA384 / h2 / True
```

- **TCP connect: 40 ms = 1 RTT** (SYN, SYN+ACK).
- **TLS 1.2 handshake: about 80 ms = 2 RTT**; **TLS 1.3 about 40 ms = 1 RTT**. So a request over a fresh connection costs 3 RTT (TCP + 2) with TLS 1.2 and 2 RTT with TLS 1.3 before the request even starts: on a 100 ms intercontinental path, that is 100 ms saved on every new connection. The first-byte column is larger by one more RTT, the request and response themselves.
- **Resumption** (a session ticket from an earlier connection) skips certificate verification and the expensive public-key operations, saving CPU; with TLS 1.3 the round-trip count stays at 1 RTT unless **0-RTT early data** is used, where the client sends its first request together with the `ClientHello`. 0-RTT removes even that round trip, at a price: early data can be **replayed** by an attacker, so servers must accept it only for idempotent requests.
- **The Nagle trap.** The last block repeats the test with `TCP_NODELAY` turned off. The first-byte time rises by about 40 ms. The client writes its TLS `Finished` and then, a moment later, the application request: Nagle's algorithm holds the second small write until the first is acknowledged, and the server delays that acknowledgment (up to 40 ms on Linux). This is the classic Nagle plus delayed-ACK stall from the TCP note, and it is why TLS libraries and HTTP clients set `TCP_NODELAY` by default.

## SNI, ALPN and virtual hosting

The `ClientHello` carries extensions that matter operationally:

- **SNI (Server Name Indication)**: the hostname the client wants, sent in clear text (so one IP address can serve many certificates, and so a middlebox can see which site you visit). *Encrypted ClientHello (ECH)* hides it.
- **ALPN (Application-Layer Protocol Negotiation)**: which protocol to run inside TLS (`h2` for HTTP/2, `http/1.1`, `h3`...). The experiment negotiated `h2`.
- **Supported groups / signature algorithms / versions**: negotiation of crypto parameters.

## Mutual TLS and service identity

By default only the server is authenticated. In **mutual TLS (mTLS)** the server also requests and verifies a client certificate, giving strong machine-to-machine identity: used for service meshes, Kubernetes components, VPN-like zero-trust access and some APIs. It requires a PKI to issue and rotate client certificates, usually short-lived.

## QUIC and HTTP/3

**QUIC** (RFC 9000) is a transport protocol that runs over **UDP** and builds in what TCP and TLS provide separately:

- TLS 1.3 is integrated into the QUIC handshake: **1 RTT for a new connection** (instead of TCP's 1 plus TLS's 1), and **0-RTT** for resumed ones.
- **Streams**: many independent byte streams in one connection, so a lost packet blocks only the stream it belongs to (TCP's **head-of-line blocking** stalls everything behind a loss).
- **Connection IDs** instead of the 5-tuple, so a connection survives a client changing networks (Wi-Fi to cellular).
- **Almost everything is encrypted**, including most of the transport header; middleboxes can no longer interfere or measure.
- Loss recovery and congestion control (CUBIC, BBR) implemented in user space, so they can evolve without operating-system updates.

**HTTP/3** (RFC 9114) is HTTP mapped onto QUIC streams. Practical consequences for network engineers: it is **UDP port 443**, not TCP, so firewalls that only allow TCP/443 force a fallback to HTTP/2; there are no TCP flags or sequence numbers to inspect; and NAT/load-balancer timeouts for UDP flows matter.

## Linux and tooling

| Tool | Use |
|---|---|
| `openssl s_client -connect host:443 -servername host` | inspect a server: protocol, cipher, chain, verification result (`-showcerts`, `-msg`, `-tls1_3`) |
| `openssl x509 -in cert.pem -noout -text` | read a certificate |
| `openssl verify -CAfile ca.pem cert.pem` | check a chain |
| `curl -v https://host` (and `--http3` where built in) | handshake summary and HTTP exchange |
| `ss -tni` | TCP internals under a TLS connection |
| Wireshark with the **key log file** (`SSLKEYLOGFILE` environment variable) | decrypt your own captures |

## Failure modes and troubleshooting

| Symptom | Likely cause | Check |
|---|---|---|
| "certificate has expired" | validity dates passed (or the *client's clock* is wrong) | `openssl x509 -noout -dates`; check the system time (NTP) |
| "unable to get local issuer certificate" | server not sending the intermediate certificate, or private CA missing from the trust store | `openssl s_client -showcerts`; install the chain correctly |
| "hostname mismatch" | certificate's SAN does not include the name used (or an IP address was used) | `-ext subjectAltName` |
| `handshake failure` / `protocol version` alert | no common version or cipher (old client, or hardened server disabling TLS 1.0/1.1) | `openssl s_client -tls1_2`, check server policy |
| Works to some clients but not others | SNI not sent (very old clients) or wrong default certificate; ALPN mismatch | `-servername` |
| Intermittent resets after the handshake | middlebox or firewall inspecting traffic; MTU black hole on large certificate messages | capture; check path MTU |
| Slow first request | TLS 1.2 on a high-latency path, OCSP lookups, DNS | measure with `curl -w` timing variables |
| Browser warning on a corporate network | TLS-intercepting proxy using its own CA | inspect the issuer of the certificate presented |

## Key takeaways

1. TLS gives confidentiality, integrity and authentication on top of TCP; QUIC integrates the same cryptography into a UDP-based transport.
2. Trust comes from the client's trust store and the chain of signatures, plus name, validity and usage checks; a certificate alone proves nothing.
3. ECDHE gives forward secrecy; TLS 1.3 removed weak options, encrypts the certificate, and needs one round trip instead of two (measured: about 40 ms versus 80 ms on a 40 ms path).
4. Resumption saves cryptographic work; 0-RTT saves a round trip but allows replay.
5. Small-write interactions (Nagle with delayed ACK) can add 40 ms to every new connection: set `TCP_NODELAY`.
6. Most TLS outages are expired or mismatched certificates, missing intermediates, wrong clocks and middleboxes; `openssl s_client` finds them.
