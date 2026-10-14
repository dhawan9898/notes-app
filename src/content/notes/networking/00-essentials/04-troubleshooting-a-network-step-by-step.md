---
title: "Troubleshooting a Network Problem Step by Step"
track: networking
module: Networking Essentials
moduleOrder: 0.5
order: 4
level: beginner
summary: "A repeatable method for 'it does not work': divide the problem by layer, the five failures you will actually see (no address, no route, DNS failure, connection refused versus timed out, and an HTTP or certificate error), with real curl, dig and ping output for each and what every message means."
prerequisites:
  - What Is a Network?
  - "How the Internet Works: From a Browser Click to a Web Page"
  - "IP Addresses, Subnets and Ports in Plain Language"
sources:
  - label: "RFC 1122 — Requirements for Internet Hosts: Communication Layers"
    url: https://www.rfc-editor.org/rfc/rfc1122
  - label: "man 1 curl (EXIT CODES) and man 1 dig"
    url: https://curl.se/docs/manpage.html
  - label: "RFC 792 — Internet Control Message Protocol"
    url: https://www.rfc-editor.org/rfc/rfc792
  - label: "Limoncelli, Hogan, Chalup — The Practice of System and Network Administration, Ch. 14 (Debugging)"
---

"The internet is down" can mean a dozen different faults. The professionals' advantage is not secret knowledge; it is a method: **split the problem in half, test one layer, and move on only when it passes.** This note gives that method and shows the five failures you will meet most, using output captured from real tests so you recognise the messages.

## The method

1. **Define the failure exactly.** What are you trying to reach (name or address, which port)? From where? Since when? Does it fail for everyone or just you? Did anything change?
2. **Work from the bottom up (or split the path in the middle).** If the lowest layer is broken, nothing above it can work.
3. **Change one thing at a time**, and write down what you tried.
4. **Compare with something that works**: another site, another device, another network (a phone hotspot is a great control).

The bottom-up checklist:

| Layer | Question | Test |
|---|---|---|
| Link | Is the cable or Wi-Fi connected? | link lights, `ip -br link` |
| Address | Do I have an IP, a mask, a gateway? | `ip -br addr`, `ip route` |
| Local network | Can I reach the gateway? | `ping <gateway>` |
| Internet routing | Can I reach a far *address*? | `ping 1.1.1.1` |
| Name resolution | Do names resolve? | `dig example.com`, `getent hosts example.com` |
| Transport | Is the port reachable? | `curl -v`, `nc -vz host 443` |
| Application | Does the service answer correctly? | `curl -i`, application logs |

Commands are explained in *Network Commands and SSH for Beginners*.

## Failure 1: no address (DHCP failed)

Symptom: nothing works, not even the gateway. `ip -br addr` shows no IPv4 address, or a `169.254.x.x` address (the self-assigned fallback). Causes: the DHCP server is unreachable, the cable or Wi-Fi password is wrong, or the pool is exhausted. Fix the link or renew the lease (`sudo dhclient -r && sudo dhclient`, or toggle the connection with `nmcli`).

## Failure 2: no route or no gateway

Symptom: you can reach local devices but not the internet. `ip route` shows no `default via ...` line, or the gateway is outside your subnet (a wrong mask or address). `ping <gateway>` fails or works; if the gateway answers but `ping 1.1.1.1` does not, the problem is on the router's side or the ISP's. A ping to a deliberately unreachable address looks like this:

```text
$ ping -c1 -W 1 192.0.2.1
--- 192.0.2.1 ping statistics ---
1 packets transmitted, 0 received, 100% packet loss, time 0ms
```

(`192.0.2.1` is reserved for documentation, so it never answers.) "100% packet loss" says *no reply*, not *why*. Remember that some networks drop ping on purpose; confirm with a TCP test.

## Failure 3: DNS cannot resolve the name

```text
$ curl -sS -m 4 -o /dev/null -w 'http=%{http_code}\n' http://no-such-host-xyz123.example/
curl: (6) Could not resolve host: no-such-host-xyz123.example
http=000
$ dig no-such-host-xyz123.example +noall +comments | grep status
;; ->>HEADER<<- opcode: QUERY, status: NXDOMAIN, id: 35715
```

`curl: (6)` means resolution failed. `NXDOMAIN` is DNS saying "this name does not exist" (check the spelling). A different failure is a *timeout* from the resolver (`;; connection timed out; no servers could be reached`), which means your configured DNS server is down or unreachable: test with `dig @1.1.1.1 example.com` to bypass it. The classic diagnostic: **if `ping 1.1.1.1` works but names fail, DNS is broken.**

## Failure 4: "connection refused" versus "timed out"

These sound alike and mean very different things.

```text
$ curl -sS -m 4 -o /dev/null -w 'http=%{http_code}\n' http://127.0.0.1:1/
curl: (7) Failed to connect to 127.0.0.1 port 1 after 0 ms: Could not connect to server
http=000
$ curl -sS -m 4 -o /dev/null -w 'http=%{http_code}\n' http://10.255.255.1/
curl: (28) Connection timed out after 4002 milliseconds
http=000
```

- **Refused** (`curl: (7)`, after 0 ms): the packet **arrived** and the target machine actively replied "nothing listens on that port" (a TCP reset). So the network path works; the service is stopped, bound to the wrong address or listening on a different port. On the server run `ss -tln`.
- **Timed out** (`curl: (28)`, after the full wait): **no reply at all**. Either the packets never arrive (no route, wrong address, link down) or a firewall silently drops them, or the server is dead. Test earlier layers; check firewall rules (*ACLs, Stateless vs Stateful Filtering and Infrastructure Protection*).

The speed of the failure is itself information: refusal is instant, a timeout takes as long as the timeout.

## Failure 5: connected, but the application complains

```text
$ curl -sS -m 4 -o /dev/null -w 'http=%{http_code}\n' https://example.com/nothing-here
http=404
```

The network is fine; the server answered `404 Not Found` (see the HTTP status classes in *How the Internet Works*). `401`/`403` are about permission, `5xx` about the server. A TLS (certificate) failure appears *before* any HTTP status. Typical messages are shown below for recognition (illustrative; you will see them when a certificate has expired or the name does not match):

```text
curl: (60) SSL certificate problem: certificate has expired
curl: (60) SSL: no alternative certificate subject name matches target host name 'example.org'
```

An expired certificate is the server owner's problem; **do not "fix" it with `curl -k`** (which disables verification) except for a deliberate test; and check that your own clock is right, because a wrong date makes valid certificates look expired.

## Reading curl's exit codes

| Code | Meaning | Usual next step |
|---|---|---|
| 6 | could not resolve host | DNS (Failure 3) |
| 7 | connection refused / failed to connect | service or port (Failure 4) |
| 28 | timed out | path or firewall (Failure 4) |
| 35, 60 | TLS handshake / certificate problem | certificate, clock, name (Failure 5) |
| 22 | HTTP error status with `-f` | application (Failure 5) |

## A worked example

*"The wiki does not load for me, but colleagues are fine."* Link and address are fine (other sites load), so test the name: `dig wiki.example.com` returns the right address, so DNS works. `curl -v https://wiki.example.com` says "timed out" after 10 seconds: not refused, so something is dropping packets between you and the server. A colleague's laptop works, so it is not the server. Your laptop is on a VPN profile that routes the wiki's network through a dead tunnel: disconnecting the VPN fixes it. Each step eliminated one layer; none was a guess.

## Pitfalls

- **Testing only one thing** (a browser) instead of splitting by layer.
- **Reading "ping failed" as "host down"**; many networks block ICMP.
- **Changing several settings at once**, then not knowing which one helped.
- **Confusing refused with timeout**; they point at opposite causes.
- **Disabling certificate checks** to "make it work".
- **Forgetting local causes**: VPNs, proxies, wrong system time, a stale DNS cache or a firewall on your own machine.

## Key takeaways

1. Troubleshoot methodically: define the failure, test one layer at a time, change one thing at a time and use a control.
2. No address or a `169.254.x.x` address means DHCP failed; a missing default route means no way out of the network.
3. If a far address works but the name does not, DNS is the culprit; `NXDOMAIN` means the name does not exist.
4. "Connection refused" (instant reply) means the host is reachable but nothing listens; "timed out" means no reply at all.
5. HTTP status codes and certificate errors mean the network is fine and the application layer needs attention.
