#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include "common.h"

static void *serve(void *arg) {
    int fd = (int)(intptr_t)arg;
    char buf[4096]; uint32_t len;
    while (recv_msg(fd, buf, sizeof buf, &len) == 0) {      /* echo each framed message, upper-cased */
        for (uint32_t i = 0; i < len; i++) if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] -= 32;
        if (send_msg(fd, buf, len)) break;
    }
    close(fd);
    return NULL;
}

int main(int argc, char **argv) {
    const char *port = argc > 1 ? argv[1] : "9100";
    struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM, .ai_flags = AI_PASSIVE }, *res;
    int rc = getaddrinfo("127.0.0.1", port, &hints, &res);    /* name resolution: IPv4/IPv6 agnostic */
    if (rc) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc)); return 1; }
    int ls = socket(res->ai_family, res->ai_socktype | SOCK_CLOEXEC, res->ai_protocol);
    int one = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);      /* allow quick restart despite TIME_WAIT */
    if (bind(ls, res->ai_addr, res->ai_addrlen) || listen(ls, 128)) { perror("bind/listen"); return 1; }
    freeaddrinfo(res);
    signal(SIGPIPE, SIG_IGN);
    fprintf(stderr, "listening on 127.0.0.1:%s\n", port);
    for (;;) {
        struct sockaddr_storage peer; socklen_t pl = sizeof peer;
        int c = accept4(ls, (struct sockaddr *)&peer, &pl, SOCK_CLOEXEC);
        if (c < 0) { if (errno == EINTR) continue; perror("accept"); break; }
        setsockopt(c, IPPROTO_TCP, 1 /* TCP_NODELAY */, &one, sizeof one);
        pthread_t t;
        pthread_create(&t, NULL, serve, (void *)(intptr_t)c);
        pthread_detach(t);
    }
    return 0;
}
