#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int parse_records(const uint8_t *data, size_t size);

/* xorshift64: tiny deterministic PRNG so the run is reproducible */
static uint64_t rng = 88172645463325252ULL;
static uint64_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

#define MAXLEN 128

/* Apply 1-4 random mutations: bit flips, interesting values, insertions and chunk duplication. */
static size_t mutate(uint8_t *buf, size_t n) {
    int muts = 1 + (int)(rnd() % 4);
    for (int m = 0; m < muts; m++) {
        size_t i = (size_t)(rnd() % n);
        switch (rnd() % 5) {
        case 0: buf[i] ^= (uint8_t)(1u << (rnd() % 8)); break;
        case 1: buf[i] = (uint8_t)rnd(); break;
        case 2: buf[i] = (uint8_t[]){0, 1, 0x7f, 0x80, 0xff, 16, 17, 32}[rnd() % 8]; break;
        case 3:                                                       /* insert a random byte */
            if (n < MAXLEN) { memmove(buf + i + 1, buf + i, n - i); buf[i] = (uint8_t)rnd(); n++; }
            break;
        case 4: {                                                     /* duplicate a chunk */
            size_t len = 1 + (size_t)(rnd() % 8);
            if (i + len <= n && n + len <= MAXLEN) { memmove(buf + i + len, buf + i, n - i); n += len; }
            break;
        }
        }
    }
    return n;
}

static int run_one(const uint8_t *buf, size_t n) {       /* run the target in a child so a crash doesn't stop the fuzzer */
    pid_t pid = fork();
    if (pid == 0) { parse_records(buf, n); _exit(0); }
    int st; waitpid(pid, &st, 0);
    return WIFSIGNALED(st) || (WIFEXITED(st) && WEXITSTATUS(st) != 0);
}

int main(int argc, char **argv) {
    if (argc > 2 && !strcmp(argv[1], "replay")) {            /* ./fuzz replay crash.bin */
        uint8_t buf[MAXLEN];
        FILE *f = fopen(argv[2], "rb");
        size_t n = f ? fread(buf, 1, sizeof buf, f) : 0;
        printf("replaying %zu bytes\n", n);
        printf("result=%d\n", parse_records(buf, n));
        return 0;
    }
    const uint8_t seed[] = { 'R', 4, 'a', 'b', 'c', 'd', 'R', 2, 'x', 'y', 'E' };   /* a valid seed input */
    uint8_t cur[MAXLEN];
    for (unsigned long iters = 1; iters <= 300000; iters++) {
        size_t n = sizeof seed;
        memcpy(cur, seed, n);
        n = mutate(cur, n);
        if (run_one(cur, n)) {
            FILE *f = fopen("crash.bin", "wb"); fwrite(cur, 1, n, f); fclose(f);
            printf("crash after %lu inputs; %zu-byte reproducer saved to crash.bin\n", iters, n);
            return 0;
        }
    }
    puts("no crash found");
    return 0;
}
