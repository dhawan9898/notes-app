#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- 1. balanced brackets with a stack ---- */
static int balanced(const char *s) {
    char st[256]; int sp = 0;
    for (; *s; s++) {
        if (strchr("([{", *s)) { if (sp == 256) return 0; st[sp++] = *s; }
        else if (strchr(")]}", *s)) {
            if (!sp) return 0;
            char open = st[--sp];
            if ((*s == ')' && open != '(') || (*s == ']' && open != '[') || (*s == '}' && open != '{')) return 0;
        }
    }
    return sp == 0;
}

/* ---- 2. shunting-yard: infix -> value, with precedence and right-associative ^ ---- */
static int prec(char op) { return op == '^' ? 3 : (op == '*' || op == '/') ? 2 : (op == '+' || op == '-') ? 1 : 0; }
static double apply(char op, double a, double b) { switch (op) { case '+': return a + b; case '-': return a - b; case '*': return a * b; case '/': return a / b; default: return pow(a, b); } }
static double evaluate(const char *s) {
    double vals[64]; int nv = 0; char ops[64]; int no = 0;
    #define REDUCE() do { double b = vals[--nv], a = vals[--nv]; vals[nv++] = apply(ops[--no], a, b); } while (0)
    for (; *s; ) {
        if (isspace((unsigned char)*s)) { s++; continue; }
        if (isdigit((unsigned char)*s) || *s == '.') { char *end; vals[nv++] = strtod(s, &end); s = end; continue; }
        if (*s == '(') ops[no++] = *s++;
        else if (*s == ')') { while (ops[no - 1] != '(') REDUCE(); no--; s++; }
        else {
            char op = *s++;
            while (no && ops[no - 1] != '(' && (prec(ops[no - 1]) > prec(op) || (prec(ops[no - 1]) == prec(op) && op != '^'))) REDUCE();
            ops[no++] = op;
        }
    }
    while (no) REDUCE();
    return vals[0];
}

/* ---- 3. next greater element with a monotonic stack: O(n) ---- */
static void next_greater(const int *a, int n, int *out) {
    int st[1024], sp = 0;
    for (int i = 0; i < n; i++) out[i] = -1;
    for (int i = 0; i < n; i++) {
        while (sp && a[st[sp - 1]] < a[i]) out[st[--sp]] = a[i];       /* a[i] is the answer for every smaller element waiting on the stack */
        st[sp++] = i;
    }
}
static void next_greater_brute(const int *a, int n, int *out) { for (int i = 0; i < n; i++) { out[i] = -1; for (int j = i + 1; j < n; j++) if (a[j] > a[i]) { out[i] = a[j]; break; } } }

/* ---- 4. sliding window maximum with a monotonic deque of indices: O(n) ---- */
static void window_max(const int *a, int n, int k, int *out) {
    int dq[1024], head = 0, tail = 0;                                   /* indices; values decreasing from head to tail */
    for (int i = 0; i < n; i++) {
        while (tail > head && a[dq[tail - 1]] <= a[i]) tail--;           /* smaller elements can never be a maximum again */
        dq[tail++] = i;
        if (dq[head] <= i - k) head++;                                   /* expired */
        if (i >= k - 1) out[i - k + 1] = a[dq[head]];
    }
}
static void window_max_brute(const int *a, int n, int k, int *out) { for (int i = 0; i + k <= n; i++) { int m = a[i]; for (int j = i; j < i + k; j++) if (a[j] > m) m = a[j]; out[i] = m; } }

/* ---- 5. largest rectangle in a histogram with a monotonic stack: O(n) ---- */
static long largest_rect(const int *h, int n) {
    int st[1024], sp = 0; long best = 0;
    for (int i = 0; i <= n; i++) {
        int cur = i == n ? 0 : h[i];
        while (sp && h[st[sp - 1]] >= cur) {
            long height = h[st[--sp]];
            long width = sp ? i - st[sp - 1] - 1 : i;
            if (height * width > best) best = height * width;
        }
        st[sp++] = i;
    }
    return best;
}
static long largest_rect_brute(const int *h, int n) { long best = 0; for (int i = 0; i < n; i++) { int m = h[i]; for (int j = i; j < n; j++) { if (h[j] < m) m = h[j]; if ((long)m * (j - i + 1) > best) best = (long)m * (j - i + 1); } } return best; }

int main(void) {
    const char *tests[] = { "([]{()})", "([)]", "((", "" };
    for (int i = 0; i < 4; i++) printf("balanced(\"%s\") = %d\n", tests[i], balanced(tests[i]));
    printf("3 + 4 * 2 / (1 - 5) ^ 2 ^ 3 = %.10f   (expected 3.0001220703)\n", evaluate("3 + 4 * 2 / ( 1 - 5 ) ^ 2 ^ 3"));
    printf("(1+2)*(3+4)-10/4 = %g\n", evaluate("(1+2)*(3+4)-10/4"));

    srand(3);
    int checks = 0;
    for (int t = 0; t < 3000; t++) {
        int n = 1 + rand() % 40, a[40], o1[40], o2[40];
        for (int i = 0; i < n; i++) a[i] = rand() % 15;
        next_greater(a, n, o1); next_greater_brute(a, n, o2); assert(!memcmp(o1, o2, n * sizeof(int)));
        int k = 1 + rand() % n;
        window_max(a, n, k, o1); window_max_brute(a, n, k, o2); assert(!memcmp(o1, o2, (n - k + 1) * sizeof(int)));
        assert(largest_rect(a, n) == largest_rect_brute(a, n));
        checks += 3;
    }
    printf("%d randomized comparisons of O(n) monotonic-stack/deque algorithms against brute force: all equal\n", checks);
    int h[] = {2, 1, 5, 6, 2, 3};
    printf("largest rectangle in {2,1,5,6,2,3} = %ld\n", largest_rect(h, 6));
    return 0;
}
