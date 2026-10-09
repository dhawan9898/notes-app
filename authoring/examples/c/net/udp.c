#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    int srv = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(0), .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    bind(srv, (struct sockaddr *)&a, sizeof a);
    socklen_t al = sizeof a;
    getsockname(srv, (struct sockaddr *)&a, &al);                         /* learn the ephemeral port the kernel chose */
    printf("UDP server bound to 127.0.0.1 on an ephemeral port (>1023: %s)\n", ntohs(a.sin_port) > 1023 ? "yes" : "no");

    if (fork() == 0) {                                                    /* client: three separate datagrams */
        int c = socket(AF_INET, SOCK_DGRAM, 0);
        sendto(c, "first", 5, 0, (struct sockaddr *)&a, sizeof a);
        sendto(c, "second message", 14, 0, (struct sockaddr *)&a, sizeof a);
        sendto(c, "", 0, 0, (struct sockaddr *)&a, sizeof a);              /* an empty datagram is valid (not EOF!) */
        _exit(0);
    }
    wait(NULL);
    char buf[8];                                                           /* deliberately small */
    for (int i = 0; i < 3; i++) {
        struct sockaddr_in from; socklen_t fl = sizeof from;
        ssize_t n = recvfrom(srv, buf, sizeof buf, MSG_TRUNC, (struct sockaddr *)&from, &fl);   /* MSG_TRUNC: return the real length */
        printf("datagram %d: real length %zd, kept %zu bytes: \"%.*s\" (sender port known: %s)\n", i + 1, n, n < (ssize_t)sizeof buf ? (size_t)n : sizeof buf,
               (int)(n < (ssize_t)sizeof buf ? n : (ssize_t)sizeof buf), buf, ntohs(from.sin_port) ? "yes" : "no");
    }
    return 0;
}
