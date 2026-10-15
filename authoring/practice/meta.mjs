// Problem descriptions for the /practice/ pages (see scripts/build-practice.mjs).
// Key: path of the starter program below authoring/examples. The tests live in the starter; this file is what the reader sees next to the editor.
// statement: paragraphs separated by a blank line; `code` and **bold** are supported. examples: {input, output, why?}.
const c = (x) => x;
export default {
  /* ---------------------------------------------------------------- C ---- */
  'c/starters/start-here/ex3-swap.c': {
    slug: 'swap-ints', title: 'Swap two integers', difficulty: 'Easy', tags: ['pointers', 'pass by value'],
    statement: 'C passes every argument **by value**, so a function cannot change a caller’s variable through a copy. `swap_ints` receives the **addresses** of two `int` variables and must exchange the values stored there.\n\nThe code in the editor makes the classic mistake: it swaps its own copies of the two pointers and leaves the caller’s ints alone. Fix it.',
    examples: [
      { input: 'a = 1, b = 2\nswap_ints(&a, &b);', output: 'a = 2, b = 1' },
      { input: 'x = -7, y = 7\nswap_ints(&x, &y);', output: 'x = 7, y = -7' },
    ],
    constraints: ['Both pointers are valid and point to two different `int` variables.', 'Use a temporary variable; no helper functions or globals.'],
    hints: ['Assigning to the parameter `a` changes only the local copy of the pointer.', 'To change the variable the pointer refers to, write through it: `*a = ...`.'],
  },
  'c/starters/start-here/ex4-max.c': {
    slug: 'max-of-array', title: 'Largest element', difficulty: 'Easy', tags: ['arrays', 'loops'],
    statement: 'Write `int max_of(const int *v, int count)` that returns the largest element of the array `v`.',
    examples: [
      { input: 'v = {4, 9, -2, 7}', output: '9' },
      { input: 'v = {-5, -1, -7}', output: '-1', why: 'All elements are negative: starting the running maximum at 0 would give the wrong answer.' },
    ],
    constraints: ['`count >= 1`.', 'Elements can be any `int`, including negative values.', 'The largest element may be the last one, or the only one.'],
    hints: ['Start the running maximum with `v[0]`, not with 0.', 'Loop from index 1 and replace the maximum when `v[i]` is larger.'],
  },
  'c/starters/start-here/ex5-reverse.c': {
    slug: 'reverse-string', title: 'Reverse a string in place', difficulty: 'Easy', tags: ['strings', 'two pointers'],
    statement: 'Write `void reverse(char *s)` that reverses the NUL-terminated string `s` **in place**: no second buffer.',
    examples: [
      { input: '"stressed"', output: '"desserts"' },
      { input: '"hello world"', output: '"dlrow olleh"' },
      { input: '""', output: '""', why: 'The empty string has nothing to swap.' },
    ],
    constraints: ['`s` points to a writable, NUL-terminated array (not a string literal).', 'Odd and even lengths, a single character and the empty string must all work.'],
    hints: ['Find the length with `strlen`, then swap `s[i]` with `s[len - 1 - i]` for the first half only.', 'Stop when the two indices meet: swapping the whole string reverses it twice.'],
  },
  'c/starters/start-here/ex6-popcount.c': {
    slug: 'popcount', title: 'Count the 1 bits', difficulty: 'Easy', tags: ['bits'],
    statement: 'Write `int popcount(uint32_t x)` that returns the number of bits set to 1 in `x`.',
    examples: [
      { input: 'x = 255', output: '8' },
      { input: 'x = 0x80000001', output: '2', why: 'The highest and the lowest bit are set.' },
      { input: 'x = 0xFFFFFFFF', output: '32' },
    ],
    constraints: ['`x` is an unsigned 32-bit value: use unsigned shifts only.', 'Result is between 0 and 32.'],
    hints: ['`x & 1` is the lowest bit. Add it up, then shift `x` right by one until it is zero.', 'Faster: `x &= x - 1` clears the lowest set bit, so the loop runs once per set bit.'],
  },
  'c/starters/start-here/ex7-vec.c': {
    slug: 'growable-int-array', title: 'Growable array of ints', difficulty: 'Medium', tags: ['malloc', 'realloc', 'dynamic array'],
    statement: 'An `IntVec` has three fields: `data` (the elements), `size` (elements in use) and `cap` (elements allocated). A zero-initialised vector is a valid empty vector.\n\nImplement `vec_push`, which appends one value and **doubles the capacity** with `realloc` when the vector is full, and `vec_free`, which releases the memory and leaves the vector empty.',
    examples: [
      { input: 'IntVec v = {0};\nfor (i = 0; i < 10; i++) vec_push(&v, i * i);', output: 'v.size == 10, v.data[9] == 81' },
      { input: 'vec_free(&v);', output: 'v.data == NULL, v.size == 0' },
    ],
    constraints: ['`vec_push` returns 1 on success and 0 if memory runs out; on failure the vector must stay valid.', 'Start from `cap == 0`; grow by doubling (first allocation of a small fixed size is fine).', 'Never lose the old pointer if `realloc` fails.'],
    hints: ['Assign the result of `realloc` to a temporary, check it for NULL, and only then store it in `v->data`.', 'The new capacity is `v->cap ? v->cap * 2 : 4`.', 'After `free(v->data)` set `data = NULL` and `size = cap = 0`.'],
  },
  'c/starters/start-here/ex8-sum.c': {
    slug: 'fix-off-by-one', title: 'Fix the off-by-one sum', difficulty: 'Easy', tags: ['debugging', 'loops', 'arrays'],
    statement: '`sum_first_n(v, n)` should return the sum of the **first `n` elements** of `v`. It has an off-by-one bug that reads one element too many. Find and fix it.',
    examples: [
      { input: 'v = {1, 2, 3, 4, 99}, n = 4', output: '10', why: 'Only v[0] .. v[3] count; the 99 must not be added.' },
      { input: 'v = {1, 2, 3, 4, 99}, n = 0', output: '0' },
    ],
    constraints: ['`0 <= n <= length of v`.', 'Never read `v[n]`: on a full-length array that is out of bounds.'],
    hints: ['n elements means indexes 0 to n - 1.', 'Look at the loop condition: `<=` versus `<`.'],
  },
  'c/starters/start-here/ex9-network.c': {
    slug: 'network-address', title: 'Network address of an IPv4 prefix', difficulty: 'Medium', tags: ['bits', 'networking', 'masks'],
    statement: 'An IPv4 address is a 32-bit number and a prefix length `/p` says that the first `p` bits are the network part. Write `uint32_t network_of(uint32_t ip, int prefix)` that returns `ip` with the last `32 - prefix` host bits cleared. The macro `IP(a, b, c, d)` builds an address from its four bytes.',
    examples: [
      { input: '10.20.77.130 / 21', output: '10.20.72.0' },
      { input: '172.16.5.4 / 12', output: '172.16.0.0' },
      { input: '10.1.2.3 / 0', output: '0.0.0.0', why: 'A /0 mask is all zeros. A shift by 32 is undefined behaviour for a 32-bit value.' },
    ],
    constraints: ['`0 <= prefix <= 32`.', 'Do not shift a 32-bit value by 32 or more: handle `prefix == 0` separately or shift in 64 bits.'],
    hints: ['The mask for /p has p ones followed by zeros: `~0u << (32 - p)`.', 'Check /0 and /32: one of them makes the shift count 32.'],
  },
  'c/starters/types/ex3-safe-add.c': {
    slug: 'overflow-safe-add', title: 'Overflow-safe addition', difficulty: 'Medium', tags: ['undefined behaviour', 'integers'],
    statement: 'Signed integer overflow is undefined behaviour in C, so you cannot add first and look at the result afterwards. Write `bool safe_add(int a, int b, int *out)` that stores `a + b` in `*out` and returns `true` when the true sum fits in an `int`; otherwise it returns `false` and leaves `*out` untouched.\n\nNever perform an addition that overflows.',
    examples: [
      { input: 'a = 2000000000, b = -5', output: 'true, *out = 1999999995' },
      { input: 'a = 2000000000, b = 2000000000', output: 'false', why: 'The true sum 4000000000 is above INT_MAX.' },
      { input: 'a = INT_MIN, b = -1', output: 'false' },
    ],
    constraints: ['Any `int` values for `a` and `b`, including `INT_MIN` and `INT_MAX`.', 'Must not rely on undefined behaviour: the overflow check has to happen before the addition.'],
    hints: ['Rewrite `a + b > INT_MAX` as `a > INT_MAX - b`, which cannot overflow when `b > 0`.', 'Treat positive and negative `b` separately; use `INT_MIN - b` for the lower bound.', 'Alternatively add in `long long` and compare with the limits (an `int` is narrower than a `long long` on common platforms).'],
  },
  'c/starters/types/ex5-countdown.c': {
    slug: 'unsigned-countdown', title: 'The countdown that never ends', difficulty: 'Easy', tags: ['unsigned', 'loops', 'compiler warnings'],
    statement: '`countdown(n, out)` should record every index from `n - 1` down to `0` in `out[]` and return how many were visited. The loop in the editor never ends on its own (a safety cap stops it after 5 steps) and the compiler warns about the comparison. Find out why and rewrite it.',
    examples: [
      { input: 'n = 3', output: 'returns 3, out = {2, 1, 0}' },
      { input: 'n = 0', output: 'returns 0', why: 'For an unsigned n, `n - 1` wraps around to a huge number.' },
    ],
    constraints: ['`n` is a `size_t` (unsigned); `out` has room for `n` entries.', 'The result must be correct for `n == 0` as well.'],
    hints: ['`i >= 0` is always true for an unsigned `i`: after 0 it wraps to the maximum value.', 'Loop with `for (size_t i = n; i-- > 0; )` or test before decrementing.'],
  },
  'c/starters/pointers/ex2-greeting.c': {
    slug: 'return-a-greeting', title: 'Return a greeting safely', difficulty: 'Medium', tags: ['strings', 'dangling pointer', 'malloc'],
    statement: 'A function that builds `"Hello, <name>"` in a local array and returns its address returns a **dangling pointer**. Implement the two correct replacements:\n\n1. `make_greeting_buf(name, out, cap)` writes into a buffer the caller supplies and truncates if it does not fit.\n2. `make_greeting_heap(name)` returns a newly allocated string that the caller frees, or `NULL` if memory runs out.',
    examples: [
      { input: 'make_greeting_buf("Ada", buf, 32)', output: 'buf = "Hello, Ada"' },
      { input: 'make_greeting_buf("Grace", buf, 8)', output: 'buf = "Hello, "', why: 'Seven characters plus the terminating NUL fill the 8-byte buffer.' },
      { input: 'make_greeting_heap("Linus")', output: '"Hello, Linus" (heap)' },
    ],
    constraints: ['The buffer version must never write past `cap` bytes and must always NUL-terminate when `cap > 0`.', 'The heap version must allocate exactly enough for any name length.'],
    hints: ['`snprintf(out, cap, "Hello, %s", name)` truncates and terminates for you.', 'For the heap version, `snprintf(NULL, 0, ...)` returns the length you need.'],
  },
  'c/starters/pointers/ex3-leak.c': {
    slug: 'free-everything', title: 'Fix the memory leak', difficulty: 'Easy', tags: ['malloc', 'free', 'ownership'],
    statement: '`student_new` allocates a `Student` (a struct holding a name, an array of scores and a count) and two more blocks owned by it. `student_free` only releases one of them, so memory leaks.\n\nRewrite `student_free` so that **every** block `student_new` allocated is released. The tests count live allocations.',
    examples: [
      { input: 'Student *s = student_new("Ada", 3);', output: '3 blocks are allocated' },
      { input: 'student_free(s);', output: '0 blocks are still allocated' },
    ],
    constraints: ['Free each owned block exactly once, then the struct itself, which must be freed last.', 'No use after free: do not read `s` after releasing it.'],
    hints: ['Count the `malloc`/`calloc` calls in `student_new`.', 'Release the members first (`name`, `scores`), then the struct they live in.'],
  },
  'c/starters/pointers/ex5-builder.c': {
    slug: 'string-builder', title: 'Growable string builder', difficulty: 'Hard', tags: ['realloc', 'strings', 'error handling'],
    statement: 'A `Builder` has `data` (a NUL-terminated string), `len` (its length) and `cap` (bytes allocated). Implement `sb_init` and `sb_append`.\n\n`sb_append` doubles `cap` until the new text and its terminating NUL fit. If `realloc` fails, it returns 0 and leaves the builder **exactly as it was**: the old string must survive.',
    examples: [
      { input: 'sb_init(&b); sb_append(&b, "item-1;"); sb_append(&b, "item-2;");', output: 'b.data = "item-1;item-2;", b.len = 14' },
      { input: 'realloc fails during sb_append', output: 'returns 0; b.data and b.len unchanged' },
    ],
    constraints: ['`sb_init` and `sb_append` return 1 on success and 0 on failure.', 'There is always room for the terminating NUL: `cap > len` after every successful call.', 'Never assign the result of `realloc` straight to `b->data`.'],
    hints: ['Compute `need = b->len + strlen(s) + 1` before touching the buffer.', 'Grow into a local `char *p = realloc(...)`, check it, then update `data` and `cap`.', 'Use `memcpy` to append and write the NUL yourself.'],
  },
  'c/starters/callbacks/ex2-dispatch.c': {
    slug: 'dispatch-table', title: 'Dispatch table of function pointers', difficulty: 'Easy', tags: ['function pointers', 'tables'],
    statement: 'A `Command` pairs a name with a function pointer `int (*op)(int, int)`. Implement `add`, `sub` and `mul` and fill the table so that each entry points at its function. The tests find a command by name and call it through the pointer: no `switch`.',
    examples: [
      { input: 'table["add"].op(6, 3)', output: '9' },
      { input: 'table["sub"].op(6, 3)', output: '3' },
      { input: 'table["mul"].op(6, 3)', output: '18' },
    ],
    constraints: ['The three entries stay in the order add, sub, mul.', 'Entries hold the function name without parentheses: `{"add", add}`.'],
    hints: ['A function name by itself decays to a pointer to that function.', 'Replace each `NULL` in the table with the matching function.'],
  },
  'c/starters/callbacks/ex3-comparators.c': {
    slug: 'qsort-comparators', title: 'Comparators for qsort', difficulty: 'Medium', tags: ['qsort', 'callbacks', 'overflow'],
    statement: 'Write two `qsort` comparators for a `Person` (a name and an `int` age): `by_age` and `by_name`. Each returns a negative number, zero or a positive number, like `strcmp`.\n\nThe obvious `return a->age - b->age;` is **wrong**: the subtraction can overflow for extreme ages and then returns the wrong sign.',
    examples: [
      { input: 'ages {Linus 55, Ada 36, Big 2000000000, Small -2000000000}', output: 'Small Ada Linus Big (by age)' },
      { input: 'names {Linus, Ada, Dennis}', output: 'Ada Dennis Linus (by name)' },
    ],
    constraints: ['The comparator receives `const void *` pointing at the **elements** (here, pointers to `Person`).', 'Ages can be anywhere in the `int` range.'],
    hints: ['Cast: `const Person *a = x;`.', 'Compare with `(a->age > b->age) - (a->age < b->age)`.', 'For names, `strcmp(a->name, b->name)` already has the right contract.'],
  },
  'c/starters/bits/ex1-bit-toolkit.c': {
    slug: 'bit-toolkit', title: 'A bit toolkit for uint32_t', difficulty: 'Medium', tags: ['bits', 'masks', 'rotate'],
    statement: 'Implement seven small bit operations on `uint32_t`: `popcount32`, `is_pow2`, `rotl32` (rotate left), `set_bit`, `clear_bit`, `toggle_bit` and `byte_swap`.\n\nAll must be free of undefined behaviour: in particular a rotate by 0 and shifts by large counts need care.',
    examples: [
      { input: 'popcount32(0xF0F0)', output: '8' },
      { input: 'rotl32(0x80000001, 1)', output: '0x00000003' },
      { input: 'byte_swap(0x11223344)', output: '0x44332211' },
      { input: 'is_pow2(0)', output: '0 (false)', why: '0 is not a power of two even though `0 & (0 - 1) == 0`.' },
    ],
    constraints: ['Bit indexes `i` are 0 to 31; rotate counts `r` are 0 to 31.', '`rotl32(x, 0)` must return `x`: `x >> 32` is undefined.'],
    hints: ['A power of two has exactly one set bit: `x != 0 && (x & (x - 1)) == 0`.', 'Rotate: `(x << r) | (x >> ((32 - r) & 31))`.', 'Use `1u << i`, not `1 << i`, so bit 31 is not a signed shift.'],
  },
  'c/starters/bits/ex3-big-endian.c': {
    slug: 'big-endian-codec', title: 'Network byte order codec', difficulty: 'Medium', tags: ['endianness', 'bits', 'networking'],
    statement: 'Network protocols put the most significant byte first (**big-endian**). Implement `put_u16` and `put_u32`, which store a value into a byte buffer in that order, and `get_u32`, which reads it back. Use only shifts and masks: the result must not depend on the machine’s own byte order, and the buffer may be unaligned.',
    examples: [
      { input: 'put_u16(buf, 0xABCD)', output: 'buf = AB CD' },
      { input: 'get_u32({C0, A8, 01, 02})', output: '0xC0A80102', why: 'This is 192.168.1.2 as it travels on the wire.' },
    ],
    constraints: ['No casts of the buffer to `uint32_t *` (alignment and aliasing problems).', 'Promote the bytes to `uint32_t` before shifting: `p[0] << 24` on a promoted `int` can overflow.'],
    hints: ['`p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v;`', 'Build the value with `(uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | ...`.'],
  },

  /* -------------------------------------------------------------- DSA ---- */
  'dsa/starters/start-here/ex1-two-sum.c': {
    slug: 'two-sum-sorted', title: 'Two sum in a sorted array', difficulty: 'Easy', tags: ['two pointers', 'arrays'],
    statement: 'Given an array `a` sorted in ascending order and a `target`, find two elements at **different** indexes that add up to `target`. Store their indexes (`i < j`) in `*i_out` and `*j_out` and return 1, or return 0 if there is no such pair.\n\nDo better than checking every pair.',
    examples: [
      { input: 'a = {1, 3, 4, 7, 9, 12}, target = 16', output: '1, for example i = 2, j = 5', why: '4 + 12 = 16. Any valid pair is accepted, such as 7 + 9.' },
      { input: 'a = {1, 3, 4, 7, 9, 12}, target = 2', output: '0', why: 'An element cannot pair with itself.' },
    ],
    constraints: ['`0 <= n`; the array is sorted ascending and may contain duplicates.', 'Target O(n) time and O(1) extra memory.'],
    hints: ['Keep two indices, one at each end.', 'If the sum is too small move the left index right; if too big move the right index left.', 'Stop when the indices meet: `i < j` guarantees different elements.'],
  },
  'dsa/starters/start-here/ex2-palindrome.c': {
    slug: 'valid-palindrome', title: 'Valid palindrome', difficulty: 'Easy', tags: ['strings', 'two pointers'],
    statement: 'Is a sentence a palindrome if you ignore case and everything that is not a letter or a digit? Write `int is_palindrome(const char *s)` returning 1 or 0.',
    examples: [
      { input: '"A man, a plan, a canal: Panama"', output: '1' },
      { input: '"hello"', output: '0' },
      { input: '"!!"', output: '1', why: 'No letters at all: an empty comparison is trivially a palindrome.' },
    ],
    constraints: ['`s` is NUL-terminated and may be empty.', 'Pass `unsigned char` values to `<ctype.h>` functions.'],
    hints: ['Move two indices inward, skipping characters that are not alphanumeric (`isalnum`).', 'Compare with `tolower` on both sides.'],
  },
  'dsa/starters/start-here/ex3-binary-search.c': {
    slug: 'binary-search', title: 'Binary search', difficulty: 'Easy', tags: ['binary search', 'arrays'],
    statement: 'Return the index of `key` in the sorted array `a`, or -1 if it is not there.',
    examples: [
      { input: 'a = {1, 3, 4, 7, 9, 12}, key = 9', output: '4' },
      { input: 'a = {1, 3, 4, 7, 9, 12}, key = 8', output: '-1' },
      { input: 'a = {5}, n = 0, key = 5', output: '-1', why: 'The empty array contains nothing.' },
    ],
    constraints: ['`a` is sorted ascending, distinct values.', 'O(log n) time. Compute the middle as `lo + (hi - lo) / 2` to avoid overflow.'],
    hints: ['Keep a half-open range `[lo, hi)` and shrink it until it is empty.', 'The first and the last element are the classic places where the loop condition goes wrong.'],
  },
  'dsa/starters/start-here/ex4-anagram.c': {
    slug: 'valid-anagram', title: 'Anagrams', difficulty: 'Easy', tags: ['counting', 'strings'],
    statement: 'Are two words made of exactly the same letters, with the same counts? Case does not matter. The words contain only the letters a to z.',
    examples: [
      { input: '"listen", "silent"', output: '1' },
      { input: '"aab", "abb"', output: '0', why: 'Same letters, different counts.' },
      { input: '"Dusty", "study"', output: '1' },
    ],
    constraints: ['Only letters `a`–`z` or `A`–`Z`; both strings may be empty.', 'O(n) time, O(1) extra memory (26 counters).'],
    hints: ['Count letters of the first word up, letters of the second word down.', 'They are anagrams if every counter ends at zero.'],
  },
  'dsa/starters/start-here/ex5-merge.c': {
    slug: 'merge-sorted-arrays', title: 'Merge two sorted arrays', difficulty: 'Easy', tags: ['merge', 'arrays'],
    statement: 'Merge the sorted arrays `a` (length `na`) and `b` (length `nb`) into `out`, which has room for `na + nb` elements, in a single pass.',
    examples: [
      { input: 'a = {1, 3, 4, 7, 9, 12}, b = {2, 3, 10}', output: '{1, 2, 3, 3, 4, 7, 9, 10, 12}' },
      { input: 'a = {20, 30}, b = {}', output: '{20, 30}' },
    ],
    constraints: ['Either array can be empty.', 'O(na + nb) time: do not sort the result afterwards.'],
    hints: ['Keep one index per input and always copy the smaller current element.', 'When one input runs out, copy the rest of the other.'],
  },
  'dsa/starters/start-here/ex6-dedupe.c': {
    slug: 'remove-duplicates-sorted', title: 'Remove duplicates in place', difficulty: 'Easy', tags: ['two pointers', 'in place'],
    statement: 'Remove duplicates from the sorted array `a` **in place** and return the new length. The first `new length` elements must hold each distinct value once, in order.',
    examples: [
      { input: 'a = {1, 1, 2, 2, 2, 3, 5, 5}', output: 'returns 4, a starts with {1, 2, 3, 5}' },
      { input: 'a = {7, 7, 7}', output: 'returns 1, a starts with {7}' },
      { input: 'n = 0', output: 'returns 0' },
    ],
    constraints: ['The array is sorted; `n >= 0`.', 'O(n) time, O(1) extra memory.'],
    hints: ['A write index trails a read index: copy `a[r]` only when it differs from the last value written.', 'Handle `n == 0` before touching `a[0]`.'],
  },
  'dsa/starters/start-here/ex7-brackets.c': {
    slug: 'balanced-brackets', title: 'Balanced brackets', difficulty: 'Easy', tags: ['stack', 'strings'],
    statement: 'Return 1 if every `(`, `[` and `{` in the string is closed by the matching bracket in the right order, otherwise 0. All other characters are ignored.',
    examples: [
      { input: '"{[()]}"', output: '1' },
      { input: '"([)]"', output: '0', why: 'Counting brackets is not enough: the order matters.' },
      { input: '")("', output: '0' },
    ],
    constraints: ['The string may be empty (balanced).', 'Do not read below the bottom of your stack when a closing bracket arrives first.'],
    hints: ['Push every opening bracket; on a closing bracket pop and compare.', 'At the end the stack must be empty.'],
  },
  'dsa/starters/start-here/ex8-fibonacci.c': {
    slug: 'fibonacci-two-ways', title: 'Fibonacci, recursive and iterative', difficulty: 'Easy', tags: ['recursion', 'iteration'],
    statement: 'Write `fib_rec` (a direct recursive definition) and `fib_iter` (a loop). `fib(0) = 0`, `fib(1) = 1`, `fib(n) = fib(n-1) + fib(n-2)`. They must agree for every `n` from 0 to 25, and `fib_iter` must handle `n = 50` without overflowing a `long long`.',
    examples: [
      { input: 'fib_iter(10)', output: '55' },
      { input: 'fib_iter(50)', output: '12586269025' },
      { input: 'fib_rec(25)', output: '75025' },
    ],
    constraints: ['`0 <= n <= 90` for `fib_iter`; `0 <= n <= 25` for `fib_rec`.', 'Return type is `long long`.'],
    hints: ['The iterative version needs only two variables: the previous and the current value.', 'The recursive version makes about 240,000 calls for `n = 25`: that is why it is slow.'],
  },
  'dsa/starters/start-here/ex9-reverse-list.c': {
    slug: 'reverse-linked-list', title: 'Reverse a linked list', difficulty: 'Easy', tags: ['linked list', 'pointers'],
    statement: 'Reverse a singly linked list **in place** by changing the `next` pointers and return the new head. Do not allocate any nodes.',
    examples: [
      { input: '1 -> 2 -> 3 -> 4', output: '4 -> 3 -> 2 -> 1' },
      { input: 'single node 9', output: '9 (next is NULL)' },
      { input: 'NULL', output: 'NULL' },
    ],
    constraints: ['The list may be empty.', 'O(n) time, O(1) extra memory.'],
    hints: ['Walk the list with three pointers: previous, current and next.', 'Save `current->next` before overwriting it.'],
  },
  'dsa/starters/start-here/ex10-mode.c': {
    slug: 'most-frequent-value', title: 'Most frequent value', difficulty: 'Easy', tags: ['counting', 'arrays'],
    statement: 'Given `n > 0` values between 0 and 99, return the value that occurs most often. On a tie, return the **smallest** of the most frequent values.',
    examples: [
      { input: '{4, 7, 4, 2, 7, 7, 9}', output: '7' },
      { input: '{3, 1, 3, 1}', output: '1', why: 'Tie between 1 and 3: the smaller wins.' },
      { input: '{0, 0, 5}', output: '0' },
    ],
    constraints: ['`1 <= n`; every value is in `0..99`.', 'O(n + 100) time.'],
    hints: ['A counting array of 100 `int`s, indexed by the value.', 'Scan the counters in increasing order and replace the best only on a strictly larger count.'],
  },
  'dsa/starters/recursion/ex2-fib-memo.c': {
    slug: 'fibonacci-memoisation', title: 'Fibonacci with memoisation', difficulty: 'Medium', tags: ['recursion', 'memoisation', 'dynamic programming'],
    statement: 'The recursive `fib_memo` in the editor recomputes the same values over and over (its call counter shows how many). Keep the recursion but remember results so that `fib_memo(30)` makes at most 60 calls and `fib_memo(90)` returns in an instant.',
    examples: [
      { input: 'fib_memo(30)', output: '832040, in at most 60 calls' },
      { input: 'fib_memo(90)', output: '2880067194370816120' },
    ],
    constraints: ['`0 <= n <= 90` (the result fits in a `long long`).', 'Keep `calls++` at the top of the function: the tests read it.'],
    hints: ['A `static long long memo[91]` initialised to 0 works because `fib(n) > 0` for `n >= 1`.', 'Return the stored value before recursing; store the result before returning.'],
  },
  'dsa/starters/recursion/ex3-hanoi.c': {
    slug: 'tower-of-hanoi', title: 'Tower of Hanoi', difficulty: 'Medium', tags: ['recursion', 'recurrence'],
    statement: 'Moving `n` disks from one peg to another, never putting a larger disk on a smaller one, takes `T(n) = 2 T(n-1) + 1` moves. Implement `hanoi_moves(n)` from this recurrence, then `hanoi(n, from, to, via)`, which performs the moves by calling `move(from, to)` for each one. The tests check that every move is legal.',
    examples: [
      { input: 'hanoi_moves(5)', output: '31' },
      { input: 'hanoi_moves(63)', output: '9223372036854775807' },
      { input: 'hanoi(6, 0, 2, 1)', output: '63 legal moves; all six disks end on peg 2' },
    ],
    constraints: ['Pegs are numbered 0, 1, 2.', '`1 <= n <= 63` for `hanoi_moves`; `hanoi` is tested with 6 disks.'],
    hints: ['Move n-1 disks out of the way, move the largest one, then move the n-1 disks on top of it.', 'The closed form is `2^n - 1`; `(1LL << n) - 1` works up to 62, be careful with 63.'],
  },
  'dsa/starters/recursion/ex4-fast-power.c': {
    slug: 'fast-exponentiation', title: 'Fast exponentiation', difficulty: 'Medium', tags: ['recursion', 'divide and conquer', 'math'],
    statement: 'Compute `b^e` with **O(log e)** multiplications instead of `e`. The helper `mul(x, y)` is the multiplication the tests count. The loop in the editor is correct but makes `e` multiplications.',
    examples: [
      { input: 'power_fast(3, 13)', output: '1594323' },
      { input: 'power_fast(-2, 5)', output: '-32' },
      { input: 'power_fast(1, 1000)', output: '1, using at most 20 multiplications' },
    ],
    constraints: ['`e >= 0`; the result fits in a `long long`.', 'Do not square the base once more than needed: the intermediate value can overflow.'],
    hints: ['`b^e = (b^(e/2))^2`, times `b` when `e` is odd.', 'Iterative form: square the base each step and multiply into the result when the low bit of `e` is 1.'],
  },
  'dsa/starters/lists/ex2-cycle.c': {
    slug: 'linked-list-cycle', title: 'Detect a cycle in a linked list', difficulty: 'Medium', tags: ['linked list', 'two pointers'],
    statement: 'Does a singly linked list contain a cycle? Use **O(1) extra memory**. The list may be empty, a single node, or a node that points to itself.',
    examples: [
      { input: '0 -> 1 -> 2 -> 3 -> 4 -> 5 -> NULL', output: '0' },
      { input: '0 -> ... -> 5 -> back to 2', output: '1' },
      { input: 'a node pointing to itself', output: '1' },
    ],
    constraints: ['`head` may be NULL.', 'Do not modify the list and do not use a hash set.'],
    hints: ['Two pointers: one moves one step, the other two steps.', 'If they ever meet there is a cycle; if the fast one reaches NULL there is none.'],
  },
  'dsa/starters/lists/ex3-merge-lists.c': {
    slug: 'merge-sorted-lists', title: 'Merge two sorted linked lists', difficulty: 'Medium', tags: ['linked list', 'stable merge'],
    statement: 'Merge two sorted lists by relinking their nodes (no allocation) and return the head. The merge must be **stable**: for equal keys the node from `a` comes before the node from `b`.',
    examples: [
      { input: 'a = 1 5 9, b = 2 5 8', output: '1a 2b 5a 5b 8b 9a', why: 'The 5 from a precedes the 5 from b.' },
      { input: 'a = NULL, b = 4', output: '4' },
    ],
    constraints: ['Either list may be empty.', 'O(n + m) time, O(1) extra memory; every node in the result comes from the inputs.'],
    hints: ['A dummy head node removes the special case for the first element.', 'Take from `a` when `a->key <= b->key`, not `<`, to keep it stable.', 'Append the non-empty remainder at the end.'],
  },
  'dsa/starters/lists/ex4-queue-two-stacks.c': {
    slug: 'queue-with-two-stacks', title: 'Queue from two stacks', difficulty: 'Medium', tags: ['queue', 'stack', 'amortised analysis'],
    statement: 'Build a FIFO queue out of two LIFO stacks, `in` and `out`, using only `spush` and `spop`. Each operation must be **O(1) amortised**: items must not be shuffled back and forth on every call. `dequeue` returns -1 when the queue is empty.',
    examples: [
      { input: 'enqueue 10, 20, 30, 40; dequeue twice', output: '10, 20' },
      { input: 'then enqueue 50, 60; dequeue four times', output: '30, 40, 50, 60' },
    ],
    constraints: ['Each item is pushed at most twice (once per stack).', 'At most 64 items are stored at once.'],
    hints: ['Enqueue always pushes onto `in`.', 'Dequeue pops from `out`; only when `out` is empty, move **all** of `in` onto it.'],
  },
  'dsa/starters/sorting/ex1-bounds.c': {
    slug: 'lower-and-upper-bound', title: 'Lower bound and upper bound', difficulty: 'Medium', tags: ['binary search', 'duplicates'],
    statement: 'In a sorted array that may contain duplicates, `lower_bound(a, n, x)` is the first index `i` with `a[i] >= x` and `upper_bound(a, n, x)` is the first index with `a[i] > x`. Both return `n` if there is no such index. The count of `x` is their difference.',
    examples: [
      { input: 'a = {1, 3, 3, 3, 5, 8, 8, 13}, x = 3', output: 'lower 1, upper 4' },
      { input: 'x = 4', output: 'lower 4, upper 4 (absent)' },
      { input: 'x = 20', output: 'lower 8, upper 8', why: 'Both return n when every element is smaller.' },
    ],
    constraints: ['`0 <= n`; `a` is sorted ascending.', 'O(log n) time.'],
    hints: ['Search the half-open range `[lo, hi)`; the answer is where the predicate flips from false to true.', 'The two functions differ only in `<` versus `<=` in one comparison.'],
  },
  'dsa/starters/sorting/ex3-counting-sort.c': {
    slug: 'stable-counting-sort', title: 'Stable counting sort', difficulty: 'Medium', tags: ['counting sort', 'stability'],
    statement: 'Sort `in[0..n)` by `age` (0 to 10) into `out[0..n)` without comparing elements. People with equal ages must keep their input order (the sort is **stable**).',
    examples: [
      { input: '7a 3b 10c 3d 0e 7f 7g 1h 10i 3j', output: '0e 1h 3b 3d 3j 7a 7f 7g 10c 10i' },
    ],
    constraints: ['`0 <= age <= 10`.', 'O(n + k) time with k = 11.'],
    hints: ['Count each age, then turn the counts into starting positions (a running sum).', 'Walk the input from the first element to the last, placing each person at the next free position of its age.'],
  },
  'dsa/starters/sorting/ex4-inversions.c': {
    slug: 'count-inversions', title: 'Count inversions', difficulty: 'Hard', tags: ['merge sort', 'divide and conquer'],
    statement: 'An inversion is a pair `i < j` with `a[i] > a[j]`. Count the inversions of an array in **O(n log n)** by extending merge sort. `tmp` is scratch space for `n` ints and you may reorder `a`.',
    examples: [
      { input: '{8, 4, 2, 1, 7, 3, 9, 5}', output: '13' },
      { input: '{2, 2, 1}', output: '2', why: 'Equal keys are not inversions: (2, 1) twice.' },
      { input: '2000 values in descending order', output: '1999000' },
    ],
    constraints: ['`n <= 2000` in the tests; the count needs a `long long`.', 'A quadratic double loop is too slow for large inputs.'],
    hints: ['While merging, every time you take an element from the right half, all elements still left in the left half are inversions with it.', 'Take from the left when `left <= right` so equal keys do not count.'],
  },
  'dsa/starters/hashing/ex2-tombstone.c': {
    slug: 'hash-table-deletion', title: 'Deleting from a probing table', difficulty: 'Medium', tags: ['hash table', 'open addressing'],
    statement: 'In a linear-probing table, setting a deleted slot to `EMPTY` breaks the probe chain of keys stored after it: `find` stops at the hole and misses them. Fix `remove_key` so later searches still work and the freed slot can be reused by `insert`.',
    examples: [
      { input: 'insert 8, 16, 24 (all hash to slot 0); remove_key(16)', output: 'find(24) == 2, find(16) == -1' },
      { input: 'then insert(32)', output: 'find(32) == 1 (the freed slot is reused)' },
    ],
    constraints: ['Removing a key that is not present changes nothing.', 'The table has 8 slots; `EMPTY` and `TOMBSTONE` are the two special values.'],
    hints: ['Mark the slot `TOMBSTONE` instead of `EMPTY`.', '`find` keeps probing past a tombstone, and `insert` may reuse one.'],
  },
  'dsa/starters/trees/ex1-valid-bst.c': {
    slug: 'validate-bst', title: 'Validate a binary search tree', difficulty: 'Medium', tags: ['binary search tree', 'recursion'],
    statement: 'Return 1 if the binary tree is a valid binary search tree: every key in a node’s **left subtree** is smaller than the node’s key and every key in its **right subtree** is larger. Checking only the direct children is not enough.',
    examples: [
      { input: '50 (30 (20, 40), 70 (60, 80))', output: '1' },
      { input: '50 (30 (20, 55), 70)', output: '0', why: '55 is the right child of 30, but it sits in the left subtree of 50.' },
      { input: 'empty tree', output: '1' },
    ],
    constraints: ['Keys are distinct; the tree may be empty.', 'O(n) time.'],
    hints: ['Pass down the range `(low, high)` of keys allowed at each node.', 'Going left tightens `high` to the node’s key; going right tightens `low`.', 'An in-order walk must be strictly increasing: another valid approach.'],
  },
  'dsa/starters/trees/ex2-kth-balanced.c': {
    slug: 'kth-smallest-and-balance', title: 'k-th smallest and balance check', difficulty: 'Medium', tags: ['binary search tree', 'in-order', 'height'],
    statement: 'Implement two functions on a binary search tree:\n\n1. `kth_smallest(root, k, &out)` stores the k-th smallest key (`k = 1` is the smallest) and returns 1, or returns 0 if the tree has fewer than `k` keys. Do not copy the tree into an array.\n2. `is_balanced(root)` returns 1 if at **every** node the heights of the two subtrees differ by at most 1.',
    examples: [
      { input: 'kth_smallest(good tree of 7 keys, 3)', output: '1, key 40' },
      { input: 'kth_smallest(good tree, 8)', output: '0' },
      { input: 'is_balanced(chain 1 - 2 - 3)', output: '0' },
    ],
    constraints: ['Empty tree is balanced.', 'Equal heights at the root are not enough: check every node.'],
    hints: ['In-order traversal visits keys in ascending order: count as you go and stop at k.', 'Compute height and balance in one pass; return a sentinel (-1) when a subtree is unbalanced.'],
  },
  'dsa/starters/trees/ex3-top-k.c': {
    slug: 'top-k-largest', title: 'Top k largest values', difficulty: 'Medium', tags: ['heap', 'priority queue'],
    statement: 'Write the `k` largest values of `data[0..n)` into `out[0..k)`, smallest first, using **O(k) extra memory**: keep a min-heap of size `k` instead of sorting everything. `k <= n`.',
    examples: [
      { input: 'data = {42, 7, 99, 15, 3, 88, 61, 27, 54, 70, 12, 95}, k = 4', output: '{70, 88, 95, 99}' },
      { input: '{5, 9, 9, 1, 9}, k = 3', output: '{9, 9, 9}', why: 'Duplicates count separately.' },
      { input: '{1, 2, 3, 4, 5, 6}, k = 6', output: '{1, 2, 3, 4, 5, 6}' },
    ],
    constraints: ['`1 <= k <= n <= 12` in the tests.', 'O(n log k) time.'],
    hints: ['The root of a min-heap is the smallest of the k kept values: replace it when a bigger value arrives.', 'At the end, repeatedly remove the root to get ascending order.'],
  },
  'dsa/starters/trees/ex4-trie.c': {
    slug: 'prefix-trie', title: 'Prefix trie', difficulty: 'Medium', tags: ['trie', 'strings'],
    statement: 'Implement `trie_insert`, `count_prefix` (how many stored words start with a prefix) and `contains` (is this exact word stored) on a trie of lowercase words. `count_prefix` must take time proportional to the **prefix length**, which is why each node keeps `words_below`.',
    examples: [
      { input: 'words car card care cart cat dog do; count_prefix("car")', output: '4' },
      { input: 'count_prefix("")', output: '7', why: 'The empty prefix matches every word.' },
      { input: 'contains("ca")', output: '0', why: 'A prefix of a stored word is not itself a word.' },
    ],
    constraints: ['Words use only `a`–`z`.', 'Create child nodes with the given `new_node()`.'],
    hints: ['While inserting, increment `words_below` on every node along the path, including the last one.', 'Mark the last node with `is_word = 1`.', '`count_prefix` walks the prefix and returns `words_below` of the node it ends on (0 if the path breaks).'],
  },
  'dsa/starters/graphs/ex1-islands.c': {
    slug: 'number-of-islands', title: 'Number of islands', difficulty: 'Medium', tags: ['graph', 'DFS', 'BFS', 'grid'],
    statement: 'A grid contains `1` (land) and `0` (water). Count the islands, groups of land cells connected **up, down, left or right** (not diagonally), and the size of the largest. You may modify the grid, for example to mark visited cells.',
    examples: [
      { input: '11000111\n11000101\n00100000\n00011100\n10011100\n10000001', output: '6 islands, largest 6' },
      { input: '10\n01', output: '2 islands', why: 'Diagonal cells do not connect.' },
      { input: '000', output: '0 islands' },
    ],
    constraints: ['The grid has `rows` strings of `cols` characters, at most `MAXC` columns.', 'Avoid unbounded recursion on large islands if you can (an explicit stack works).'],
    hints: ['Scan every cell; when you find unvisited land start a flood fill and count its cells.', 'Mark visited land as `0` (or any other character) so it is not counted twice.'],
  },
  'dsa/starters/graphs/ex2-maze.c': {
    slug: 'shortest-path-in-maze', title: 'Shortest path in a maze', difficulty: 'Medium', tags: ['graph', 'BFS', 'grid'],
    statement: '`S` is the start, `E` the exit, `#` a wall and `.` a free cell. Moves go up, down, left or right at cost 1. Return the number of steps of the shortest path from `S` to `E`, or -1 if `E` cannot be reached.',
    examples: [
      { input: 'S.#......\n.##.####.\n....#....\n.####.##.\n......#.E', output: '16' },
      { input: 'SE', output: '1' },
      { input: 'S#E', output: '-1' },
    ],
    constraints: ['Exactly one `S` and one `E`.', 'Grids are small (at most a few hundred cells).'],
    hints: ['Breadth-first search explores cells in order of distance, so the first time you reach `E` is the shortest path.', 'Keep a distance (or visited) array and a queue of cells.'],
  },
  'dsa/starters/graphs/ex3-topo.c': {
    slug: 'task-ordering', title: 'Order tasks with prerequisites', difficulty: 'Medium', tags: ['graph', 'topological sort', 'cycle'],
    statement: '`edge[a][b] == 1` means task `a` must happen before task `b`. Fill `order[0..N)` with a valid order and return 1, or return 0 if the prerequisites contain a **cycle** and no order exists. There are `N = 6` tasks.',
    examples: [
      { input: 'wake -> shower -> dress -> leave; wake -> coffee -> breakfast -> leave', output: '1, e.g. wake coffee breakfast shower dress leave' },
      { input: 'the same plus leave -> wake', output: '0 (cycle)' },
    ],
    constraints: ['Any order that respects every edge is accepted.', 'Do not modify the caller’s `edge` matrix.'],
    hints: ['Kahn’s algorithm: repeatedly take a task with no remaining prerequisites.', 'If fewer than N tasks were taken, there is a cycle.'],
  },
  'dsa/starters/graphs/ex4-dijkstra.c': {
    slug: 'dijkstra-shortest-distances', title: 'Dijkstra’s shortest distances', difficulty: 'Hard', tags: ['graph', 'Dijkstra', 'shortest path'],
    statement: '`w[u][v]` is the weight of the edge `u -> v` (non-negative) or `INF` if there is none. Fill `dist[v]` with the length of the shortest path from `src` to `v`, or `INF` if `v` is unreachable. The code in the editor only looks at direct edges.',
    examples: [
      { input: 'edges 0->1 (4), 0->2 (1), 2->1 (2), 1->3 (1), 2->3 (5), 3->4 (3); src = 0', output: 'dist = {0, 3, 1, 4, 7}' },
      { input: 'same graph; src = 3', output: 'dist[4] = 3, dist[0] = INF' },
    ],
    constraints: ['`N` vertices, weights `>= 0`.', 'Careful: `INF + weight` must not overflow or look like a real distance.'],
    hints: ['Repeat N times: pick the unvisited vertex with the smallest distance and relax its outgoing edges.', 'Skip edges equal to `INF` and stop early if the best remaining vertex is `INF`.'],
  },
  'dsa/starters/strings/ex2-palindrome.c': {
    slug: 'longest-palindromic-substring', title: 'Longest palindromic substring', difficulty: 'Medium', tags: ['strings', 'expand around centre'],
    statement: 'Find the longest palindromic substring of `s`: store where it starts and its length. If several have the maximum length, any of them is accepted. Expand around each of the `2n - 1` centres.',
    examples: [
      { input: '"babad"', output: 'length 3 ("bab" or "aba")' },
      { input: '"forgeeksskeegfor"', output: 'length 10 ("geeksskeeg")' },
      { input: '""', output: 'length 0' },
    ],
    constraints: ['`s` may be empty; `O(n^2)` time is fine.', 'Both odd-length (one centre character) and even-length (centre between two) palindromes must be found.'],
    hints: ['For each centre expand while the two characters are equal.', 'Try centre `(i, i)` for odd and `(i, i + 1)` for even lengths.'],
  },
  'dsa/starters/strings/ex3-rabin-karp.c': {
    slug: 'rabin-karp-search', title: 'Rabin-Karp pattern search', difficulty: 'Hard', tags: ['strings', 'rolling hash'],
    statement: 'Find **every** occurrence of `pat` in `text` (overlaps included) with a rolling hash. Store the start index of each in `pos[]` (at most `max`) and return how many there are. Update the window hash in O(1) per step and confirm each candidate character by character, because different strings can share a hash.',
    examples: [
      { input: 'text = "the cat sat on the mat with the other cat", pat = "the"', output: 'positions {0, 15, 28, 33}' },
      { input: '"aaaa", "aa"', output: '{0, 1, 2}', why: 'Overlapping matches count.' },
      { input: '"abc", "abcd"', output: '0', why: 'The pattern is longer than the text.' },
    ],
    constraints: ['Pattern longer than text returns 0.', 'The last window of the text must be checked too.'],
    hints: ['hash = (hash - first * base^(m-1)) * base + next, all modulo a prime.', 'Keep intermediate values non-negative: add the modulus before subtracting.', 'Compare with `memcmp` when the hashes are equal.'],
  },
  'dsa/starters/design/ex1-coins.c': {
    slug: 'coin-change', title: 'Coin change', difficulty: 'Medium', tags: ['dynamic programming'],
    statement: 'Given coin values (unlimited supply of each) and a target amount, write two functions:\n\n1. `min_coins` returns the **fewest** coins that add up to the target, or -1 if it is impossible.\n2. `count_ways` returns the number of different **multisets** of coins that add up to the target (order does not matter).',
    examples: [
      { input: 'coins {4, 3, 1}, target 6', output: 'min_coins = 2', why: '3 + 3. The greedy choice 4 + 1 + 1 needs three coins.' },
      { input: 'coins {1, 5, 10}, target 10', output: 'count_ways = 4' },
      { input: 'coins {5, 10}, target 3', output: 'min_coins = -1' },
    ],
    constraints: ['Target 0 needs 0 coins and has exactly one way (the empty set).', '`count_ways({25,10,5,1}, 100)` is 242: use `long long`.'],
    hints: ['`best[t] = 1 + min(best[t - coin])` over the coins that fit.', 'To count multisets rather than sequences, loop over the coins in the **outer** loop and the amounts in the inner loop.'],
  },
  'dsa/starters/design/ex2-edit-distance.c': {
    slug: 'edit-distance', title: 'Edit distance', difficulty: 'Hard', tags: ['dynamic programming', 'strings'],
    statement: 'The edit distance between two strings is the minimum number of single-character **insertions, deletions and substitutions** that turn one into the other. Implement `edit_distance(a, b)`.',
    examples: [
      { input: '"kitten", "sitting"', output: '3' },
      { input: '"intention", "execution"', output: '5' },
      { input: '"", "abc"', output: '3' },
    ],
    constraints: ['Either string may be empty.', 'O(|a| * |b|) time; a table of `(|a| + 1) x (|b| + 1)` entries (or two rows) is enough.'],
    hints: ['`d[i][j]` is the distance between the first i characters of a and the first j of b.', 'If `a[i-1] == b[j-1]` take `d[i-1][j-1]`; otherwise 1 + the minimum of the three neighbours.', 'The first row and column are `0..|b|` and `0..|a|`.'],
  },
  'dsa/starters/design/ex3-intervals.c': {
    slug: 'maximum-non-overlapping-jobs', title: 'Maximum number of jobs', difficulty: 'Medium', tags: ['greedy', 'intervals'],
    statement: 'Each job has a `start` and an `end`. Two jobs can run one after another if one **starts when the other ends** (touching is allowed). Return the maximum number of jobs that can be scheduled; you may reorder the array.',
    examples: [
      { input: 'A(0,9) B(1,3) C(2,5) D(4,6) E(6,8) F(5,7) G(8,10)', output: '4', why: 'For example B, D, E, G.' },
      { input: '(2,3) (0,1) (1,2)', output: '3', why: 'Back-to-back jobs all fit.' },
      { input: 'no jobs', output: '0' },
    ],
    constraints: ['`n >= 0`.', 'The greedy choice needs the jobs sorted by end time.'],
    hints: ['Sort by end time (use `qsort` with a safe comparator).', 'Take a job when its start is at least the end of the last chosen job.'],
  },
  'dsa/starters/design/ex4-queens.c': {
    slug: 'n-queens-count', title: 'Count N-Queens placements', difficulty: 'Hard', tags: ['backtracking', 'recursion'],
    statement: 'Count the ways to place `n` queens on an `n x n` board so that no two attack each other (same row, column or diagonal).',
    examples: [
      { input: 'n = 4', output: '2' },
      { input: 'n = 8', output: '92' },
      { input: 'n = 2', output: '0' },
    ],
    constraints: ['`1 <= n <= 10` in the tests (724 solutions for n = 10).', 'Prune as soon as a placement is illegal.'],
    hints: ['Place one queen per row; track used columns and both diagonals (`r + c` and `r - c + n`).', 'Undo the placement after the recursive call (backtrack).'],
  },
  'dsa/starters/systems/ex1-page-faults.c': {
    slug: 'page-replacement-policies', title: 'Page replacement: FIFO, LRU, OPT', difficulty: 'Hard', tags: ['caching', 'simulation', 'operating systems'],
    statement: 'Simulate a cache of `frames` pages (at most 16, initially empty) on a sequence of page references (page numbers 0 to 99) and count the **misses** for three policies: FIFO (evict the oldest loaded page), LRU (evict the least recently used) and OPT (evict the page used furthest in the future, or never).',
    examples: [
      { input: 'refs = 1 2 3 4 1 2 5 1 2 3 4 5, frames = 3', output: 'FIFO 9, LRU 10, OPT 7' },
      { input: 'same refs, frames = 4', output: 'FIFO 10, LRU 8, OPT 6', why: 'FIFO gets worse with more frames (Belady’s anomaly).' },
    ],
    constraints: ['Every first access to a page is a miss.', '`n` up to a few dozen references.'],
    hints: ['Keep the frames in an array and a timestamp (load time for FIFO, last use for LRU).', 'For OPT, scan forward from the current position to find each cached page’s next use.'],
  },
  'dsa/starters/systems/ex2-arena.c': {
    slug: 'arena-allocator', title: 'Arena allocator', difficulty: 'Medium', tags: ['allocator', 'alignment'],
    statement: 'An `Arena` has a block of memory `base`, its size `cap` and the bytes handed out so far, `used`. Implement `arena_alloc(a, size, align)`, which returns `size` bytes aligned to `align` (a power of two) or `NULL` if they do not fit, and `arena_reset`, which releases everything at once.',
    examples: [
      { input: '100 allocations of a 16-byte node', output: 'used = 100 * sizeof(Node) (no padding between equal-sized, aligned objects)' },
      { input: 'arena_alloc(a, 1, 1); arena_alloc(a, 4, 4)', output: 'the int starts 4 bytes after the char' },
      { input: 'arena_alloc(a, 5000, 1) on a 4096-byte arena', output: 'NULL' },
    ],
    constraints: ['`align` is a power of two; align the **address**, not just the offset.', 'Never read or write outside `cap` bytes; guard against overflow in `used + size`.'],
    hints: ['`aligned = (addr + align - 1) & ~(align - 1)` on the address `base + used`.', 'Convert via `uintptr_t`, then compute the new `used` as an offset from `base`.', 'Reset is just `used = 0`.'],
  },
};
