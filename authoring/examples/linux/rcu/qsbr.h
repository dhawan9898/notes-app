// A minimal quiescent-state-based RCU (QSBR) for user space, in the style of liburcu's "qsbr" flavour.
// Readers pay NOTHING per access; they only announce "I hold no RCU-protected pointer" at points of their choosing.
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>

#define MAX_READERS 16
static atomic_ulong global_epoch = 1;
static atomic_ulong reader_epoch[MAX_READERS];           // 0 = offline (not registered / not reading)
static atomic_int   nreaders;

static int rcu_register_thread(void) {                    // returns this reader's slot
    int id = atomic_fetch_add(&nreaders, 1);
    atomic_store(&reader_epoch[id], atomic_load(&global_epoch));
    return id;
}
static void rcu_quiescent_state(int id) {                 // "I am between read-side critical sections"
    atomic_store_explicit(&reader_epoch[id], atomic_load_explicit(&global_epoch, memory_order_acquire), memory_order_seq_cst);
}
static void rcu_thread_offline(int id) { atomic_store(&reader_epoch[id], 0); }
__attribute__((unused)) static void rcu_thread_online(int id)  { atomic_store(&reader_epoch[id], atomic_load(&global_epoch)); }

// Wait for a grace period: every online reader has passed through a quiescent state since we started.
// After this returns, no reader can still hold a pointer that was reachable before the call.
static void synchronize_rcu(void) {
    unsigned long target = atomic_fetch_add(&global_epoch, 1) + 1;
    int n = atomic_load(&nreaders);
    for (int i = 0; i < n; i++)
        for (;;) {
            unsigned long e = atomic_load(&reader_epoch[i]);
            if (e == 0 || e >= target) break;              // offline, or has announced a quiescent state after the update
            sched_yield();
        }
}

// publish / consume: the release store makes the object's contents visible before the pointer is; the acquire load pairs with it
#define rcu_assign_pointer(p, v) atomic_store_explicit(&(p), (v), memory_order_release)
#define rcu_dereference(p)       atomic_load_explicit(&(p), memory_order_acquire)
