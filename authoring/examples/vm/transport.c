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
