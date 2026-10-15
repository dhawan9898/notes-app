---
title: Joining and Leaving a Multicast Group
track: networking
module: Multicast
moduleOrder: 8
order: 1
level: intermediate
minutes: 40
env: browser
terminal:
  - ip link set lo up
  - ./multicast
summary: "Four exercises with a C program on the loopback interface: joining a multicast group from two sockets, sending one packet to several receivers, what leaving the group does (and the subtlety of interface-level membership and IP_MULTICAST_ALL), and mapping multicast IP addresses to Ethernet addresses and scopes."
prerequisites:
  - "IP Multicast Basics — Addressing, IGMP, MLD and Snooping"
  - "PIM Sparse Mode, SSM and Multicast Routing"
sources:
  - label: "RFC 1112 — Host Extensions for IP Multicasting"
    url: https://www.rfc-editor.org/rfc/rfc1112
  - label: "RFC 3376 — Internet Group Management Protocol, Version 3"
    url: https://www.rfc-editor.org/rfc/rfc3376
  - label: "RFC 5771 — IANA Guidelines for IPv4 Multicast Address Assignments"
    url: https://www.rfc-editor.org/rfc/rfc5771
  - label: "man 7 ip (IP_ADD_MEMBERSHIP, IP_MULTICAST_TTL), man 8 ip-maddress"
    url: https://man7.org/linux/man-pages/man7/ip.7.html
---

Multicast sends **one packet to many receivers** without the sender knowing who they are: receivers *join* a group address and the network delivers copies only where members exist. The program below uses the loopback interface so everything happens on your machine; read the output, then answer the questions.

```c
/* Join and leave an IPv4 multicast group on the loopback interface and watch the kernel's membership table. */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define GROUP "239.1.2.3"
#define PORT 5007
#ifndef IP_MULTICAST_ALL
#define IP_MULTICAST_ALL 49          /* Linux option number; some libc headers do not name it */
#endif

static struct ip_mreq mreq(void) { struct ip_mreq m; inet_pton(AF_INET, GROUP, &m.imr_multiaddr); inet_pton(AF_INET, "127.0.0.1", &m.imr_interface); return m; }

static int make_receiver(void) {
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP), one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons(PORT), .sin_addr.s_addr = htonl(INADDR_ANY)};
    if (bind(s, (struct sockaddr *)&a, sizeof a) < 0) { perror("bind"); exit(1); }
    struct ip_mreq m = mreq();                                                    /* join GROUP on the loopback interface */
    if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof m) < 0) { perror("IP_ADD_MEMBERSHIP"); exit(1); }
    struct timeval tv = {1, 0}; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    return s;
}
static void leave(int s) { struct ip_mreq m = mreq(); setsockopt(s, IPPROTO_IP, IP_DROP_MEMBERSHIP, &m, sizeof m); }

static void got(const char *who, int s, const char *note) {
    char b[100]; ssize_t n = recv(s, b, sizeof b - 1, 0);
    if (n < 0) printf("   %s got nothing\n", who); else { b[n] = 0; printf("   %s got: %s%s\n", who, b, note); }
}

int main(void) {
    int r1 = make_receiver(), r2 = make_receiver();
    printf("1. two sockets joined %s on the loopback interface (IP_ADD_MEMBERSHIP); the kernel keeps one membership per interface and group, counted by the sockets that asked for it\n", GROUP);

    int tx = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP), ttl = 1; struct in_addr ifa; inet_pton(AF_INET, "127.0.0.1", &ifa);
    setsockopt(tx, IPPROTO_IP, IP_MULTICAST_IF, &ifa, sizeof ifa); setsockopt(tx, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof ttl);
    struct sockaddr_in to = {.sin_family = AF_INET, .sin_port = htons(PORT)}; inet_pton(AF_INET, GROUP, &to.sin_addr);
    sendto(tx, "one packet, many receivers", 26, 0, (struct sockaddr *)&to, sizeof to);
    printf("2. one datagram to %s:%d\n", GROUP, PORT); got("receiver 1", r1, ""); got("receiver 2", r2, "");

    leave(r2);
    sendto(tx, "second packet", 13, 0, (struct sockaddr *)&to, sizeof to);
    printf("3. after receiver 2 left the group\n"); got("receiver 1", r1, "");
    got("receiver 2, which left,", r2, " (the interface is still a member because receiver 1 is, and IP_MULTICAST_ALL defaults to on)");
    leave(r1);
    printf("   both sockets have now left the group\n");
    sendto(tx, "third packet", 12, 0, (struct sockaddr *)&to, sizeof to);
    char b[100]; printf("   a third datagram sent to the group: %s\n", recv(r1, b, sizeof b, 0) < 0 ? "arrives nowhere, nobody on this interface is a member" : "a packet arrived");

    /* 3b. a socket that left the group can still see the group's traffic unless IP_MULTICAST_ALL is switched off */
    int member = make_receiver(), left_default = make_receiver(), left_all0 = make_receiver(), zero = 0;
    setsockopt(left_all0, IPPROTO_IP, IP_MULTICAST_ALL, &zero, sizeof zero);
    leave(left_default); leave(left_all0);
    sendto(tx, "check", 5, 0, (struct sockaddr *)&to, sizeof to);
    struct { const char *name; int fd; } who[] = {{"member", member}, {"left, IP_MULTICAST_ALL=1 (default)", left_default}, {"left, IP_MULTICAST_ALL=0", left_all0}};
    printf("3b. who receives a packet for the group (one socket is still a member)\n   %-36s %s\n", "socket", "received");
    for (int i = 0; i < 3; i++) printf("   %-36s %s\n", who[i].name, recv(who[i].fd, b, sizeof b, 0) < 0 ? "nothing" : "received");

    printf("4. group address facts:\n");
    const char *addrs[] = {"224.0.0.1", "224.0.0.2", "224.0.0.251", "239.1.2.3", "232.1.1.1"};
    for (int i = 0; i < 5; i++) {
        unsigned a, b2, c, d; sscanf(addrs[i], "%u.%u.%u.%u", &a, &b2, &c, &d);
        const char *scope = !strcmp(addrs[i], "224.0.0.1") ? "all hosts" : !strcmp(addrs[i], "224.0.0.2") ? "all routers" : !strcmp(addrs[i], "224.0.0.251") ? "mDNS (link-local)"
                          : a == 232 ? "source-specific multicast range" : "administratively scoped (private)";
        printf("   %-12s -> Ethernet 01:00:5e:%02x:%02x:%02x   %s\n", addrs[i], b2 & 0x7f, c, d, scope);
    }
    return 0;
}
```

```text
$ ip link set lo up
$ ./multicast
1. two sockets joined 239.1.2.3 on the loopback interface (IP_ADD_MEMBERSHIP); the kernel keeps one membership per interface and group, counted by the sockets that asked for it
2. one datagram to 239.1.2.3:5007
   receiver 1 got: one packet, many receivers
   receiver 2 got: one packet, many receivers
3. after receiver 2 left the group
   receiver 1 got: second packet
   receiver 2, which left, got: second packet (the interface is still a member because receiver 1 is, and IP_MULTICAST_ALL defaults to on)
   both sockets have now left the group
   a third datagram sent to the group: arrives nowhere, nobody on this interface is a member
3b. who receives a packet for the group (one socket is still a member)
   socket                               received
   member                               received
   left, IP_MULTICAST_ALL=1 (default)   received
   left, IP_MULTICAST_ALL=0             nothing
4. group address facts:
   224.0.0.1    -> Ethernet 01:00:5e:00:00:01   all hosts
   224.0.0.2    -> Ethernet 01:00:5e:00:00:02   all routers
   224.0.0.251  -> Ethernet 01:00:5e:00:00:fb   mDNS (link-local)
   239.1.2.3    -> Ethernet 01:00:5e:01:02:03   administratively scoped (private)
   232.1.1.1    -> Ethernet 01:00:5e:01:01:01   source-specific multicast range
```

## Exercises

**1. Membership.** Two sockets joined `239.1.2.3` on `lo`. Why does the kernel keep one membership for both of them, and what would `ip maddr show dev lo` and `/proc/net/igmp` list on a real machine?

**2. One send, two receivers.** The sender called `sendto` once. How many packets left the sender, and how many receivers got the message? What would unicast have needed?

**3. Leaving.** Receiver 2 leaves the group but still gets the second packet; receiver 1 then leaves too and the third packet reaches nobody. Explain both observations.

**4. Addresses.** For each group in the last block, explain the Ethernet address (`01:00:5e:...`) and the scope. How is the MAC computed, and what is the catch?

## Answers

Try before opening.

<details>
<summary>Show the explanations</summary>

**1.** Group membership is a property of the **interface** (the host tells the network "someone here wants this group"), not of each socket: the kernel counts the sockets that asked, so the second join only increments a reference count and `lo` gets exactly **one** entry for `239.1.2.3`. On a real machine `ip maddr show dev lo` and `cat /proc/net/igmp` show that table: `224.0.0.1` ("all hosts", every IP multicast host is always a member) and usually `224.0.0.251` (mDNS, joined by a system service), plus `239.1.2.3` with two users while the program runs. (The in-browser Linux kernel is built without IGMP, so its `/proc/net/igmp` does not exist and the program does not print the table: the delivery results show the same behaviour.) `/proc/net/igmp` is the kernel's table of the groups it reports through **IGMP** (the protocol hosts use to tell routers and switches about memberships). On a real Ethernet interface the host sends IGMP membership reports for each group when it joins and in answer to periodic queries from the router, which is how routers and snooping switches learn which ports have receivers.

**2.** **One packet left the sender** (one `sendto`, one datagram on the wire), and the network or the host's own stack replicates it for each member: both receivers got the message. With unicast the sender would need a separate `sendto` and a separate copy on the wire **per receiver**, so the cost grows with the audience (a video stream to 10,000 viewers would send 10,000 copies from the source), whereas multicast keeps the sender's cost constant and copies packets only where the delivery tree branches, which is why it is used for market-data feeds, IPTV, routing protocols (OSPF uses `224.0.0.5` and `224.0.0.6`) and service discovery. The price: **UDP only** (no connection, no retransmission), the sender gets no feedback, and the network must support multicast routing (PIM) and group management (IGMP), and it is generally **not routed across the public internet**. The sender set `IP_MULTICAST_TTL` to 1 so the packet cannot cross a router: for multicast, the TTL doubles as the scope limit.

**3.** Receiver 2 called `IP_DROP_MEMBERSHIP`, but it **still received the second packet**. Two things combine. Membership is counted per **interface**, and receiver 1 was still a member, so the interface kept receiving the group. And on Linux a socket receives every datagram for its port that arrives on the machine, including groups joined by *other* sockets, unless the socket option **`IP_MULTICAST_ALL`** is switched off (the default is on, for historical compatibility). Step 3b of the output confirms it: with three sockets on the same port, one a real member, one that joined and left with the default option, and one that joined and left with `IP_MULTICAST_ALL` set to 0, a packet to the group reached the member and the default socket but not the third.

So a socket should set `IP_MULTICAST_ALL` to 0 with `setsockopt` (the program defines the Linux option number 49 itself, because some libc headers do not name it) or `bind()` to the group address instead of `INADDR_ANY` if it must only see the groups it joined itself. After **both** program sockets had left, the third datagram to the group **arrived nowhere**: the host sent an IGMP *leave* (so routers and snooping switches stop forwarding the group to this interface), and the timeout shows no receiver got it. This is the essential multicast rule: **traffic flows only where there are members, membership is tracked per interface, and IGMP (IPv4) or MLD (IPv6) is the signalling.**

**4.**

| Group | Ethernet address | Meaning |
|---|---|---|
| `224.0.0.1` | `01:00:5e:00:00:01` | all hosts on the link |
| `224.0.0.2` | `01:00:5e:00:00:02` | all routers on the link |
| `224.0.0.251` | `01:00:5e:00:00:fb` | mDNS, link-local |
| `239.1.2.3` | `01:00:5e:01:02:03` | administratively scoped (private) range `239.0.0.0/8` |
| `232.1.1.1` | `01:00:5e:01:01:01` | source-specific multicast range `232.0.0.0/8` |

IPv4 multicast addresses are `224.0.0.0/4` (first octet 224 to 239). The Ethernet address is **computed**, not looked up (no ARP): the fixed prefix `01:00:5e`, a zero bit, then the **low 23 bits** of the group address. The catch is that an IPv4 group has 28 significant bits but only 23 fit, so **32 different groups map to the same MAC** (for example `224.1.1.1` and `225.129.1.1` share one), and a host's network card may receive frames for groups it did not join; the IP layer then discards them. Address ranges: `224.0.0.0/24` is **link-local** (never forwarded by routers: routing protocols, `224.0.0.5` OSPF, `224.0.0.18` VRRP), `232.0.0.0/8` is for **source-specific multicast** (receivers name both the group and the source, avoiding the need to discover sources), and `239.0.0.0/8` is **administratively scoped**, the multicast equivalent of private addresses, which you should use for your own applications. IPv6 uses `ff00::/8` with the group's low 32 bits copied into `33:33:xx:xx:xx:xx`.

</details>

## Key takeaways

1. Multicast sends one packet to a group address; the network copies it only where members exist, so the sender's cost does not grow with the audience.
2. Hosts announce membership per interface with IGMP (IPv4) or MLD (IPv6); the first join sends a report and the last leave sends a leave.
3. On Linux a socket sees group traffic that another socket's membership brought in unless `IP_MULTICAST_ALL` is 0 or the socket is bound to the group address.
4. Multicast MAC addresses are derived from the IP address (`01:00:5e` plus 23 bits), so 32 groups share each MAC; use `239.0.0.0/8` for private applications and TTL 1 to stay on the link.
