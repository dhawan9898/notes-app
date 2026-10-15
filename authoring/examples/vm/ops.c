/* Interface counters, wrap-around arithmetic, NTP offset/delay and syslog priorities. */
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

typedef struct { unsigned long long rx_bytes, rx_packets, rx_errs, rx_drop, tx_bytes, tx_packets, tx_errs, tx_drop; } Counters;
static int counters(const char *dev, Counters *c) {                               /* one line per interface in /proc/net/dev */
    FILE *f = fopen("/proc/net/dev", "r"); char line[512]; size_t n = strlen(dev);
    if (!f) return -1;
    while (fgets(line, sizeof line, f)) {
        char *p = line; while (*p == ' ') p++;
        if (!strncmp(p, dev, n) && p[n] == ':') {
            unsigned long long skip[4];
            int ok = sscanf(p + n + 1, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu", &c->rx_bytes, &c->rx_packets, &c->rx_errs, &c->rx_drop,
                            &skip[0], &skip[1], &skip[2], &skip[3], &c->tx_bytes, &c->tx_packets, &c->tx_errs, &c->tx_drop);
            fclose(f); return ok == 12 ? 0 : -1;
        }
    }
    fclose(f); return -1;
}
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (double)t.tv_sec + (double)t.tv_nsec / 1e9; }
static uint32_t delta32(uint32_t old, uint32_t cur) { return cur - old; }          /* unsigned subtraction wraps modulo 2^32 */

int main(void) {
    printf("1. polling interface counters twice and computing rates\n");
    Counters a, b; counters("lo", &a); double t0 = now();
    int rx = socket(AF_INET, SOCK_DGRAM, 0), tx = socket(AF_INET, SOCK_DGRAM, 0), big = 1 << 22;
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(9400), .sin_addr.s_addr = htonl(0x7f000001)};
    bind(rx, (struct sockaddr *)&addr, sizeof addr); setsockopt(rx, SOL_SOCKET, SO_RCVBUF, &big, sizeof big);
    char payload[1000]; memset(payload, 'x', sizeof payload);
    for (int i = 0; i < 1000; i++) sendto(tx, payload, sizeof payload, 0, (struct sockaddr *)&addr, sizeof addr);       /* 1000 datagrams of 1000 payload bytes */
    usleep(500000); counters("lo", &b); double dt = now() - t0;
    unsigned long long txp = b.tx_packets - a.tx_packets, rxp = b.rx_packets - a.rx_packets, txb = b.tx_bytes - a.tx_bytes;
    printf("   packets sent: %llu, packets received: %llu (loopback counts each packet once in each direction)\n", txp, rxp);
    printf("   bytes counted per packet: %.0f  (1000 payload + 8 UDP + 20 IP = %d; Linux counts the IP packet, not the Ethernet header)\n", (double)txb / (double)txp, 1000 + 8 + 20);
    printf("   average rate over %.1f s: %.1f Mbit/s; errors %llu, drops %llu\n", dt, (double)txb * 8 / dt / 1e6, (b.tx_errs - a.tx_errs) + (b.rx_errs - a.rx_errs), (b.tx_drop - a.tx_drop) + (b.rx_drop - a.rx_drop));

    printf("\n2. a 32-bit counter wraps: computing the delta correctly\n");
    printf("   old 4294967000, new 500 -> %u bytes (not -4294966500)\n", delta32(4294967000u, 500u));
    printf("   at 1 Gbit/s a 32-bit byte counter wraps every %.1f seconds, so poll faster than that or use 64-bit counters (ifHCInOctets)\n", 4294967296.0 / (1e9 / 8));

    printf("\n3. NTP: offset and delay from four timestamps\n");
    double T1 = 100.000, T2 = 100.130, T3 = 100.131, T4 = 100.061;                  /* client send, server receive, server send, client receive */
    double offset = ((T2 - T1) + (T3 - T4)) / 2, delay = (T4 - T1) - (T3 - T2);
    printf("   offset = ((T2-T1)+(T3-T4))/2 = %.1f ms (the client clock is %s the server)\n", offset * 1000, offset > 0 ? "behind" : "ahead of");
    printf("   round-trip delay = (T4-T1)-(T3-T2) = %.1f ms; the offset error can be at most half of that: +/-%.1f ms\n", delay * 1000, delay / 2 * 1000);

    printf("\n4. syslog priority = facility * 8 + severity\n");
    const char *sevs[] = {"emerg", "alert", "crit", "err", "warning", "notice", "info", "debug"};
    for (int pri = 0, list[] = {13, 34, 165, 190}; pri < 4; pri++) {
        int fac = list[pri] / 8, sev = list[pri] % 8;
        const char *name = fac == 1 ? "user" : fac == 4 ? "auth" : fac == 16 ? "local0" : fac == 20 ? "local4" : fac == 23 ? "local7" : "?";
        printf("   <%d> -> facility %d (%s), severity %d (%s)\n", list[pri], fac, name, sev, sevs[sev]);
    }
    return 0;
}
