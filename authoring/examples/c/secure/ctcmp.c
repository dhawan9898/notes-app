#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Constant-time equality: examine EVERY byte, accumulate differences, no early exit. */
static int ct_equal(const void *a, const void *b, size_t n) {
    const volatile unsigned char *x = a, *y = b;
    unsigned char diff = 0;
    for (size_t i = 0; i < n; i++) diff |= x[i] ^ y[i];
    return diff == 0;
}

int main(void) {
    const char secret[] = "s3cr3t-token-value", guess1[] = "x3cr3t-token-value", guess2[] = "s3cr3t-token-valuX";
    printf("memcmp   guess1: %d  guess2: %d  (time depends on how many leading bytes matched)\n", memcmp(secret, guess1, sizeof secret) == 0, memcmp(secret, guess2, sizeof secret) == 0);
    printf("ct_equal guess1: %d  guess2: %d  (always scans all bytes)\n", ct_equal(secret, guess1, sizeof secret), ct_equal(secret, guess2, sizeof secret));
    return 0;
}
