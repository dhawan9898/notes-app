// A single-producer single-consumer ring buffer between an "interrupt handler" (a SIGALRM handler, fired by a timer)
// and the main loop. The classic embedded pattern: the ISR must never block; the main loop drains the queue.
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#define SIZE 256                                   // power of two: index wrap is a mask
static unsigned buf[SIZE];
static atomic_uint head, tail;                     // head: written only by the ISR; tail: written only by the main loop
static atomic_uint dropped;
static unsigned next_sample;                       // ISR-private counter

static void isr(int sig) {                         // async-signal-safe: only lock-free atomics and plain stores
    (void)sig;
    unsigned h = atomic_load_explicit(&head, memory_order_relaxed);
    unsigned t = atomic_load_explicit(&tail, memory_order_acquire);
    if (h - t == SIZE) { atomic_fetch_add_explicit(&dropped, 1, memory_order_relaxed); next_sample++; return; }   // full: drop, never block
    buf[h & (SIZE - 1)] = next_sample++;
    atomic_store_explicit(&head, h + 1, memory_order_release);        // publish the element
}

int main(void) {
    struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_handler = isr; sigaction(SIGALRM, &sa, NULL);
    struct itimerval it = { {0, 200}, {0, 200} };  // every 200 microseconds
    setitimer(ITIMER_REAL, &it, NULL);
    unsigned expect = 0, got = 0, out_of_order = 0;
    while (got < 5000) {
        unsigned t = atomic_load_explicit(&tail, memory_order_relaxed);
        unsigned h = atomic_load_explicit(&head, memory_order_acquire);
        while (t != h) {                           // drain everything published so far
            unsigned v = buf[t & (SIZE - 1)]; t++;
            if (v < expect) out_of_order++;        // values must be strictly increasing; gaps are only from drops
            expect = v + 1; got++;
        }
        atomic_store_explicit(&tail, t, memory_order_release);
    }
    struct itimerval off = {{0, 0}, {0, 0}}; setitimer(ITIMER_REAL, &off, NULL);
    printf("consumed %u samples in order (out of order: %u), samples dropped because the ring was full: %u\n", got, out_of_order, atomic_load(&dropped));
    return 0;
}
