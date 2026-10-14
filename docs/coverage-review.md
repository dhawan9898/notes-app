# Coverage and quality review (intern view and expert view)

Reviewed: all 122 notes (structure checked automatically for every note; content read in depth for a sample from each track: IPv4 addressing, Ethernet/L2 notes, C pointers, Linux filesystem/boot, and the DSA notes written in the latest sessions, plus keyword-coverage census across tracks). Keyword counts below show where a topic is only *mentioned* in passing, which is not coverage.

## 1. What is solid

- **Uniform structure.** Every note has a summary, 3+ cited sources (465 source URLs in total), a Key takeaways section and, except the RFC-reading guide, code or command examples. Module order is a coherent learning path in every track.
- **Evidence.** C and DSA listings are compiled and run (sanitizers, brute-force oracles, measured output). Networking and Linux commands were exercised in throwaway namespaces where possible.
- **Expert depth where it exists.** DSA, C systems topics, Linux performance/containers/eBPF, and the routing/overlay notes (OSPF, IS-IS, BGP, MPLS, EVPN) go well beyond tutorials.

## 2. Findings

### 2.1 Depth is uneven across tracks

| Track | Notes | Typical size (words incl. code) | Reading |
|---|---|---|---|
| DSA | 28 | 3,000-6,000 | meets the "2,500-6,000 words" bar; every claim measured |
| C | 23 | 1,800-3,100 | good; a few notes (Pointers 1.3k, Preprocessor 1.8k) are thin for their importance |
| Linux | 31 | 1,250-2,250 | compact reference style, below the stated bar |
| Networking | 40 | 900-2,000 | **thinnest track**; many notes are a dense cheat sheet (e.g. "A Packet's Journey" 900 words, "Bridge-Reserved Addresses" 900) |
| DSA seed | *Hash Tables with Open Addressing* | 1,200 | **inconsistent with the rest of DSA**: no authoring-pipeline tests or measurements, no chaining, resizing, deletion or Robin Hood detail demonstrated |

### 2.2 Intern view: the basics are the weak spot

The notes are written for someone who already works in the field. An intern would hit these walls:

1. **No on-ramp.** Each track starts at an intermediate level. Examples:
   - C: *Pointers, Arrays and Decay* opens with "A pointer is not an integer", provenance and §6.3.2.1 before ever showing what `&` and `*` do, what an address is, or why pointers exist. There is no "C from zero" note (variables, control flow, functions, arrays, `printf`/`scanf`, compiling and running, reading compiler errors).
   - Networking: *IPv4 Addressing* starts at "32 bits written as four decimal octets" with no binary primer (bits, bytes, hex, binary to decimal, why subnets exist). There is no beginner "what is a network" note (LAN/WAN, switch vs router vs hub, MAC vs IP in plain words, a first ping and what happened).
   - Linux: no "using the shell day to day" (ls, cd, cp, find, pipes at beginner level, man pages, editors), package management, cron, disk mounting/fstab walk-through, or network configuration (NetworkManager/netplan/systemd-networkd).
   - DSA: starts at complexity analysis; no "arrays and strings in C" refresher, and no explicit prerequisites.
2. **No glossary and no stated prerequisites.** Notes use terms (FIB, RIB, LSDB, NAPI, cgroup, ABI, UB, amortised) defined only in other notes or not at all. No per-note "you should know first" line; no per-module "start here" path from beginner to expert.
3. **Worked examples are sparse for first-time learners** in networking and Linux (subnetting has one example; there is no practice-problem set, no "try it yourself" lab with expected output for each module, and no exercises with answers in any track).
4. **Mixed levels inside notes.** Intermediate notes switch to standards citations or kernel internals without a signpost.

### 2.3 Expert view: topic coverage gaps

Keyword counts are notes that mention the term at all; most are passing mentions.

**Networking (largest gaps)**

- **Transport layer is missing as a topic**: TCP (state machine, handshake, retransmission, congestion control: Reno/CUBIC/BBR, window scaling, SACK) and UDP appear only inside Linux tuning and QoS notes (2 notes mention); there is no TCP note. No QUIC/HTTP-3, no TLS handshake note (mentioned in 4 notes only).
- **DNS** (resolution, records, caching, DNSSEC) appears in 1 note only: no dedicated note. DHCP covered; DNS is a fundamental service gap.
- **Wi-Fi / 802.11** (CSMA/CA, frames, roaming, WPA2/WPA3, 802.1X in wireless) is a Layer 2 topic with no note.
- **Operations and observability**: no packet-capture/Wireshark analysis note (tools are used in examples, but analysis technique is not taught), no SNMP/streaming telemetry/NetFlow/IPFIX note, no NTP/PTP time synchronisation, no syslog.
- **Automation**: NETCONF/YANG/gNMI/Ansible appear in 1 note; no network-automation note.
- **Routing/overlay breadth**: no OSPFv3 or EIGRP note (only mentions), no Segment Routing/SRv6, no BGP in the data centre (RFC 7938 Clos designs), no MPLS-TE/RSVP-TE, no SD-WAN, no MLAG/vPC design note, no NAT64/DNS64 detail, no IPv6 security/first-hop security (RA Guard).
- **Design level**: no end-to-end design/case-study note (campus, data-centre Clos, WAN) tying the modules together.

**Linux**

- Missing fundamentals: package management (apt/dnf/rpm), editors, logging beyond journald (rsyslog), time sync (chrony), network configuration tools, backup (rsync, tar, snapshots), kernel build/configuration.
- Missing advanced: virtualisation (KVM/QEMU/libvirt/virtio/SR-IOV: mentioned in 5 notes only), kernel debugging (kdump/crash, kgdb), RCU and kernel memory allocators (slab/SLUB: passing mentions), real-time Linux (PREEMPT_RT tuning), Kubernetes/CNI networking (mentioned, not taught), a boot-time/systemd-analyze performance note, cgroup v1 to v2 migration.

**C**

- Missing: beginner fundamentals (above); `volatile`/memory-mapped I/O and embedded C (linker scripts: 0 notes; inline assembly: 1), C11 atomics and memory model as a standalone note (touched in 6 notes), threads in C (`<threads.h>`, `pthread` beyond the Linux note), the standard library tour (`<time.h>`, locale/wide characters in one note), generic-programming patterns beyond `_Generic`, C/C++ and C/Rust FFI, capstone projects (shell, HTTP server, allocator, interpreter).

**DSA (strongest track, remaining gaps are specialised)**

- Hash tables: the seed note lacks chaining, resizing, deletion (tombstones vs backward-shift), cuckoo/perfect hashing, SipHash and hash-flooding demonstrated with code.
- Strings: suffix arrays/automata/FM-index only described, not implemented; no Manacher/palindromes, no tries-based autocomplete at scale (covered partly).
- Trees: no LCA/Euler-tour/heavy-light/centroid decomposition, no k-d trees/quadtrees/R-trees (spatial indexes), no persistent/functional structures, no cache-oblivious layouts beyond Eytzinger.
- Graphs: no 2-SAT/matching beyond flow, no Hopcroft-Karp/Hungarian, no planarity/graph colouring, no dominator trees, no network-simplex/min-cost flow.
- Algorithms: no computational geometry (convex hull, sweep line, closest pair), no FFT/NTT implementation, no randomized algorithms (reservoir sampling, Monte Carlo/Las Vegas, treap analysis), no approximation algorithms, no online algorithms/competitive analysis, no external-memory sorting code, no streaming algorithms beyond sketches.

### 2.4 Quality risks that cut across tracks

- **Unverified standards claims** in Networking/Linux (timers, defaults, clause numbers written from knowledge, not checked against the RFCs/IEEE/kernel docs). Needs the fact-check pass in CLAUDE.md section 8.2.
- **Visual verification incomplete**: three pages were viewed in a browser; Mermaid diagrams, mobile width, dark mode and search were not checked.
- **Cross-links are prose only** ("see the X note"); there is no related-notes component, glossary, or reading-path page, which hurts both interns (what first?) and experts (where next?).
- **Timing prose goes stale on rebuild** for C/DSA notes (every build re-measures); each quoted ratio was checked once but should be re-verified after any rebuild.

## 3. Recommended work, in priority order

**Status:** P0 items 1-4 are done in a first version (Start Here module per track, prerequisites boxes on 33 existing notes, glossary of about 150 terms, one exercise note per track, 7 beginner notes). Remaining from P0: per-module exercises, linking glossary terms from first use inside notes.

**P1 progress (2026-10-13):** all P1 items are done except three networking topics that need protocol daemons or vendor gear which this environment does not have (OSPFv3/EIGRP, MLAG/vPC, SD-WAN). Added since the review: networking (TCP/UDP, DNS, TLS/QUIC, Wi-Fi, packet capture, SNMP/telemetry, NTP/PTP, SRv6, automation, data-centre Clos/ECMP, NAT64/DNS64); Linux (kernel allocators and RCU, latency/real-time, virtualization with KVM, kernel config/build/kdump, Kubernetes networking); C (atomics and the memory model, embedded C, FFI, standard library tour); DSA (hash tables rewritten, tree queries, spatial indexes, FFT/NTT, suffix arrays, computational geometry, randomized algorithms). P2: LACP, bridge reserved addresses/LLDP and BFD gained labs. P3: link check (all source URLs reachable, `docs/link-check.md`), fact-check log (`docs/fact-check-log.md`), CI checks workflow, 'Continue with' box on every note, all 37 Mermaid diagrams parse. Remaining: P2 deepening of other thin notes (C Pointers/Preprocessor), per-module exercises, glossary term linking.

**P0: make the site usable for beginners (small effort, high value)**

1. A **"Start here" page per track** with prerequisites, a beginner-to-expert reading path, and estimated time; add a one-line "Prerequisites" and "Level" banner to every note (front-matter field `prerequisites`).
2. A **site-wide glossary** (about 150 terms) linked from first use.
3. **Beginner notes (new)**: C from zero; Pointers for beginners (before the current note); Binary, hex and bit basics for engineers; What is a network (LAN/WAN, devices, first ping); Linux command-line basics; Linux package management and services for beginners; Arrays, strings and recursion refresher for DSA.
4. **Exercises**: 5 to 10 graded exercises with answers at the end of each module (subnetting drills, shell tasks, C debugging tasks, DSA problems).

**P1: close the biggest expert gaps (new notes)**

5. Networking: TCP deep dive; UDP/QUIC/TLS; DNS; Wi-Fi 802.11; packet-capture analysis; SNMP/telemetry/NetFlow; NTP/PTP; network automation; data-centre Clos with BGP; Segment Routing; OSPFv3/EIGRP; design case studies.
6. Linux: virtualisation (KVM/QEMU/virtio); kernel debugging and kdump; RCU and kernel memory allocation; real-time tuning; networking configuration tools; package management; Kubernetes networking.
7. C: atomics and the C11 memory model; embedded C (volatile, linker scripts, startup code); threads in C; standard library tour; FFI with C++/Rust.
8. DSA: rewrite the hash-table note to the track standard; tree algorithms (LCA/HLD/centroid); spatial indexes; computational geometry; FFT/NTT; randomized algorithms; suffix array implementation.

**P2: deepen thin notes (revision, not new notes)**

9. Expand the 20 shortest Networking and Linux notes toward 1,800 to 2,500 words with worked examples, captures and troubleshooting walk-throughs; expand C *Pointers* and *Preprocessor*.

**P3: verification and tooling (from CLAUDE.md section 8.2)**

10. Fact-check standards claims and keep a log; link-check all source URLs; browser verification (Mermaid, mobile, dark mode, search); CI that runs the build and checkers on every PR; related-notes component.

Rough size: P0 about 8 new notes plus 4 pages and exercises; P1 about 30 notes; P2 about 20 revisions.

## 5. Continuity review and fixes (second pass)

A learner-path review of all notes (prerequisite graph, level ramp, forward references, first use of jargon) found these problems; each has a fix in the repository.

| Finding | Fix |
|---|---|
| Beginner cliff: only 20 of 163 notes were beginner; Start Here jumped to ELF/UB | New beginner **Essentials** module (moduleOrder 0.5) in every track: 5 C, 4 DSA, 6 Linux, 5 Networking notes |
| C never taught `struct`, `enum`, `typedef`, `switch`, headers/multi-file builds, 2-D arrays, `argv`, files | C Essentials notes 1-4, all code compiled and run |
| No bridge from beginner C to ELF/ABI/memory notes | *From Source to Running Process: A Bridge to Systems C*; added as a prerequisite of the pipeline and stack-frame notes |
| DSA beginner path had only one note before a 3,800-word complexity note | Four DSA Essentials notes (search/sort, lists/stacks/queues, hash tables, trees/graphs) |
| Heap sort used before heaps are introduced | Heap primer added to the Comparison Sorts note |
| Linux: Boot internals came before the shell, permissions and day-to-day tools; no processes/cron, disks/archives, network commands/SSH, scripting, editors/Git for beginners | Six Linux Essentials notes; module order is now Essentials, Shell and Text, Boot and Foundations (filesystem, users, boot) |
| Networking: no end-to-end beginner story (DNS, TCP, TLS, HTTP), subnetting primer, home-network devices or troubleshooting method | Four Networking Essentials notes; *Practice: Networking Exercises* moved after the subnetting primer |
| Missing/duplicate prerequisites (ARP, TCP, DNS, Process Lifecycle, Practice note) | Prerequisites added or corrected |
| Recommended path ran all of C (42 notes) before DSA, Linux or Networking | `learningPath()` now builds Start here, Essentials (interleaved) and a Core path that takes five notes from each track in turn |



### Exercises pass

Practice notes (6-7 graded exercises, answers behind `<details>`, every answer verified by running it) now close the core modules: C modules 1, 2, 4 and 7; DSA modules 2, 4, 5, 6 and 8; Linux modules 2 (Shell), 3, 5 and 7; Networking modules 2, 3, 4 and 5. The networking labs run on one machine in a throwaway user, network and mount namespace (`unshare -Urnm`, then `mount -t tmpfs tmpfs /run` so that `ip netns` works). Added bridging notes: a C capstone project, DSA *Measuring Growth* and *Depth-First Search and Ordering Tasks*, Linux *Reading Logs*, Networking *Wi-Fi for Beginners* and *Planning a Small Office Network*.

Remaining known gaps: practice notes do not yet exist for C modules 3, 5, 6 and 8, DSA modules 1, 3, 7 and 9, Linux modules 1, 4, 6, 8-11, and Networking modules 1 and 6-11; protocol facts are still unverified against the standards.
