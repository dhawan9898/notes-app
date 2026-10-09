#define _GNU_SOURCE
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <time.h>
#include <sys/socket.h>
#include "common.h"

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

static int dial(const char *host, const char *port) {
    struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM }, *res, *ai;
    if (getaddrinfo(host, port, &hints, &res)) return -1;
    int fd = -1;
    for (ai = res; ai; ai = ai->ai_next) {                     /* try each address until one connects */
        fd = socket(ai->ai_family, ai->ai_socktype | SOCK_CLOEXEC, ai->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        close(fd); fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

/* Request/response where the application writes header and body SEPARATELY, then waits for the reply. */
static double round_trips(int fd, int n, int nodelay) {
    int v = nodelay;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &v, sizeof v);
    double t0 = now();
    char reply[64]; uint32_t len;
    for (int i = 0; i < n; i++) {
        uint8_t hdr[4] = {0, 0, 0, 5};
        send_all(fd, hdr, 4);               /* write 1: 4-byte header */
        send_all(fd, "hello", 5);           /* write 2: 5-byte body (Nagle may hold this until the header is ACKed) */
        if (recv_msg(fd, reply, sizeof reply, &len)) { puts("recv failed"); break; }
    }
    return (now() - t0) / n * 1e3;
}

int main(int argc, char **argv) {
    const char *port = argc > 1 ? argv[1] : "9100";
    int fd = dial("127.0.0.1", port);
    if (fd < 0) { perror("connect"); return 1; }
    char reply[64]; uint32_t len;
    send_msg(fd, "hello, sockets", 14);
    if (recv_msg(fd, reply, sizeof reply, &len) == 0) printf("echo reply: %.*s\n", (int)len, reply);
    printf("split writes, Nagle ON  (default): %7.2f ms per request\n", round_trips(fd, 20, 0));
    printf("split writes, TCP_NODELAY on     : %7.2f ms per request\n", round_trips(fd, 200, 1));
    close(fd);
    return 0;
}
