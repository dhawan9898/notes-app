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
