/* qos_tx: send 2 MiB over TCP as fast as the path allows.   Usage: qos_tx ADDRESS PORT */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "usage: %s ADDRESS PORT\n", argv[0]); return 2; }
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons((uint16_t)atoi(argv[2]))};
    if (inet_pton(AF_INET, argv[1], &a.sin_addr) != 1 || connect(s, (struct sockaddr *)&a, sizeof a) < 0) { perror("connect"); return 1; }
    static char data[65536]; memset(data, 'x', sizeof data);
    for (int i = 0; i < 32; i++) for (size_t off = 0; off < sizeof data;) { ssize_t n = send(s, data + off, sizeof data - off, 0); if (n <= 0) { perror("send"); return 1; } off += (size_t)n; }
    close(s);
    return 0;
}
