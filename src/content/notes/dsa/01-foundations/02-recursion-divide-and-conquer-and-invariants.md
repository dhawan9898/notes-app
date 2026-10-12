---
title: Recursion, Divide and Conquer, and Loop Invariants
track: dsa
module: Foundations
moduleOrder: 1
order: 2
level: intermediate
summary: How recursion maps to the call stack, the divide-and-conquer pattern with worked examples, why recursive code crashes at depth (and when the compiler removes the recursion), and how loop invariants turn "it seems to work" into a proof you can check at run time.
sources:
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 2.1 (Loop invariants), Ch. 4 (Divide-and-Conquer)"
  - label: "Gries — The Science of Programming (Springer, 1981), Ch. 11 (Developing loops from invariants)"
  - label: "Dijkstra — A Discipline of Programming (Prentice-Hall, 1976)"
  - label: "Bentley — Programming Pearls, 2nd ed., Column 4 (Writing Correct Programs; the binary search bug)"
  - label: "Bloch — Extra, Extra - Read All About It: Nearly All Binary Searches and Mergesorts are Broken (Google Research blog, 2006)"
    url: https://research.google/blog/extra-extra-read-all-about-it-nearly-all-binary-searches-and-mergesorts-are-broken/
  - label: "GCC manual — Optimize Options (-foptimize-sibling-calls, tail-call elimination)"
    url: https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
---

Recursion and loops are two notations for repetition. Recursion expresses *self-similar* problems directly: solve a smaller instance, combine the answers. Loop invariants are the tool that makes any repetition — recursive or iterative — provably correct. Together they are the foundation for sorting, searching, trees, graphs and dynamic programming.

## Anatomy of a recursive function

1. **Base case(s)**: inputs solved directly, with no further recursion. A missing or unreachable base case means infinite recursion.
2. **Recursive case**: reduce the problem to *strictly smaller* instances of the same problem (smaller `n`, a sub-range, a child node) and combine the results.
3. **Progress measure**: something that decreases on every call and is bounded below (e.g. `n`, `hi - lo`, remaining nodes). It proves termination.

Correctness argument by **induction**: assume the function is correct for all smaller inputs (induction hypothesis); show the combine step produces the correct answer for the current input; check the base case. If you can state the induction hypothesis in one sentence ("`hanoi(n-1, a, b, c)` moves n−1 disks from a to b"), the code almost writes itself.

## How recursion runs: the call stack

Every call pushes a **stack frame** (return address, saved registers, parameters, locals; see the C stack-frames note). Depth `d` consumes `d` frames. The recursion tree describes the calls; the *stack* holds only the current root-to-node path, so stack space is O(depth), while time is the number of nodes in the tree.

| Recursion shape | Calls | Depth | Example |
|---|---|---|---|
| Linear (one call per level) | n | n | list sum, factorial |
| Halving (one call, n/2) | log n | log n | binary search, fast exponentiation |
| Binary tree (two calls, n/2 each) | n | log n | merge sort, balanced tree traversal |
| Unbalanced binary (two calls, n−1 and n−2) | exponential | n | naive Fibonacci |
| Branching with choices | exponential | n | subsets, permutations, backtracking |

## Four classics, tested

`classics.c` implements binary exponentiation, the Towers of Hanoi, Euclid's gcd and subset enumeration, counting calls:

```c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static unsigned long calls;

/* Binary exponentiation: x^n in O(log n) multiplications (divide and conquer on the exponent). */
static uint64_t power_naive(uint64_t x, unsigned n) { calls++; return n == 0 ? 1 : x * power_naive(x, n - 1); }
static uint64_t power_fast(uint64_t x, unsigned n) {
    calls++;
    if (n == 0) return 1;
    uint64_t half = power_fast(x, n / 2);
    return n % 2 ? half * half * x : half * half;
}

/* Tower of Hanoi: T(n) = 2T(n-1) + 1  =>  2^n - 1 moves; the recursion IS the proof of the algorithm. */
static unsigned long moves;
static void hanoi(int n, char from, char to, char via) {
    if (n == 0) return;
    hanoi(n - 1, from, via, to);
    moves++;
    hanoi(n - 1, via, to, from);
}

/* Greatest common divisor: Euclid. The recursion shrinks (a, b) -> (b, a mod b); depth <= ~1.44 log2(b). */
static unsigned gcd(unsigned a, unsigned b) { calls++; return b ? gcd(b, a % b) : a; }

/* Generate all subsets by recursion (include / exclude): 2^n leaves. */
static void subsets(const char *items, int n, int i, char *cur, int len, unsigned long *count) {
    if (i == n) { (*count)++; return; }
    subsets(items, n, i + 1, cur, len, count);                  /* exclude items[i] */
    cur[len] = items[i];
    subsets(items, n, i + 1, cur, len + 1, count);              /* include items[i] */
}

int main(void) {
    calls = 0; uint64_t a = power_naive(3, 30);
    unsigned long c1 = calls;
    calls = 0; uint64_t b = power_fast(3, 30);
    printf("3^30 = %llu (both agree: %s)  naive calls=%lu  fast calls=%lu\n", (unsigned long long)b, a == b ? "yes" : "NO", c1, calls);

    for (int n = 5; n <= 25; n += 5) { moves = 0; hanoi(n, 'A', 'C', 'B'); printf("hanoi(%2d): %9lu moves  (2^n - 1 = %lu)\n", n, moves, (1UL << n) - 1); }

    calls = 0; unsigned g = gcd(1836311903u, 1134903170u);        /* consecutive Fibonacci numbers: worst case for Euclid */
    printf("gcd(F46, F45) = %u after %lu calls (log2 of the input ~ 30)\n", g, calls);

    char cur[16]; unsigned long count = 0;
    subsets("abcdefghijklmnop", 16, 0, cur, 0, &count);
    printf("subsets of 16 items: %lu\n", count);
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra classics.c -o classics && ./classics
3^30 = 205891132094649 (both agree: yes)  naive calls=31  fast calls=6
hanoi( 5):        31 moves  (2^n - 1 = 31)
hanoi(10):      1023 moves  (2^n - 1 = 1023)
hanoi(15):     32767 moves  (2^n - 1 = 32767)
hanoi(20):   1048575 moves  (2^n - 1 = 1048575)
hanoi(25):  33554431 moves  (2^n - 1 = 33554431)
gcd(F46, F45) = 1 after 45 calls (log2 of the input ~ 30)
subsets of 16 items: 65536
```

- **Fast exponentiation** (`x^n` via `x^(n/2)` squared) needs 6 calls for n = 30 where the naive linear recursion needs 31: the exponent halves, so O(log n) multiplications. The same trick with modular multiplication underlies RSA and Diffie–Hellman, and with matrices gives O(log n) Fibonacci. Note the single recursive call: computing `power(x, n/2)` **once** and squaring it; writing `power(x, n/2) * power(x, n/2)` would silently turn O(log n) back into O(n).
- **Hanoi**: `T(n) = 2T(n−1) + 1` solves to `2ⁿ − 1` moves, matching the measured 33,554,431 at n = 25. The recursion is not an inefficiency here: the output itself has 2ⁿ−1 moves, so no algorithm can be faster.
- **Euclid's gcd** takes at most ~1.44·log₂(b) steps; consecutive Fibonacci numbers are the worst case (45 calls for ~2·10⁹ inputs).
- **Subsets**: two recursive calls per item gives 2ⁿ leaves (65,536 for n = 16): inherent to the problem. Pruning is the topic of the backtracking note.

## Divide and conquer

The pattern: **divide** the problem into independent subproblems of smaller size, **conquer** them recursively (base case: solve directly), **combine** the sub-results. The running time follows the recurrence `T(n) = aT(n/b) + f(n)` (see the complexity note).

| Algorithm | Divide | Combine | Time |
|---|---|---|---|
| Merge sort | split array in half | merge two sorted halves (linear) | Θ(n log n) |
| Quicksort | partition around a pivot | nothing (in place) | Θ(n log n) expected |
| Binary search | choose half by comparison | nothing | Θ(log n) |
| Quickselect | partition around pivot, recurse into one side | nothing | Θ(n) expected |
| Closest pair of points | split by x, recurse | check the strip near the split (linear) | Θ(n log n) |
| Karatsuba multiplication | split digits | 3 multiplications instead of 4 | Θ(n^1.585) |
| FFT | even/odd indices | butterfly combine | Θ(n log n) |
| Tree algorithms | left / right subtree | combine child results | Θ(n) |
| Maximum subarray | halves + crossing sum | max of three | Θ(n log n) |

Design tips:

- Make subproblems **independent** (disjoint). Overlapping subproblems (Fibonacci, edit distance) call for memoization/dynamic programming instead.
- Prefer **balanced** splits: unbalanced ones turn log n depth into n.
- Use a **cutoff**: below ~16–64 elements switch to a simple algorithm (insertion sort) — less call overhead, better cache use; real library sorts do this.
- Recursion exposes **parallelism**: independent halves can run on separate threads (fork/join), with the combine step as the sequential part.
- Pass **ranges (pointer, length / lo, hi)** instead of copying subarrays.

## Recursion versus iteration, and the stack limit

Any recursion can be converted to iteration with an explicit stack (the call stack is just a stack). In C the practical reason to do so is the **finite stack** (8 MiB by default): a recursion of depth 1,000,000 over a linked list crashes. Three versions of "sum a list" (`deep.c`), compiled at `-O0` and `-O2`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>

struct node { int value; struct node *next; };

static long sum_recursive(const struct node *n) { return n ? n->value + sum_recursive(n->next) : 0; }   /* not a tail call: addition after the call */
static long sum_tail(const struct node *n, long acc) { return n ? sum_tail(n->next, acc + n->value) : acc; } /* tail call: -O2 turns it into a loop */
static long sum_iterative(const struct node *n) { long s = 0; for (; n; n = n->next) s += n->value; return s; }

int main(int argc, char **argv) {
    long count = argc > 1 ? atol(argv[1]) : 100000;
    struct node *head = NULL;
    for (long i = 0; i < count; i++) { struct node *n = malloc(sizeof *n); n->value = 1; n->next = head; head = n; }
    printf("list of %ld nodes\n", count);
    printf("iterative: %ld\n", sum_iterative(head));
    printf("tail-recursive (-O2 makes it a loop): %ld\n", sum_tail(head, 0));
    fflush(stdout);
    printf("recursive: %ld\n", sum_recursive(head));
    return 0;
}
```

```text
$ gcc -O0 -o deep_O0 deep.c && gcc -O2 -o deep_O2 deep.c && echo '--- -O0, 100,000 nodes:' && ./deep_O0 100000 && echo '--- -O0, 1,000,000 nodes:'; ./deep_O0 1000000; echo "exit status: $?"; echo '--- -O2, 1,000,000 nodes:'; ./deep_O2 1000000; echo "exit status: $?"
--- -O0, 100,000 nodes:
list of 100000 nodes
iterative: 100000
tail-recursive (-O2 makes it a loop): 100000
recursive: 100000
--- -O0, 1,000,000 nodes:
exit status: 139
--- -O2, 1,000,000 nodes:
list of 1000000 nodes
iterative: 1000000
tail-recursive (-O2 makes it a loop): 1000000
recursive: 1000000
exit status: 0
/bin/bash: line 1: 3002427 Segmentation fault      (core dumped) ./deep_O0 1000000
```

- At `-O0` each call really pushes a frame: 100,000 nodes fit, 1,000,000 do not — the process dies with `SIGSEGV` (status 139) when the stack is exhausted. The failure occurs in the first recursive version it reaches (the tail-recursive one: `-O0` does not optimize tail calls).
- At `-O2` GCC performs **tail-call elimination** (`sum_tail` becomes a loop) and, with its accumulator transformation, even the plain `sum_recursive` (`n->value + sum_recursive(...)`) is rewritten into a loop, so all three run in constant stack. **C does not guarantee this**: the standard has no tail-call requirement, so correctness must not depend on the optimizer. (Scheme guarantees tail calls; Clang offers `[[clang::musttail]]`.)
- Therefore: recursion on **data of unbounded depth** (linked lists, degenerate trees, graphs from user input, parsers of nested input) needs either an iterative version, an explicit stack in the heap, a depth limit with an error, or a larger stack (`ulimit -s`, threads with custom stacks). Balanced recursion (depth log n) is always safe.

Converting a recursion to an explicit stack (pattern):

```c
struct frame { struct node *n; int state; };
struct frame stack[MAX]; int sp = 0;
stack[sp++] = (struct frame){ root, 0 };
while (sp) {
    struct frame *f = &stack[sp - 1];
    if (!f->n) { sp--; continue; }
    /* state 0: before left, 1: after left (visit), 2: after right */
    switch (f->state++) {
    case 0: stack[sp++] = (struct frame){ f->n->left, 0 }; break;
    case 1: visit(f->n); stack[sp++] = (struct frame){ f->n->right, 0 }; break;
    default: sp--;
    }
}
```

(In-order traversal; the `state` field is the "return address".) For trees, Morris traversal achieves O(1) extra space by temporarily threading pointers (covered in the trees note).

### Memoization: recursion with a cache

When subproblems repeat (Fibonacci, grid paths, edit distance), store results: `if (memo[n]) return memo[n];`. This turns exponential recursion into linear or polynomial time (measured in the complexity note: 29.9 million calls vs 69). Two caveats: memo tables need keys that capture the entire subproblem state, and top-down memoization still has recursion depth `n` (use bottom-up tabulation when depth could be large).

## Loop invariants: proving repetition correct

A **loop invariant** is a statement about the program state that is true **before the loop, after every iteration, and therefore when the loop exits**. A proof has three parts (CLRS):

1. **Initialization**: the invariant holds before the first iteration.
2. **Maintenance**: if it holds before an iteration (and the loop condition holds), it still holds after.
3. **Termination**: when the loop ends, the invariant plus the negated loop condition imply the postcondition (the property you wanted). Also show the loop *does* end (a decreasing, bounded measure — the **variant**).

Recursion has the same structure: the induction hypothesis plays the role of the invariant.

### Invariants you can execute

`invariants.c` writes the invariants of insertion sort and of `lower_bound` binary search as `assert`s and runs 2,000 random arrays through them:

```c
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Insertion sort annotated with its loop invariants as runtime assertions. */
static int is_sorted(const int *a, int n) { for (int i = 1; i < n; i++) if (a[i - 1] > a[i]) return 0; return 1; }

static void insertion_sort(int *a, int n) {
    for (int i = 1; i < n; i++) {
        assert(is_sorted(a, i));                       /* INVARIANT: a[0..i) is sorted at the start of every iteration */
        int key = a[i], j = i - 1;
        while (j >= 0 && a[j] > key) { a[j + 1] = a[j]; j--; }
        a[j + 1] = key;
        assert(is_sorted(a, i + 1));                   /* after the body the invariant holds for i+1 (maintenance) */
    }
    assert(is_sorted(a, n));                           /* termination: i == n, so the whole array is sorted */
}

/* Binary search lower_bound with the invariant: every index < lo has a[idx] < key; every index >= hi has a[idx] >= key. */
static int lower_bound(const int *a, int n, int key) {
    int lo = 0, hi = n;
    while (lo < hi) {
        for (int i = 0; i < lo; i++) assert(a[i] < key);
        for (int i = hi; i < n; i++) assert(a[i] >= key);
        int mid = lo + (hi - lo) / 2;
        if (a[mid] < key) lo = mid + 1; else hi = mid;
    }
    return lo;                                         /* lo == hi: first index with a[idx] >= key */
}

int main(void) {
    srand(7);
    long trials = 0;
    for (int t = 0; t < 2000; t++) {
        int n = rand() % 40, a[40];
        for (int i = 0; i < n; i++) a[i] = rand() % 20;
        insertion_sort(a, n);
        int key = rand() % 22, pos = lower_bound(a, n, key);
        assert(pos == 0 || a[pos - 1] < key);
        assert(pos == n || a[pos] >= key);
        trials++;
    }
    printf("%ld random arrays: all invariants and postconditions held\n", trials);
    int broken[] = {3, 1, 2};
    int ok = is_sorted(broken, 3);
    printf("is_sorted({3,1,2}) = %d  (an assertion on this would have fired)\n", ok);
    return 0;
}
```

```text
$ gcc -O1 -Wall -Wextra -fsanitize=address,undefined invariants.c -o invariants && ./invariants
2000 random arrays: all invariants and postconditions held
is_sorted({3,1,2}) = 0  (an assertion on this would have fired)
```

- **Insertion sort**: *a[0..i) is sorted* at the start of iteration `i`. Initialization: a one-element prefix is sorted. Maintenance: inserting `a[i]` by shifting larger elements right keeps the prefix sorted. Termination: at `i = n` the whole array is sorted.
- **`lower_bound`** (first index with `a[idx] >= key`): *every index below `lo` holds a value `< key`, every index at or above `hi` holds a value `>= key`.* Each step moves `lo` or `hi` to `mid` while preserving that statement; the interval `[lo, hi)` shrinks every iteration (variant: `hi − lo`), and at `lo == hi` the invariant says `lo` is exactly the boundary. Writing the loop from the invariant avoids the usual off-by-one bugs: the half-open interval `[lo, hi)` and `hi = mid` (not `mid − 1`) fall out of it.

Run such assertions in debug builds and tests (they are O(n) per iteration, so never in production hot paths) — combined with randomized tests against a simple reference implementation (a **test oracle**), they find most algorithmic bugs quickly.

### The famous binary search bug

For decades `int mid = (lo + hi) / 2;` appeared in textbooks and the JDK; it overflows when `lo + hi > INT_MAX` (arrays larger than 2³⁰ elements), producing a negative index (found in 2006, after being in Java's library since 1997). The invariant doesn't catch arithmetic overflow, only the logic; the safe forms are `lo + (hi - lo) / 2` or `(unsigned)(lo + hi) >> 1`. Lesson: prove the algorithm *and* check the arithmetic's domain.

## Writing correct loops: a checklist

1. State the postcondition first, then pick an invariant that implies it when the loop ends.
2. Use half-open ranges `[lo, hi)` consistently; the length is `hi − lo`, empty is `lo == hi`.
3. Choose the loop condition and update so the variant strictly decreases.
4. Initialize the accumulator so the invariant holds trivially for the empty prefix (sum starts at 0, product at 1, min at `a[0]` or +∞).
5. Test the boundaries: n = 0, 1, 2, all equal, already sorted, reversed, key at both ends, key absent.
6. When tempted by "off-by-one" patching, return to the invariant instead of adjusting constants until tests pass.

## Key takeaways

- A recursive function needs a base case, strictly smaller subproblems and a combine step; prove it by induction. Stack space is O(depth), time is the size of the recursion tree.
- Divide and conquer needs independent, balanced subproblems and a cheap combine (merge sort, quicksort, binary search, FFT); overlapping subproblems need memoization or dynamic programming.
- C has no guaranteed tail-call elimination: GCC removed the recursion at `-O2` but 1,000,000-deep recursion at `-O0` crashed with `SIGSEGV`. Use iteration or an explicit stack for unbounded depth.
- Loop invariants (initialization, maintenance, termination, plus a variant) prove correctness; encoded as assertions and checked on randomized inputs they catch bugs early.
- Mind arithmetic domains: `(lo + hi) / 2` overflows; use `lo + (hi - lo) / 2`.
