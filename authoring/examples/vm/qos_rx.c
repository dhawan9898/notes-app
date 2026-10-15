/* qos_rx: accept one TCP connection, count the bytes and report the rate.   Usage: qos_rx ADDRESS PORT */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (double)t.tv_sec + (double)t.tv_nsec / 1e9; }

int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "usage: %s ADDRESS PORT\n", argv[0]); return 2; }
    int s = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons((uint16_t)atoi(argv[2]))};
    if (inet_pton(AF_INET, argv[1], &a.sin_addr) != 1 || bind(s, (struct sockaddr *)&a, sizeof a) < 0 || listen(s, 1) < 0) { perror("bind/listen"); return 1; }
    int c = accept(s, NULL, NULL);
    static char buf[65536]; size_t total = 0; double t0 = 0; ssize_t n;
    while ((n = recv(c, buf, sizeof buf, 0)) > 0) { if (total == 0) t0 = now(); total += (size_t)n; }
    double dt = now() - t0;
    printf("received %zu bytes in %.2f s = %.1f Mbit/s\n", total, dt, (double)total * 8 / dt / 1e6);
    return 0;
}
