#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 1. Two-sum on a sorted array with two indices moving inward: O(n) */
static int two_sum_sorted(const int *a, int n, int target, int *i_out, int *j_out) { int i = 0, j = n - 1; while (i < j) { int s = a[i] + a[j]; if (s == target) { *i_out = i; *j_out = j; return 1; } if (s < target) i++; else j--; } return 0; }
/* 2. Palindrome ignoring case and non-letters */
static int is_palindrome(const char *s) { int i = 0, j = (int)strlen(s) - 1; while (i < j) { while (i < j && !isalpha((unsigned char)s[i])) i++; while (i < j && !isalpha((unsigned char)s[j])) j--; if (tolower((unsigned char)s[i]) != tolower((unsigned char)s[j])) return 0; i++; j--; } return 1; }
/* 3. Binary search: index of key in a sorted array, or -1 */
static int bsearch_idx(const int *a, int n, int key) { int lo = 0, hi = n - 1; while (lo <= hi) { int mid = lo + (hi - lo) / 2; if (a[mid] == key) return mid; if (a[mid] < key) lo = mid + 1; else hi = mid - 1; } return -1; }
/* 4. Are two words anagrams? Count letters. */
static int is_anagram(const char *a, const char *b) { int c[26] = {0}; for (; *a; a++) c[tolower((unsigned char)*a) - 'a']++; for (; *b; b++) c[tolower((unsigned char)*b) - 'a']--; for (int i = 0; i < 26; i++) if (c[i]) return 0; return 1; }
/* 5. Merge two sorted arrays into one sorted array */
static void merge(const int *a, int na, const int *b, int nb, int *out) { int i = 0, j = 0, k = 0; while (i < na && j < nb) out[k++] = a[i] <= b[j] ? a[i++] : b[j++]; while (i < na) out[k++] = a[i++]; while (j < nb) out[k++] = b[j++]; }
/* 6. Remove duplicates from a sorted array in place; returns the new length */
static int dedupe(int *a, int n) { if (n == 0) return 0; int w = 1; for (int r = 1; r < n; r++) if (a[r] != a[w - 1]) a[w++] = a[r]; return w; }
/* 7. Balanced brackets using a stack */
static int balanced(const char *s) { char st[128]; int sp = 0; for (; *s; s++) { if (strchr("([{", *s)) st[sp++] = *s; else if (strchr(")]}", *s)) { if (!sp) return 0; char o = st[--sp]; if ((*s == ')' && o != '(') || (*s == ']' && o != '[') || (*s == '}' && o != '{')) return 0; } } return sp == 0; }
/* 8. Fibonacci, three ways, all agreeing */
static long fib_rec(int n) { return n < 2 ? n : fib_rec(n - 1) + fib_rec(n - 2); }
static long fib_iter(int n) { long a = 0, b = 1; for (int i = 0; i < n; i++) { long t = a + b; a = b; b = t; } return a; }
/* 9. Reverse a singly linked list */
typedef struct Node { int v; struct Node *next; } Node;
static Node *reverse_list(Node *h) { Node *prev = NULL; while (h) { Node *nx = h->next; h->next = prev; prev = h; h = nx; } return prev; }
/* 10. Most frequent element (the "mode") with a counting array for values 0..99 */
static int mode_of(const int *a, int n) { int cnt[100] = {0}, best = 0; for (int i = 0; i < n; i++) cnt[a[i]]++; for (int v = 1; v < 100; v++) if (cnt[v] > cnt[best]) best = v; return best; }

int main(void) {
    int a[] = { 1, 3, 4, 7, 9, 12 }, i, j;
    assert(two_sum_sorted(a, 6, 16, &i, &j)); printf("Ex 1: two numbers summing to 16 are a[%d]=%d and a[%d]=%d; for 100: %s\n", i, a[i], j, a[j], two_sum_sorted(a, 6, 100, &i, &j) ? "found" : "none");
    printf("Ex 2: \"A man, a plan, a canal: Panama\" palindrome? %s;  \"hello\"? %s\n", is_palindrome("A man, a plan, a canal: Panama") ? "yes" : "no", is_palindrome("hello") ? "yes" : "no");
    printf("Ex 3: index of 9 = %d, of 8 = %d\n", bsearch_idx(a, 6, 9), bsearch_idx(a, 6, 8));
    printf("Ex 4: listen/silent anagrams? %s;  apple/pale? %s\n", is_anagram("listen", "silent") ? "yes" : "no", is_anagram("apple", "pale") ? "yes" : "no");
    int b[] = { 2, 3, 10 }, m[9]; merge(a, 6, b, 3, m); printf("Ex 5: merged:"); for (int k = 0; k < 9; k++) printf(" %d", m[k]); printf("\n");
    int d[] = { 1, 1, 2, 2, 2, 3, 5, 5 }; int nd = dedupe(d, 8); printf("Ex 6: after dedupe length %d:", nd); for (int k = 0; k < nd; k++) printf(" %d", d[k]); printf("\n");
    printf("Ex 7: \"{[()]}\" %s;  \"([)]\" %s;  \"((\" %s\n", balanced("{[()]}") ? "balanced" : "NOT", balanced("([)]") ? "balanced" : "NOT", balanced("((") ? "balanced" : "NOT");
    for (int n = 0; n <= 25; n++) assert(fib_rec(n) == fib_iter(n));
    printf("Ex 8: fib(25) = %ld (recursive and iterative agree for n = 0..25)\n", fib_iter(25));
    Node n3 = { 3, NULL }, n2 = { 2, &n3 }, n1 = { 1, &n2 }; Node *r = reverse_list(&n1); printf("Ex 9: reversed list:"); for (Node *p = r; p; p = p->next) printf(" %d", p->v); printf("\n");
    int data[] = { 4, 7, 4, 2, 7, 7, 9 }; printf("Ex 10: mode = %d\n", mode_of(data, 7));
    return 0;
}
