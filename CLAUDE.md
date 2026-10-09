# CLAUDE.md — DeepNotes project context

This file is the hand-off document for any Claude Code session working in this repository. Read it fully before changing anything. It records what the project is, every decision made so far, how to work in the repo, the current status, the traps already hit, and the detailed plan for what comes next.

---

## 1. What this project is

**DeepNotes** is a static website of senior-level, source-backed engineering notes for one author (the repo owner, who is the only user for now) on four subject areas:

1. **Networking (L2 & L3 and related technologies)**
2. **Linux**
3. **C programming**
4. **Data structures & algorithms (implemented in C)**

Why it exists: the owner could not find one place that covers these topics in depth at *senior-engineer* level; reaching that depth normally means reading RFCs, IEEE 802 standards, ISO C, POSIX and kernel docs directly. The notes distil those primary sources into one consistent, navigable site. Audience range: intermediate to veteran engineers. The owner plans to publish it eventually.

Delivery constraints decided with the owner:

- **Static site on GitHub Pages** (repo: `dhawan9898/notes-app`, so the site base path is `/notes-app`).
- **Later: port to Android.** The static build can be wrapped with Capacitor; nothing so far depends on a server.
- **No progress tracking and no accounts for now** ("stick with static pages till we get all info for notes"). Progress tracking, quizzes, accounts and sync were deliberately deferred.
- The **content is the main workload**. Depth over interactivity. Notes must be detailed, cover topics in depth, and cite primary sources.
- Look: clean, minimal, friendly; dark/light theme; search; per-note outline and prev/next navigation.

---

## 2. Technology and repository layout

**Stack:** Astro 5 (`astro@5`, `@astrojs/mdx@4`), content collections, `astro-pagefind` full-text search, Mermaid (client-side) for diagrams, Shiki for code highlighting, plain CSS with design tokens (no Tailwind). Astro 5 is pinned because the authoring machine has **Node 20** and Astro 7 needs Node 22; CI uses Node 22.

```text
.
├── CLAUDE.md                    this file
├── README.md                    run/deploy instructions, note front-matter schema, conventions, status table
├── astro.config.mjs             site/base from env SITE and BASE; mdx + pagefind integrations; Shiki dual themes
├── package.json                 scripts: dev, build (= check-notes --fix && astro build), preview, check
├── .github/workflows/deploy.yml GitHub Pages deploy on push to main (Node 22; SITE/BASE derived from the repo)
├── scripts/check-notes.mjs      validates every note's YAML front matter; --fix auto-quotes `label:` values
├── docs/                        topic maps with [x]/[ ] status per note
│   ├── networking-outline.md    (all done)
│   ├── linux-outline.md         (all done)
│   ├── c-outline.md             (all done)
│   └── dsa-outline.md           (only the hash-table note is done)
├── src/
│   ├── content.config.ts        note schema (zod): title, track, module, moduleOrder, order, level, summary, sources[]
│   ├── lib/tracks.ts            TRACKS metadata, notesForTrack, groupByModule, noteSlug/noteHref, url()
│   ├── layouts/Base.astro       header, theme toggle, search, mermaid loader
│   ├── pages/index.astro        track cards with note counts
│   ├── pages/[track]/index.astro   per-track roadmap (modules → notes)
│   ├── pages/[track]/[...slug].astro   note page: sidebar, TOC, sources box, prev/next
│   ├── styles/global.css
│   └── content/notes/<track>/<NN-module>/<NN-note>.md   THE CONTENT (see below)
└── authoring/                   tooling for the C and DSA tracks (see section 5)
    ├── build.py                 assembles templates → src/content/notes
    ├── README.md
    ├── templates/<track>/<module>/<note>.md   note sources with {{include}}/{{run}} directives
    └── examples/<track>/<topic>/…             the tested C/Python/shell sources the notes embed
```

**URL scheme:** `/{track}/{module-slug}/{note-slug}/` — numeric prefixes (`01-`, `02-`) in directory and file names only control ordering and are stripped from URLs. Tracks: `c`, `dsa`, `linux`, `networking` (enum in `content.config.ts`; add new tracks in both that enum and `lib/tracks.ts`).

**Note front matter (all fields required except `sources`):**

```yaml
---
title: ...                  # quote it if it contains ": "
track: networking           # c | dsa | linux | networking
module: Layer 2             # the module title; the SAME string in every note of the module
moduleOrder: 2              # module position within the track
order: 1                    # note position within the module
level: intermediate         # intermediate | advanced | expert
summary: One sentence.      # no ": " unless quoted
sources:
  - label: "RFC 826 — ARP"  # primary sources first; labels are auto-quoted by scripts/check-notes.mjs
    url: https://www.rfc-editor.org/rfc/rfc826
---
```

---

## 3. Content status (as of the last commit)

Total: **95 notes** (site builds to 100 pages: 95 notes + 4 track pages + home).

| Track | Notes | Modules (moduleOrder) |
|---|---|---|
| **Networking** | 40 | 1 Fundamentals (3) · 2 Layer 2 (9) · 3 IPv4 (6) · 4 IPv6 (3) · 5 Routing (8) · 6 Resilience (3) · 7 Overlays and VPN (4) · 8 Multicast (2) · 9 QoS (1) · 10 Linux Networking Labs (1) |
| **Linux** | 31 | 1 Boot and Foundations (3) · 2 Shell and Text Processing (2) · 3 Processes (5) · 4 Memory (2) · 5 Storage and Filesystems (4) · 6 Kernel Interfaces (3) · 7 systemd and Services (2) · 8 Containers (3) · 9 Performance and Observability (3) · 10 Network Stack (2) · 11 Security (2) |
| **C** | 22 | 1 Toolchain and Language Foundations (3) · 2 Memory and Pointers (4) · 3 Data Modeling (2) · 4 Advanced Language (4) · 5 Linking and Building (2) · 6 Debugging and Quality (3) · 7 Systems-Level C (5) |
| **DSA** | 1 | 4 Hashing: *Hash Tables with Open Addressing* (the only note written; it was a seed note) |

Each note's exact file list is visible with `find src/content/notes -name '*.md' | sort`, and per-note status is in `docs/*-outline.md`.

### Note format (the quality bar every note must meet)

- Starts from the **standard/mechanism** (RFC, IEEE clause, ISO C section, POSIX/man page, kernel doc) and explains *why* it is designed that way.
- Tables for field layouts, rules and comparisons; Mermaid diagrams for flows (see traps in section 7); real commands and real output.
- A **pitfalls / failure modes / troubleshooting** section and a **Key takeaways** list at the end.
- A `sources` list citing RFC numbers, IEEE clauses, ISO sections, man pages, papers and books.
- Depth: roughly 2,500–6,000 words per note. The whole corpus is ~170,000+ words.

### How the content was verified (important when judging trust)

- **Networking and Linux notes** were written directly in `src/content/notes/`. Their commands and outputs were captured from real runs where possible (e.g. network namespaces, bridges, `tc`, nftables, `runc` rootless container, `unshare`, overlayfs whiteouts, SSH CA certificates, systemd units/timers, `/proc` data). Command syntax was tested in throwaway user+network namespaces (`unshare -Urn`, `unshare -Urnm`). **Protocol/standard facts (field widths, timers, defaults, clause numbers) come from the author's knowledge, not from fetching the standards** (IEEE documents are paywalled). They need a human spot-check before publishing; a few uncertain claims were removed during writing.
- **C notes** are generated from `authoring/templates/` and embed code that was actually compiled with **GCC 14.2** and run (ASan/UBSan/TSan, `-fanalyzer`, gcov, cmake/make, g++, Python ctypes). Outputs such as timings differ run to run.
- **Not run/verified in the authoring environment:** `gdb`, `clang`/libFuzzer, `valgrind`, `perf`, `strace`, `bpftrace`, kernel headers/module builds, `fio`, `-m32` builds. Notes that show these use clearly labeled command references or "illustrative" code.
- **Never visually checked:** there is no browser on the authoring machine. The rendered site, Mermaid diagrams (syntax was normalized by script, never rendered), and layout have not been seen. A human should open the site and skim every track.

---

## 4. Git and publishing status

- Local repo: `~/notes-app` (also a working copy at `~/deepnotes`, which was the original scratch project; **`~/notes-app` is the source of truth that holds commits**). Sync direction used so far: edit in `~/deepnotes`, then `rsync -a --delete --exclude node_modules --exclude dist --exclude .astro --exclude .git ~/deepnotes/ ~/notes-app/` then `git clean -fdXq authoring/examples` to drop ignored build artifacts, then commit in `~/notes-app`. If you work directly in `~/notes-app`, you don't need the sync (but `node_modules` must be installed there with `npm ci`/`npm i` to build).
- Remote: `https://github.com/dhawan9898/notes-app` was an **empty repository**. A minimal `Initial commit` (README + .gitignore) exists locally on `main`. All work is on branch **`add-deepnotes-site`** (commits: site + networking notes; then Linux + C + authoring tools; then this file).
- **Nothing has been pushed.** The Claude Code remote session has no GitHub write credentials (`could not read Username for 'https://github.com'`) and no `gh` CLI. The user said they cannot use a terminal right now and will push/commit later. A bundle of both branches is at `~/deepnotes-pr.bundle` (`git fetch ~/deepnotes-pr.bundle main add-deepnotes-site:add-deepnotes-site` restores them elsewhere).
- To finish publishing (manual steps for the owner): push `main`, push `add-deepnotes-site`, open a PR (or merge) so `main` contains the site, then in GitHub Settings → Pages choose **GitHub Actions** as the source. The workflow `.github/workflows/deploy.yml` builds with `SITE=https://<owner>.github.io` and `BASE=/<repo>` and deploys `dist/`.
- Commit conventions: the attribution trailer `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>` is added to commits made in these sessions. Do **not** put model identifiers in code comments, notes, or other repo artifacts. Do not create PRs unless asked.

---

## 5. The authoring pipeline (C and DSA tracks)

The C track (and the planned DSA track) is built from templates so that every listing and output on the site is real:

1. Put sources in `authoring/examples/<track>/<topic>/` (C files, Makefile, scripts). Compile and run them; iterate until correct and warning-clean (`-Wall -Wextra`; ASan/UBSan where relevant).
2. Write the note as `authoring/templates/<track>/<NN-module>/<NN-note>.md` with the normal front matter and these directives, one per line:
   - `{{include c/dir/file.c}}` or `{{include c/dir/file.c 10-30}}` — fenced code block (language from extension; `.map` etc. render as `text`).
   - `{{run c/dir :: shell command}}` — runs the command in `authoring/examples/c/dir` (bash, 60 s timeout) and embeds `$ command` plus combined stdout/stderr.
3. `python3 authoring/build.py [path-substring…]` renders templates into `src/content/notes/…`. Runs are sequential within one template, so a later `{{run}}` may use artifacts built by an earlier one. For the DSA track, directory names under `examples/` use `dsa/<topic>`; the include/run paths are relative to `authoring/examples/` (e.g. `{{include dsa/sorting/quicksort.c}}`).
4. Run `node scripts/check-notes.mjs --fix` (or `npm run build`), then `npm run build` to verify the whole site and the Pagefind index.
5. Update the `[x]` status in `docs/<track>-outline.md`.

Rules learned the hard way (section 7 has more): print results in separate statements (argument evaluation order is unspecified); flush or use `stderr` when a program may crash; avoid `pkill -f` in tool commands; keep demo programs deterministic (fixed seeds) or describe results qualitatively; clean artifacts before committing (`authoring/examples/.gitignore` already ignores them).

Networking/Linux notes are plain Markdown written directly under `src/content/notes/` (no templates).

---

## 6. Environment facts of the authoring machine

Debian 13 VM, kernel 6.12, 8 cores (AMD EPYC), **Node 20.19**, npm 9, GCC/G++ 14.2, glibc 2.41, Python 3.13, make, cmake 3.31, nftables, iproute2, systemd 257, OpenSSH 10, runc 1.1, busybox, getfacl, lsof, ASan/UBSan/TSan, gcov. **Missing:** gdb, clang, valgrind, perf, strace, ethtool, tcpdump, xxd, shellcheck, cppcheck, kernel headers, a web browser. Unprivileged user namespaces work (`unshare -Urnm`, rootless runc, unprivileged overlayfs); real root/sudo does not. Outbound network was not used for content.

---

## 7. Traps already hit (do not repeat)

- **YAML front matter:** a colon + space inside an unquoted value (`label: RFC 5120 — M-ISIS: Multi…`, titles, summaries) breaks parsing and the whole build. `scripts/check-notes.mjs --fix` quotes `label:` values automatically (and runs in `npm run build`), but **quote `title:` and `summary:` yourself** when they contain `: `.
- **Mermaid:** in `sequenceDiagram` a `;` inside a message is a statement separator → syntax error; in `flowchart` always quote node labels `A["text, with (parens) / slashes -> arrows"]`; avoid `<` `>` in state-diagram labels. The diagrams have never been rendered here — verify in a browser.
- **`pkill -f <pattern>`** in a tool command also matches the tool's own command line and kills the shell (happened twice). Use `pgrep -x name` + `kill PID`.
- **Argument evaluation order** (`printf("%d %d", f(), g())`) is unspecified and was bitten repeatedly in demos; compute into variables first.
- **Buffered stdout is lost on crash/`_exit`:** print demo results to `stderr` or flush when a crash is part of the demo; for `system()` demos use `setvbuf(stdout, NULL, _IONBF, 0)`.
- **glibc stdio does not call the interposable `write()` symbol**, so `LD_PRELOAD` can't count stdio writes; count via `/proc/self/io` (`syscw`).
- **GCC optimizations hide effects:** `-O1+` turns data-dependent branches into `cmov` (use `-fno-if-conversion -fno-if-conversion2` to show branch misprediction), removes unused `malloc/free`, folds `static` constants (use `-O0` for symbol-table demos), vectorizes float reductions only with `-ffast-math`.
- **gcov:** when compiling several files in one command the data files get combined names; compile objects separately.
- **mawk (Debian default awk)** rejects `--` before the filename; `ip tunnel add … mode ipip` fails in user namespaces where `ip link add … type ipip` works (notes use the latter).
- **`ldd`/Compiler output paths:** strip absolute home paths from embedded sanitizer output (`sed -e 's#/[^ ]*/##'`).
- **Personal data in output:** don't embed hostnames, UIDs, usernames or host key fingerprints from the authoring machine into notes (they were replaced with generic values).
- **Privacy of examples:** the SSH CA example uses throwaway keys generated on the spot; never embed real keys.

---

## 8. What to do next — detailed plan

### 8.1 Immediate next work: the DSA track (≈27 notes, all in C, tested)

Goal: finish `docs/dsa-outline.md` using the authoring pipeline. Every note needs a tested C implementation (compiled with `-Wall -Wextra -fsanitize=address,undefined`, plus randomized/property checks against a simple reference), measured behavior (timings, operation counts, memory) via `{{run}}`, complexity analysis with proofs/intuition, practical/systems context, pitfalls, and Key takeaways. Use `track: dsa`; keep the module names/orders below. Suggested example directory: `authoring/examples/dsa/<topic>/`.

**Module 1 — Foundations (moduleOrder 1)**
1. *Complexity analysis and amortized analysis*: Big-O/Ω/Θ, recurrences and the Master theorem, amortized methods (aggregate/accounting/potential) with the dynamic-array and union-find examples, space complexity, why constants and caches matter; experiment: measure operation counts and time vs n to *show* O(n), O(n log n), O(n²) curves, and a cache-effects plot-in-text (table).
2. *Recursion, divide and conquer, loop invariants*: recursion vs iteration, stack depth limits (link to the C stack note), tail calls, memoization intro, writing and checking loop invariants, correctness proof patterns; examples: binary exponentiation, merge-style D&C, Tower of Hanoi, Karatsuba (optional), explicit-stack conversion of a recursive DFS.

**Module 2 — Linear structures (2)**
3. *Dynamic arrays*: growth factors (1.5 vs 2, memory reuse argument), amortized proof, `realloc` behavior (mremap nuance from the malloc note), generic vector via macros and via `void*`+elem size, bounds checking, iterator invalidation; benchmark vs linked list.
4. *Linked lists*: singly/doubly/circular, sentinel nodes, reversal, cycle detection (Floyd/Brent), merge sort on lists, intrusive lists (`container_of`, link to the C function-pointer note), XOR list as a curiosity, why lists lose to arrays on modern CPUs (measured), skip pointer techniques.
5. *Stacks, queues, deques and ring buffers*: array and list implementations, circular buffer with power-of-two masking, SPSC lock-free ring buffer with C11 atomics (acquire/release; tested with threads + TSan), monotonic stack/queue patterns, expression evaluation (shunting-yard), BFS queue usage.

**Module 3 — Searching and sorting (3)**
6. *Binary search and variants*: lower/upper bound, off-by-one-proof via invariants, `mid` overflow bug, rotated array, search on answer (parametric search), floating-point bisection, `bsearch` pitfalls; fuzz-tested against linear search.
7. *Comparison sorts*: insertion, selection (why not), merge (top-down/bottom-up, stability), quicksort (Lomuto/Hoare, median-of-3, 3-way partition, introsort with heap fallback), heapsort, lower bound Ω(n log n) proof; benchmark table on random/sorted/reversed/few-unique inputs; compare with `qsort`; stability demo; branch-misprediction notes (link to performance note).
8. *Non-comparison sorts and selection*: counting, radix (LSD/MSD), bucket sort, quickselect, median of medians, top-k with heap, external merge sort and k-way merge, sorting networks mention; measure radix vs quicksort on 32-bit keys.

**Module 4 — Hashing (4)** (existing note stays as order 1)
9. *Probabilistic structures*: Bloom filter (sizing formulas `m`, `k`, false-positive rate, measured vs theoretical), counting Bloom, Cuckoo filter overview, Count-Min sketch, HyperLogLog (with measured error), consistent hashing with virtual nodes (measure load balance and key movement), hash function quality (FNV, murmur-style mixers, avalanche test), universal hashing/hash-flooding (SipHash).

**Module 5 — Trees (5)**
10. *Binary trees and BSTs*: traversals (recursive, iterative, Morris), BST insert/search/delete (three cases), height vs n for random vs sorted inserts (measured degeneration), successor/predecessor, LCA, serialization, tree isomorphism basics.
11. *Self-balancing trees*: AVL (rotations, balance factors, insert+delete implementation verified by invariant checker), red-black trees (invariants, insertion cases, comparison with AVL; the kernel `rb_tree`), treap, skip list (expected O(log n), measured), splay trees (amortized); randomized tests validating all invariants.
12. *Heaps and priority queues*: binary heap array layout, sift up/down, O(n) heapify proof, d-ary heaps and cache behavior, indexed heap with decrease-key (for Dijkstra), merge k sorted lists, running median with two heaps, Fibonacci/pairing heap overview; measure heapify vs n inserts.
13. *Tries and radix trees*: trie, compressed (Patricia/radix) tree, longest-prefix match for IP routing (**link to networking forwarding/LPM notes**; implement a /8–/32 IPv4 route lookup and verify vs linear scan), autocomplete, bitwise tries, Linux `radix_tree`/XArray, ART overview.
14. *B-trees, B+ trees and LSM trees*: why disks/pages favor wide nodes (node-size arithmetic from page size), B-tree insert/split implementation (in-memory), B+ tree range scans, write amplification, LSM tree (memtable, SSTables, compaction, Bloom filters, tombstones) as used in RocksDB/LevelDB/Cassandra, comparison table; small LSM toy with measured read/write amplification.
15. *Range-query structures*: Fenwick tree (BIT), segment tree (point/range update, lazy propagation), sparse table (RMQ), prefix sums/2D prefix sums, interval trees, sqrt decomposition; measured vs naive.

**Module 6 — Graphs (6)**
16. *Representations and traversals*: adjacency list/matrix/CSR, BFS/DFS, connected components, bipartite check, topological sort (Kahn/DFS), cycle detection, iterative DFS to avoid stack overflow; relate to dependency resolution (make/systemd ordering).
17. *Shortest paths*: Dijkstra with the indexed heap, Bellman-Ford and negative cycles, Floyd–Warshall, A* with admissible heuristics, BFS on grids, 0-1 BFS, Johnson's; **link to OSPF/IS-IS SPF and BGP/distance-vector notes**; test against brute force on random graphs.
18. *Union-find, MST, SCC and flow*: DSU with path compression + union by rank (inverse-Ackermann, measured), Kruskal, Prim, Borůvka mention (**link to the Spanning Tree note**), SCC (Tarjan/Kosaraju), articulation points/bridges, max-flow (Edmonds–Karp/Dinic) and bipartite matching, min-cut intuition.

**Module 7 — Strings (7)**
19. *String matching and edit distance*: naive, KMP (failure function), Z-algorithm, Rabin–Karp (rolling hash, collision handling), Boyer–Moore–Horspool, Aho–Corasick (multi-pattern), suffix array/automaton overview, edit distance and LCS (link to DP note), UTF-8 awareness; benchmark vs `strstr`/`memmem`.

**Module 8 — Algorithm design (8)**
20. *Dynamic programming*: recognizing overlapping subproblems/optimal substructure, top-down memo vs bottom-up, state design, space optimization; knapsack (0/1, unbounded), LIS (O(n log n)), LCS, edit distance, matrix-chain, coin change, partition, DP on trees/bitmasks pointer; verify against brute force.
21. *Greedy algorithms*: exchange-argument proofs; interval scheduling, Huffman coding (implementation + bit-level encoder), fractional knapsack, MST link, when greedy fails (coin systems), scheduling by deadline.
22. *Backtracking, branch and bound, bitmask DP*: N-Queens (bitmask version), permutations/subsets/combinations generation, sudoku, subset-sum pruning, TSP with bitmask DP, Gosper's hack (link to the bit-manipulation note), pruning and heuristics, counting nodes visited.
23. *Number theory essentials*: gcd/extended Euclid, modular exponentiation, modular inverse, sieve of Eratosthenes (bit-array, segmented), Miller–Rabin, fast Fibonacci/matrix exponentiation, overflow-safe `mulmod` with `__int128`, CRT, combinatorics mod p; link to crypto/security notes.

**Module 9 — Systems-flavored structures (9)**
24. *Caches*: LRU (hash + doubly linked list, O(1)), LFU, CLOCK/second chance, 2Q/ARC overview, TTL caches, hit-ratio simulation under Zipf workloads, relation to the Linux page cache (**link to Linux memory notes**) and CPU caches.
25. *Allocators as data structures*: free lists, segregated fits, buddy allocator, slab/pool, arena (link to the C malloc note), fragmentation measurement, implementing a small `malloc` with coalescing and testing it under random workloads with ASan-style poisoning.
26. *Concurrent and lock-free structures*: mutex-protected queue, Treiber stack with the ABA problem demonstrated, Michael–Scott queue, hazard pointers/epoch reclamation (RCU intuition), concurrent hash table sharding, memory ordering recap (link to the Linux threads note); validate with TSan + stress tests.
27. *Problem-solving patterns and complexity cheat sheet*: two pointers, sliding window, prefix sums, monotonic stack, interval merging, binary search on answer, top-k, union-find usage, topological patterns, how to choose a data structure; a one-page complexity table for all structures built in the track; an "operations per second" reality check table from the measurements.

For each DSA note also: a `moduleOrder`-correct front matter, 4–8 primary sources (CLRS chapters, Knuth volumes, original papers such as Fredman/Tarjan, Bayer–McCreight, Bloom 1970, Flajolet et al. for HyperLogLog, O'Neil et al. for LSM, Michael–Scott, etc.), and cross-links in prose to related notes in other tracks (state the note title; do not hard-code URLs unless using the `/{track}/{module}/{note}/` scheme with the `/notes-app` base).

### 8.2 After DSA: hardening and verification (high value, do in this order)

1. **Visual verification pass** (needs a browser; the owner or a session with Playwright/Chromium): run `npm run build && npm run preview`, open every track page and a sample from every module; check Mermaid rendering (fix syntax errors in the offending diagrams), table overflow on mobile width (320–390 px), code block horizontal scroll, dark mode contrast, TOC, search results, prev/next links, base-path correctness under `/notes-app`. Consider adding a Playwright smoke test script (`scripts/smoke.mjs`) that loads each page and fails on console errors or mermaid error nodes.
2. **Fact-check pass of the standards claims** in Networking and Linux (and the C-standard statements): check numbers/timers/defaults/clause references against the RFCs and IEEE/ISO/POSIX documents (RFC editor and kernel docs are free; IEEE 802 documents are available via "Get IEEE 802" six months after publication). Keep a `docs/fact-check-log.md` listing each verified/changed claim. Prioritize: STP/RSTP/MSTP timers and costs, OSPF/IS-IS/BGP defaults and attribute codes, IPv6 NDP details, DHCP/IPsec/EVPN numbers, QoS DSCP values, Linux sysctl defaults, systemd directive semantics, kernel version-dependent statements (EEVDF 6.6, PREEMPT_RT 6.12, `openat2` 5.6, etc.).
3. **Link checking:** verify every `sources[].url` resolves (a script with `curl -I`/`lychee`); replace dead or guessed URLs (a few were provided from memory, e.g. IEEE and some blog URLs); remove URLs that cannot be verified rather than leaving guesses.
4. **Cross-linking:** add a "Related notes" block (title-based, using real route paths) at the end of notes that reference other notes in prose (e.g. ARP ↔ IPv6 NDP ↔ L2 security; OSPF ↔ DSA shortest paths; C malloc ↔ Linux memory ↔ DSA allocators). Possibly add a generated "Related" section driven by an optional `related:` front-matter array and a small Astro component.
5. **Consistency/editorial pass:** unify terminology (e.g. "FIB/RIB"), heading levels, the tone of "Key takeaways", remove leftover phrasing such as "this host" (a sweep was done; re-grep for ` here`, "this machine"), check that all promised cross-references exist, fix any note whose front-matter `module` string differs from siblings (it creates duplicate module groups).
6. **Site polish (keep it minimal):** active-section highlight in the TOC, "copy" button on code blocks, per-note reading time and level badge, last-updated date, print stylesheet, favicon/OG metadata, a 404 page, `sitemap.xml` (add `@astrojs/sitemap`), `robots.txt`, search filters by track. Keep the design clean and low-dependency.
7. **CI hardening:** add a workflow job that runs `npm ci && npm run build` on PRs (not just deploy on `main`), runs the front-matter checker, optionally `python3 authoring/build.py` and fails if generated notes differ from committed ones (reproducibility), and a link checker on a schedule.

### 8.3 Content expansion backlog (after the above; owner to prioritize)

- **Networking additions:** Wi-Fi 802.11 basics (CSMA/CA, frame types, security WPA3), segment routing/SRv6 in depth, TCP deep-dive note (state machine, congestion control algorithms with packet traces), DNS in depth, TLS/QUIC handshake, SD-WAN/overlay design, data-center fabric design (Clos/EVPN case study), network automation (NETCONF/YANG, gNMI, Ansible, Batfish), packet capture analysis (Wireshark cookbook), OSPFv3/IS-IS lab with FRR containers (containerlab topologies in `authoring`).
- **Linux additions:** package management and build from source, Linux kernel development basics (patch workflow, kernel modules in practice with real builds), KVM/virtualization (libvirt, QEMU, virtio, SR-IOV), SELinux policy writing, bash scripting advanced patterns, `strace`/`perf` captured sessions (needs those tools installed), real-time Linux tuning, filesystem internals deep dive (ext4 on-disk layout with `debugfs`), cgroups v1 vs v2 migration, observability stack (Prometheus/Grafana/OpenTelemetry basics).
- **C additions:** C++ interop deep dive, writing a shell / HTTP server / interpreter capstone projects (each a tested multi-file project under `authoring/examples`), embedded C topics (volatile registers, linker scripts, startup code, interrupts), lock-free programming in C11 atomics (could live in DSA module 9), generic programming patterns, memory-safe subset techniques and Rust FFI.
- **New tracks to consider (owner decision):** Operating-system internals, Distributed systems, Databases, Security engineering, Python/Go systems programming. Add by extending the track enum, `TRACKS`, an outline in `docs/`, and notes.
- **Capstone projects section:** a `projects` module per track with build-it-yourself guides (mini shell, malloc, TCP echo/HTTP server with epoll, ring buffer, hash table, LRU cache, packet sniffer with raw sockets/libpcap, ARP/ICMP responder, routing-table trie) tying notes together; each with tests and a rubric.

### 8.4 Product features (deferred by the owner; do **not** start without being asked)

Progress tracking (per-note done state, module/track progress rings, "continue where you left off"), quizzes, bookmarks/notes, spaced repetition, accounts/sync, and the **Android app** (wrap the static build with Capacitor; offline support via a service worker; check Pagefind index works offline; app icons and splash). When started, begin with local-only storage (`localStorage`/IndexedDB, wrapped in try/catch) before any backend.

---

## 9. Working agreements and conventions for future sessions

- **Always build and check before declaring done:** `node scripts/check-notes.mjs --fix` and `npm run build` must pass. For C/DSA templates also run `python3 authoring/build.py <note>` and read the generated output blocks.
- **Be honest about verification:** say what was run and what was not; never invent command output. If a tool is missing, label the block as a reference/illustrative.
- **Don't expose personal data or machine-specific values** in notes (hostnames, UIDs, home paths, key material, IPs of the authoring VM).
- **Prefer extending existing notes' conventions** (module names/orders, level badges, sources format) so the site structure stays coherent. When adding a note to an existing module, set `order` correctly and update the outline `[x]`.
- **Commit/push policy:** commit only when asked (the owner asked for a commit after the C track; pushes need credentials the remote session lacks). Never force-push or rewrite the owner's history. If asked to raise a PR and credentials are missing, say so and offer the bundle.
- **Before a long writing run, state the plan** (module list), write notes in batches of 2–4, build after each batch, and keep the outline files current so work can resume after context loss.
- **Scale expectations:** a deep note takes substantial effort; prefer fewer, correct, tested notes over many shallow ones.

---

## 10. Quick command reference

```bash
npm install                                  # first time (Node ≥ 20; Astro 5)
npm run dev                                  # http://localhost:4321  (base path "/" in dev)
npm run build                                # check front matter (+fix labels) then astro build + pagefind
npm run preview                              # serve dist/
node scripts/check-notes.mjs [--fix]         # front-matter validation only
python3 authoring/build.py [substring…]      # regenerate templated C/DSA notes
SITE=https://dhawan9898.github.io BASE=/notes-app npm run build   # reproduce the Pages build locally
find src/content/notes -name '*.md' | awk -F/ '{print $4}' | sort | uniq -c       # notes per track
git log --oneline --all | head                                                      # history
git bundle create ~/deepnotes-pr.bundle main add-deepnotes-site                     # portable copy of the work
```
