// What the compiler emits for each memory order on x86-64 (compile with: gcc -O2 -S -o - orders.c)
#include <stdatomic.h>
atomic_int a, b;
void store_relaxed(int v) { atomic_store_explicit(&a, v, memory_order_relaxed); }
void store_release(int v) { atomic_store_explicit(&a, v, memory_order_release); }
void store_seq_cst(int v) { atomic_store_explicit(&a, v, memory_order_seq_cst); }
int  load_acquire(void)   { return atomic_load_explicit(&a, memory_order_acquire); }
int  load_seq_cst(void)   { return atomic_load_explicit(&a, memory_order_seq_cst); }
int  fetch_add(void)      { return atomic_fetch_add_explicit(&a, 1, memory_order_relaxed); }
int  cas(int e, int d)    { return atomic_compare_exchange_strong(&a, &e, d); }
