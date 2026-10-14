---
title: "Network Commands and SSH for Beginners"
track: linux
module: Linux Essentials
moduleOrder: 0.5
order: 4
level: beginner
summary: "The everyday network toolkit on Linux: ip to see addresses and routes, ping, ss to see listening ports, curl to speak HTTP and read status codes, DNS lookups, and ssh with key-based login, scp and rsync, with captured loopback examples and a step-by-step troubleshooting order."
prerequisites:
  - Linux Command Line Basics
  - What Is a Network?
sources:
  - label: "man 8 ip, man 8 ss, man 8 ping, man 1 curl, man 1 ssh, man 1 ssh-keygen"
    url: https://man7.org/linux/man-pages/man8/ip.8.html
  - label: "iproute2 documentation"
    url: https://wiki.linuxfoundation.org/networking/iproute2
  - label: "curl documentation — everything curl"
    url: https://everything.curl.dev/
  - label: "OpenSSH manual pages and release notes"
    url: https://www.openssh.com/manual.html
---

Almost every Linux machine you meet is on a network, and when something "does not work", a handful of commands tell you where the problem is. This note teaches those commands in the order you use them when troubleshooting: *do I have an address? can I reach the other side? is anything listening? what does the server answer? can I log in?* The examples run against your own machine (the loopback interface), so you can reproduce them anywhere. For the concepts behind them see *What Is a Network?* and, later, *A Packet's Journey: Host to Gateway to Host*.

## 1. What addresses do I have? `ip`

`ip` (from the `iproute2` package) replaces the older `ifconfig` and `route`. The `-br` (brief) flag gives one tidy line per interface:

```text
$ ip -br addr show lo
lo               UNKNOWN        127.0.0.1/8 ::1/128
```

`lo` is the **loopback** interface: traffic sent to `127.0.0.1` (IPv4) or `::1` (IPv6) never leaves the machine, which makes it perfect for testing. On a typical laptop or server you will also see a real interface; the output looks like this (addresses from the documentation range, illustrative):

```text
$ ip -br addr
lo        UNKNOWN   127.0.0.1/8 ::1/128
eth0      UP        192.0.2.25/24 fe80::5054:ff:fe12:3456/64
$ ip route
default via 192.0.2.1 dev eth0
192.0.2.0/24 dev eth0 proto kernel scope link src 192.0.2.25
```

- `eth0 UP` means the link is up; `192.0.2.25/24` is your IPv4 address and subnet (the `/24` says the first 24 bits identify the network; see *IPv4 Addressing, Subnetting, VLSM and CIDR*).
- The **default route** (`default via 192.0.2.1`) names the **gateway**: where packets for anything outside your local network are sent. No default route usually means "no internet".
- `ip -br link` shows link state and MAC addresses, and `ip neigh` shows the ARP/neighbour table (who is on your local network).

## 2. Can I reach it? `ping`

`ping` sends small ICMP "echo" packets and times the replies:

```text
$ ping -c 2 -W 1 127.0.0.1
PING 127.0.0.1 (127.0.0.1) 56(84) bytes of data.
64 bytes from 127.0.0.1: icmp_seq=1 ttl=64 time=0.232 ms
64 bytes from 127.0.0.1: icmp_seq=2 ttl=64 time=0.101 ms

--- 127.0.0.1 ping statistics ---
2 packets transmitted, 2 received, 0% packet loss, time 1001ms
rtt min/avg/max/mdev = 0.101/0.166/0.232/0.065 ms
```

`-c 2` stops after two packets (otherwise it runs until `Ctrl+C`). Look at **packet loss** and **time**. Test outward in layers to find where it breaks:

1. `ping 127.0.0.1` : is the network stack itself alive?
2. `ping <your gateway>` : is your local link working?
3. `ping 1.1.1.1` (or another well-known public address) : do packets get to the internet?
4. `ping example.com` : does name resolution (DNS) work? If step 3 works and step 4 fails, the problem is DNS.

A missing reply does not always mean "down": many firewalls drop ICMP on purpose. The note *ICMP, Ping, Traceroute and Diagnostics* explains the reply codes and `traceroute`.

## 3. What is listening? `ss`

`ss` ("socket statistics", replacing `netstat`) lists network connections and listening ports. `-t` TCP, `-l` listening, `-n` numbers instead of names, `-p` the process (needs root to see other users' processes):

```text
$ ss -tln | grep 8765
LISTEN 0      5          127.0.0.1:8765       0.0.0.0:*
$ ss -tlnp | grep 8765
LISTEN 0      5          127.0.0.1:8765       0.0.0.0:*    users:(("python3",pid=3960405,fd=3))
```

This is a small web server started with `python3 -m http.server 8765 --bind 127.0.0.1`. The important column is the **local address**: `127.0.0.1:8765` means "reachable only from this machine"; `0.0.0.0:8765` or `*:8765` means "reachable from any interface", which exposes the service to the network. "Connection refused" from a client often means nothing is listening on that port; `ss -tln` proves it. Use `ss -tn` (without `-l`) to see established connections.

## 4. What does the server say? `curl`

`curl` speaks HTTP (and many other protocols) from the command line. `-i` includes the response headers, `-s` silences the progress bar, `-S` still shows errors:

```text
$ curl -sS -i http://127.0.0.1:8765/ | head -8
HTTP/1.0 200 OK
Server: SimpleHTTP/0.6 Python/3.13.5
Date: Wed, 14 Oct 2026 05:18:10 GMT
Content-type: text/html
Content-Length: 15
Last-Modified: Wed, 14 Oct 2026 05:18:10 GMT

<h1>hello</h1>
```

Everything before the blank line is the **response head**: the status line (`200 OK`) and headers; after it comes the body. For scripts, extract just what you need with `-w`:

```text
$ curl -sS -o /dev/null -w "status=%{http_code} size=%{size_download} time=%{time_total}s\n" http://127.0.0.1:8765/
status=200 size=15 time=0.001543s
$ curl -sS -o /dev/null -w "status=%{http_code}\n" http://127.0.0.1:8765/missing
status=404
```

Remember the families of HTTP status codes: **2xx** success, **3xx** redirect (add `-L` to follow), **4xx** you made a mistake (`404` not found, `401`/`403` not allowed), **5xx** the server failed. When no server answers at all, curl reports a *connection* error and a non-zero exit status (the 7 below means "failed to connect"):

```text
$ curl -sS --max-time 3 http://127.0.0.1:1 ; echo "curl exit status: $?"
curl: (7) Failed to connect to 127.0.0.1 port 1 after 0 ms: Could not connect to server
curl exit status: 7
```

Useful extras: `-X POST -d 'name=value'` sends data, `-H 'Header: value'` adds a header, `-o file` or `-O` saves the body, and `--max-time` prevents hanging scripts. `curl -v` shows the whole conversation including TLS.

## 5. Names: DNS

Humans use names; packets use addresses. DNS translates one into the other. Where the machine looks is in `/etc/resolv.conf` (`nameserver` lines) or, on modern desktops, a local resolver such as `systemd-resolved`. To test:

```bash
getent hosts example.com        # what the system itself resolves (includes /etc/hosts)
dig example.com                 # ask a DNS server directly and see the answer, TTL and server used
dig +short example.com MX       # one record type, compact
```

If `ping 1.1.1.1` works but `ping example.com` fails, DNS is the problem: check `resolv.conf`, try `dig @1.1.1.1 example.com` to bypass your resolver, and see the notes *DNS: Names, Resolvers and Records* and *How the Internet Works: From a Browser Click to a Web Page*.

## 6. Logging in to other machines: `ssh`

**SSH** gives you an encrypted shell on a remote machine, and also carries file copies and port forwards. The basic form is `ssh user@host`. The first connection asks you to confirm the host's **fingerprint** (this protects you from an impostor; the fingerprint is then remembered in `~/.ssh/known_hosts`). Passwords work, but **keys** are safer and more convenient:

```text
$ ssh-keygen -t ed25519 -C "alice@laptop" -f demo_key
$ ls demo_key demo_key.pub
demo_key  demo_key.pub
$ ls -l demo_key | cut -c1-10
-rw-------
$ cut -c1-60 demo_key.pub
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIFA1Ulhx4uMw1z+MayOZ1BMw
```

This created a **key pair**: the **private** key `demo_key` (never share it; note the `-rw-------` mode, because ssh refuses private keys readable by others) and the **public** key `demo_key.pub` (safe to share). In real use, give `ssh-keygen` a passphrase when asked. To enable login, append the public key to the server's `~/.ssh/authorized_keys`, which `ssh-copy-id user@host` does for you. After that `ssh user@host` logs you in without a password.

Handy forms:

```bash
ssh user@host                           # interactive shell
ssh user@host 'uptime; df -h /'         # run one command and return
scp report.pdf user@host:/tmp/          # copy a file to the server (also: scp user@host:/var/log/x .)
rsync -av project/ user@host:backup/    # copy only what changed (see the note on backups)
ssh -L 8080:localhost:80 user@host      # forward your local port 8080 to port 80 on the server
```

A `~/.ssh/config` file saves typing:

```text
Host web
    HostName 192.0.2.50
    User deploy
    IdentityFile ~/.ssh/demo_key
```

then `ssh web` is enough. Habits that keep SSH servers safe (set on the server, in `/etc/ssh/sshd_config`): disable password login (`PasswordAuthentication no`) once keys work, disallow direct root login (`PermitRootLogin no`), and keep your private key off shared machines. See *SSH, PAM, sudo and auditd* for the hardening details.

## Troubleshooting order

| Step | Command | If it fails |
|---|---|---|
| 1. Link and address | `ip -br addr`, `ip -br link` | cable, Wi-Fi, DHCP; interface DOWN or no IP |
| 2. Gateway | `ip route`, `ping <gateway>` | no default route, wrong subnet |
| 3. Internet by address | `ping 1.1.1.1` | upstream or firewall problem |
| 4. DNS | `getent hosts example.com`, `dig` | resolver settings |
| 5. Port open? | `ss -tln` (on the server), `curl -v` | service not running or bound to loopback only |
| 6. Application | `curl -i`, logs (`journalctl -u service`) | wrong URL, 4xx/5xx errors |

Working outward from "is my own machine OK?" to "is the application answering?" keeps you from guessing.

## Pitfalls

- **Testing only with a name** (`ping example.com`) and blaming "the network" when the problem is DNS.
- **A service bound to `127.0.0.1`** is invisible from other machines; check the listen address with `ss -tln`.
- **Firewalls silently dropping** ICMP or ports: "no reply" is not proof of "down". Compare with a test from a different place.
- **Wrong permissions on `~/.ssh`** (should be `700`, private keys `600`) make ssh ignore your keys.
- **Pasting commands from the web** that run `curl ... | sudo sh`: read scripts before running them.

## Key takeaways

1. Troubleshoot outward: `ip` (address, gateway), `ping` (reachability), `ss` (what is listening), `curl` (what the application answers).
2. The loopback address `127.0.0.1` lets you test services safely on your own machine.
3. HTTP status classes (2xx, 3xx, 4xx, 5xx) and curl's exit codes tell you whether you or the server is at fault.
4. If an address works but its name does not, the fault is DNS.
5. Prefer SSH keys over passwords, protect the private key, and use `scp`/`rsync` over SSH for transfers.
