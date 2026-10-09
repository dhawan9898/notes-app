#include <stdint.h>
#include <stdio.h>
#include <stdckdint.h>

constexpr int BUF_SIZE = 4 * 1024;                  /* a true constant expression: usable for array sizes */
static char buffer[BUF_SIZE];

enum small : uint8_t { LOW = 1, HIGH = 200 };       /* fixed underlying type */

[[nodiscard]] static int checked_add(int a, int b) {
    int r;
    return ckd_add(&r, a, b) ? -1 : r;
}

[[noreturn]] static void fail(const char *msg) { fprintf(stderr, "fatal: %s\n", msg); __builtin_trap(); }

static void on_ptr(void *p) { printf("pointer is %s\n", p == nullptr ? "null" : "non-null"); }

int main(void) {
    bool flag = true;                                 /* bool, true, false are keywords */
    auto n = 1'000'000;                               /* type inference + digit separator: int */
    auto mask = 0b1010'0101u;                         /* binary literal */
    typeof(n) copy = n;                               /* typeof is standard */
    int arr[BUF_SIZE / 1024] = {};                    /* empty initializer */

    printf("flag=%d n=%d mask=0x%x copy=%d arr[3]=%d sizeof(buffer)=%zu\n", flag, n, mask, copy, arr[3], sizeof buffer);
    printf("sizeof(enum small)=%zu, HIGH=%d\n", sizeof(enum small), HIGH);
    on_ptr(nullptr);
    on_ptr(&n);

    int r = checked_add(2147483647, 1);
    printf("checked_add overflow -> %d\n", r);
    (void)checked_add(1, 2);                          /* result ignored on purpose: cast to void silences [[nodiscard]] */

    unsigned _BitInt(12) twelve = 4095;               /* exact-width integers */
    twelve++;                                         /* wraps at 12 bits */
    printf("_BitInt(12) 4095+1 = %u, sizeof=%zu\n", (unsigned)twelve, sizeof twelve);

    static_assert(sizeof(int) >= 4);                  /* message optional in C23 */
    if (n < 0) fail("negative");
    return 0;
}
