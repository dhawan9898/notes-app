---
title: "Kernel Memory Allocators and RCU: Buddy, SLUB, vmalloc and Read-Copy-Update"
track: linux
module: Memory
moduleOrder: 4
order: 3
level: expert
summary: "How the kernel allocates its own memory (page allocator and buddy system, SLUB caches and kmalloc, vmalloc, GFP flags, memcg accounting) and how it avoids read-side locks with RCU. A user-space QSBR implementation shows grace periods, catches the premature-free bug, and measures a ~100x read-side advantage over a mutex."
prerequisites:
  - Virtual Memory, Paging and the Page Cache
  - Threads and Synchronization
  - "Threads, Atomics and the C11 Memory Model"
sources:
  - label: Linux kernel documentation — Memory Management (mm) concepts overview
    url: https://docs.kernel.org/admin-guide/mm/concepts.html
  - label: Linux kernel documentation — Memory Allocation Guide (GFP flags, kmalloc, vmalloc)
    url: https://docs.kernel.org/core-api/memory-allocation.html
  - label: Linux kernel documentation — Short users guide for SLUB
    url: https://docs.kernel.org/mm/slub.html
  - label: Linux kernel documentation — What is RCU? Fundamentally
    url: https://docs.kernel.org/RCU/whatisRCU.html
  - label: Linux kernel documentation — RCU Concepts, Requirements, and Design
    url: https://docs.kernel.org/RCU/rcu.html
  - label: McKenney — Is Parallel Programming Hard, And, If So, What Can You Do About It? (RCU chapter)
    url: https://mirrors.edge.kernel.org/pub/linux/kernel/people/paulmck/perfbook/perfbook.html
  - label: proc(5) — /proc/buddyinfo, /proc/meminfo, /proc/slabinfo
    url: https://man7.org/linux/man-pages/man5/proc.5.html
  - label: liburcu — Userspace RCU (flavours: QSBR, memb, bullet-proof)
    url: https://liburcu.org/
---

User space asks the kernel for memory through `mmap` and `brk` and gets virtual memory that is filled lazily. The kernel itself has a different problem: it needs memory for page tables, socket buffers, inodes, dentries, task structs and thousands of other objects, often **while handling an interrupt** or while the system is already short of memory, and often from many CPUs at once. Its answer is a layered set of allocators and, for read-mostly shared data, a synchronization scheme called RCU that makes reads free.

## Layers of kernel allocation

```mermaid
graph TD
  A["kmalloc / kmem_cache_alloc / kvmalloc"] --> B["SLUB: per-CPU caches of fixed-size objects"]
  B --> C["page allocator: buddy system, per zone and migrate type"]
  D["vmalloc"] --> C
  C --> E["physical page frames (struct page / folios)"]
```

### The page allocator and the buddy system

Physical memory is divided into **zones** (DMA, DMA32, Normal, and others) because some devices can address only low memory. In each zone, free pages are kept in lists by **order**: order *n* is a block of 2^n contiguous pages (order 0 = 4 KiB, order 10 = 4 MiB). To allocate order *k*: take a block from the order-*k* list, or split a larger block into two **buddies** and use one. To free: if the block's buddy (its neighbour of the same size, found by flipping one address bit) is also free, merge them into the next order, repeating upward. That makes coalescing cheap and keeps large contiguous blocks available when possible.

`/proc/buddyinfo` shows, for each zone, how many free blocks exist at each order (the columns are orders 0 to 10). A healthy machine has non-zero counts in the high orders; a machine whose counts are concentrated in orders 0-2 is **fragmented**, and an allocation of a large contiguous block (a huge page, a driver's DMA buffer, a high-order skb) can fail or stall in compaction even though plenty of memory is free in total. `/proc/pagetypeinfo` further splits free blocks by **migrate type** (unmovable, movable, reclaimable), which is how the kernel groups pages so that movable ones can be migrated away to rebuild large blocks (compaction).

Other mechanisms in this layer:

- **Watermarks** (min, low, high per zone): when free memory drops below *low*, the background reclaimer `kswapd` wakes; below *min*, allocators must reclaim directly (direct reclaim stalls the allocating process) or fail.
- **Per-CPU page lists** make order-0 allocations lock-free in the common case.
- **GFP flags** say what the allocator is allowed to do (see below).
- **CMA** (contiguous memory allocator) reserves a region that serves movable pages normally but can be evacuated for large contiguous device allocations.
- **Memory cgroups** charge pages (and many slab objects) to the allocating container, so `memory.max` limits kernel memory used on a cgroup's behalf too.

### GFP flags: what an allocation may do

| Flag | Meaning |
|---|---|
| `GFP_KERNEL` | normal; may sleep, reclaim memory, start I/O. **Only valid in process context.** |
| `GFP_ATOMIC` | may not sleep: for interrupt handlers and under spinlocks. Draws on emergency reserves, fails more readily |
| `GFP_NOWAIT` | like atomic without dipping into reserves; the caller must handle failure |
| `GFP_NOIO` / `GFP_NOFS` | do not start I/O / filesystem calls during reclaim; prevents deadlock when the allocation is itself inside the I/O or filesystem path |
| `__GFP_ZERO` | return zeroed memory (`kzalloc`) |
| `__GFP_ACCOUNT` | charge the memory cgroup |
| `__GFP_NOWARN`, `__GFP_RETRY_MAYFAIL`, `__GFP_NORETRY` | tune retry and failure reporting |

The common kernel bug here is allocating with `GFP_KERNEL` while holding a spinlock or in interrupt context ("sleeping function called from invalid context"); the scheduler-debugging options catch it.

### SLUB, kmalloc and caches

Most kernel objects are small and of a few fixed sizes. Asking the page allocator for a 4 KiB page for a 200-byte inode would waste memory and be slow. The **slab allocator** (default implementation **SLUB**) manages **caches**, one per object type (`task_struct`, `dentry`, `inode_cache`, `kmalloc-64`, ...). Each cache takes pages from the page allocator and carves them into equal objects. Allocation pops an object from the current CPU's freelist without a lock; free pushes it back. Benefits: no per-object header overhead, hot objects stay in the same CPU's cache, and a constructor-initialised object can stay initialised across reuse.

`kmalloc(size, gfp)` is a convenience that picks one of the generic `kmalloc-*` caches rounded up to a power-of-two-ish size (8, 16, 32, 64, 96, 128, 192, ... bytes), so a 65-byte request uses the 96-byte cache and wastes 31 bytes (internal fragmentation). For frequently allocated object types create a dedicated cache with `kmem_cache_create` and allocate with `kmem_cache_alloc`. Requests above a couple of pages go straight to the page allocator.

Observing slabs:

```bash
grep -E 'Slab|SReclaimable|SUnreclaim' /proc/meminfo   # total slab memory, reclaimable (dentries, inodes) vs not
sudo slabtop -o                                        # per-cache objects, size, active (root only)
sudo cat /proc/slabinfo                                # raw per-cache statistics (root only)
ls /sys/kernel/slab                                    # tunables and counters per cache (root-readable files)
```

`SReclaimable` is mostly dentry and inode caches: it can be dropped under pressure (`echo 2 | sudo tee /proc/sys/vm/drop_caches` is a diagnostic tool, not a tuning knob). A large and growing `SUnreclaim` often points at a kernel or driver leak; boot with `slab_debug` or enable KASAN/kmemleak in a debug kernel to find it.

### kmalloc vs vmalloc vs kvmalloc

| API | Memory | When |
|---|---|---|
| `kmalloc` | physically contiguous, from slab or page allocator | small objects, DMA buffers; fails if no contiguous block exists |
| `vmalloc` | virtually contiguous, physically scattered (page tables built per allocation) | large buffers that need no physical contiguity (module code, big tables); slower, TLB pressure |
| `kvmalloc` | tries `kmalloc`, falls back to `vmalloc` | large allocations of unknown size: the right default for big arrays |
| `alloc_pages(order)` | raw pages | when you need whole pages or specific order |

## RCU: reads that cost nothing

Many kernel structures are read constantly and changed rarely: routing tables, the dentry hash, lists of network devices, module lists. A reader-writer lock makes every reader write to a shared cache line (the lock counter), so reads from many CPUs bounce the line between them and scale poorly. **Read-copy-update (RCU)** removes the lock from the read side entirely.

The recipe for updating a structure:

1. **Read side**: `rcu_read_lock()`; `p = rcu_dereference(ptr)`; use `p`; `rcu_read_unlock()`. In non-preemptible kernels these calls compile to almost nothing: they only mark a region in which the thread must not block.
2. **Update**: allocate a new copy, change it, and publish it with `rcu_assign_pointer(ptr, new)` (a release store: the contents are visible before the pointer).
3. **Wait** for a **grace period**: every CPU has passed through a *quiescent state* (context switch, idle, or user mode), so no reader that started before the update can still be running. `synchronize_rcu()` blocks for this; `call_rcu(&old->head, free_fn)` queues a callback instead and returns immediately.
4. **Reclaim** the old copy.

Readers may see either the old or the new version, never a half-updated one, and they never block the writer from publishing. The cost moves to the writer, which waits for a grace period (typically milliseconds) and must allocate and copy. RCU therefore fits **read-mostly** data. Kernel variants: **RCU-bh/sched** merged into the main flavour, **SRCU** (sleepable readers, per-domain), **Tasks RCU** (for tracing trampolines), and list helpers such as `list_for_each_entry_rcu`, `hlist_add_head_rcu`.

Rules readers must follow: no sleeping inside a normal read-side section (except SRCU); fetch the pointer with `rcu_dereference` once and use the local copy; do not keep the pointer after `rcu_read_unlock`. Writers must serialise among themselves with a normal lock (RCU coordinates readers and writers, not writers with each other).

### A user-space QSBR RCU you can run

Kernel RCU detects quiescent states from the scheduler. In user space the application has to announce them, which gives the **quiescent-state-based reclamation (QSBR)** flavour (as in `liburcu`'s `qsbr`). Readers pay nothing per access; each reader thread calls `rcu_quiescent_state()` periodically ("I hold no protected pointer right now"), and `synchronize_rcu()` waits until every registered reader has done so after the update.

{{include linux/rcu/qsbr.h}}

The first experiment: four readers repeatedly follow a shared pointer to a record whose invariant is `a + b == 100`, and a writer keeps replacing the record, poisoning and freeing the old one. With `synchronize_rcu()` before the free, no reader ever sees a poisoned record. Without it, the writer frees memory that readers still hold, and readers observe the poison (a stand-in for a crash or corrupted data; in real code this is a use-after-free).

{{include linux/rcu/demo.c}}

{{run linux/rcu :: gcc -O2 -Wall -Wextra -pthread demo.c -o demo && ./demo}}

With the grace period the violation count is zero over about 90 million reads; without it, violations appear within a second. The second run frees memory a reader may still hold, so it is undefined behavior; it is shown only to demonstrate the failure, and its violation count varies from run to run.

The second experiment compares read-side throughput for the same workload (4 readers, one writer updating every 50 µs) under a mutex, a reader-writer lock and RCU:

{{include linux/rcu/bench.c}}

{{run linux/rcu :: gcc -O2 -Wall -Wextra -pthread bench.c -o bench && ./bench}}

RCU readers do a pointer load and two plain reads, with no atomic read-modify-write and no shared-cache-line writes; the lock-based readers all contend for the lock word. The ratio (about two orders of magnitude here) depends heavily on core count and update rate, but the shape is general: lock-based read paths do not scale with readers, RCU read paths do. The price shows on the write side: each update here must wait for all readers to reach a quiescent state.

## Putting the two halves together

RCU is why memory reclamation in the kernel is deferred: a dentry or route entry removed from a hash table cannot be freed immediately because lock-free readers may still be looking at it. It is freed by a `call_rcu` callback or with `kfree_rcu(ptr, rcu_head)`, and slab caches created with `SLAB_TYPESAFE_BY_RCU` go further: the *memory* stays valid as the same type of object even after free (readers must re-validate the object after taking a reference). Heavy `call_rcu` use can delay memory reuse, so on a system short of memory check for a backlog of RCU callbacks (the `rcutree` tracepoints) and RCU stall warnings in `dmesg`.

## Diagnostics

- **RCU stall warnings** (`rcu_sched detected stalls on CPUs/tasks`) in `dmesg`: a CPU stayed in a read-side critical section or with preemption disabled so long that grace periods could not end. Causes: a busy-looping driver, a long loop under `rcu_read_lock`, a CPU that never reschedules.
- **Allocation failures** (`page allocation failure: order:N`): high-order allocation with fragmented memory. Look at `/proc/buddyinfo`, consider `vm.min_free_kbytes`, `vm.compact_memory` (diagnostic), or change the caller to `kvmalloc`.
- **Memory growth**: `/proc/meminfo` `SUnreclaim`, `slabtop`, `/proc/vmallocinfo`; kernel debugging tools `kmemleak`, `KASAN` for leaks and use-after-free; `perf` and tracepoints `kmem:*` for allocation profiles.
- **Direct reclaim latency**: PSI (`/proc/pressure/memory`) and the `allocstall` counters in `/proc/vmstat`.

## Key takeaways

- The kernel allocates in layers: the buddy allocator hands out physical pages in power-of-two blocks (fragmentation shows in `/proc/buddyinfo`), SLUB turns pages into caches of fixed-size objects, and `vmalloc`/`kvmalloc` give virtually contiguous memory for large buffers.
- GFP flags encode what the allocator may do; `GFP_KERNEL` may sleep and must never be used in atomic contexts.
- RCU makes the read side free by deferring reclamation: publish with `rcu_assign_pointer`, wait a grace period (`synchronize_rcu`/`call_rcu`), then free. Readers see the old or the new version, never a mix.
- Freeing before the grace period ends is a use-after-free; the demonstration above shows the failure and a throughput advantage of roughly two orders of magnitude for readers over a mutex.
- Use RCU for read-mostly data and accept more costly, delayed updates.
