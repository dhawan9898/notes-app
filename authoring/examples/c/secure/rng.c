#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/random.h>
#include <time.h>

int main(void) {
    for (int run = 0; run < 2; run++) {
        srand(1700000000);                              /* an attacker who knows the approximate seed time reproduces everything */
        printf("rand() with a guessable seed, run %d: %d %d %d\n", run, rand(), rand(), rand());
    }
    unsigned char token[8];
    if (getrandom(token, sizeof token, 0) != (ssize_t)sizeof token) return 1;     /* kernel CSPRNG: blocks only until initialized */
    printf("getrandom token: ");
    for (size_t i = 0; i < sizeof token; i++) printf("%02x", token[i]);
    puts("");
    return 0;
}
