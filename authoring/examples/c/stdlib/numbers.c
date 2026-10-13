// Converting text to numbers: atoi vs strtol, and the comparator bug in qsort.
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

// Correct parse: reports whether the whole string was a valid in-range integer.
static int parse_long(const char *s, long *out) {
    char *end; errno = 0;
    long v = strtol(s, &end, 10);
    if (end == s) return -1;                       // no digits at all
    if (*end != '\0') return -2;                   // trailing garbage
    if (errno == ERANGE) return -3;                // overflow/underflow: v is LONG_MAX/LONG_MIN
    *out = v; return 0;
}

static int cmp_bad(const void *a, const void *b)  { return *(const int *)a - *(const int *)b; }          // overflows for large values
static int cmp_good(const void *a, const void *b) { int x = *(const int *)a, y = *(const int *)b; return (x > y) - (x < y); }

int main(void) {
    const char *inputs[] = {"42", "  -17", "12abc", "abc", "", "99999999999999999999", "0x1f"};
    printf("atoi(\"abc\") = %d, atoi(\"12abc\") = %d  (atoi cannot report errors)\n", atoi("abc"), atoi("12abc"));
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; i++) {
        long v = 0; int rc = parse_long(inputs[i], &v);
        printf("parse_long(\"%s\") -> %s", inputs[i], rc == 0 ? "ok" : rc == -1 ? "no digits" : rc == -2 ? "trailing garbage" : "out of range");
        if (rc == 0) printf(" %ld", v);
        printf("\n");
    }
    printf("strtol(\"0x1f\", base 16) = %ld, base 0 (auto-detect) = %ld\n", strtol("0x1f", NULL, 16), strtol("0x1f", NULL, 0));

    int v[] = {INT_MAX, -5, 3, INT_MIN, 0, 100};
    int n = sizeof v / sizeof *v, w[6]; for (int i = 0; i < n; i++) w[i] = v[i];
    qsort(v, n, sizeof *v, cmp_bad);  qsort(w, n, sizeof *w, cmp_good);
    printf("qsort with 'a - b' comparator: "); for (int i = 0; i < n; i++) printf("%d ", v[i]);
    printf("\nqsort with (a>b)-(a<b):        "); for (int i = 0; i < n; i++) printf("%d ", w[i]);
    printf("\n");
    return 0;
}
