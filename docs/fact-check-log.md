# Fact-check log

How claims in the notes are verified. Three levels:

1. **Executed**: the note embeds code and its real output (`authoring/` pipeline); the claim is whatever the program printed, and generated notes are rebuilt from the sources.
2. **Checked against the primary source** on the date shown (the standard was fetched and the passage read).
3. **From memory of the documentation**: not re-fetched. These are the highest-risk statements; check them before relying on them for something important.

## Checked against primary sources (2026-10-13)

| Note | Claim | Source and result |
|---|---|---|
| NAT64, DNS64 and 464XLAT | NAT64 default timeouts: UDP 5 min, TCP established 2 h + 4 min, TCP transitory 4 min | RFC 6146 §3.5.x: UDP_DEFAULT 5 minutes, TCP_TRANS 4 minutes, established = 2 hours + 4 minutes. Confirmed. |
| NAT64, DNS64 and 464XLAT | RFC 6052 embedding of 192.0.2.33 for /32, /40, /48, /56, /64, /96 | RFC 6052 §2.4 table. The note's program output matches all six rows. |
| BFD | Single-hop BFD uses UDP source port 49152-65535 and TTL/hop limit 255 on send and receive | RFC 5881 §4 and §5. Confirmed. |
| BFD | Sessions not Up use a desired transmit interval of at least 1 s | RFC 5880 §6.8.3. Confirmed. |
| SRv6 | SRH is routing type 4 | RFC 8754 §2. Confirmed. |

## Verified by experiment

| Note | Claim | How |
|---|---|---|
| Clos / ECMP | Linux hashes L3 only by default (one path for one host pair); policy 1 spreads flows; a single flow never splits; a dead next hop is skipped | namespace lab output |
| LACP | LACPDU layout and state byte 0x3f; hash policies layer2, layer2+3 and layer3+4 distribute as described; a static-vs-LACP pair leaves ports in separate aggregators | lab output and decoder |
| Bridge reserved addresses | Linux bridge filters LLDP/LACP/PAUSE/EAPOL addresses; relays the STP address only with STP off; `group_fwd_mask` bits 0-2 refused | lab output |
| BFD | detection time = remote multiplier x negotiated interval; slow start; TTL check | own RFC 5880 implementation, lab output |
| NAT64 | translation of ICMP, UDP and a 20 KB TCP stream works with recomputed checksums | TUN-based gateway lab |
| Kubernetes networking | per-connection random DNAT; client IP preserved across nodes; hairpin needs masquerade | nftables lab |
| TCP/UDP, DNS, TLS, Wi-Fi, SNMP, NTP, SRv6, automation | see each note's lab | lab output |
| C atomics | racy counter loses updates; x86-64 orders compile as described; TSan flags the race | compiled and run |
| Embedded C | volatile changes compiled code; linker-script symbols; gc-sections shrinks the image | compiled and run |
| Hash tables | measured probe counts match Knuth's formulas; SipHash test vector; flooding attack cost | compiled and run |
| FFT/NTT, suffix arrays, geometry, k-d trees, tree queries, randomized algorithms | each implementation is compared against a naive implementation | test programs in each note |
| Virtualization | KVM VMM runs a guest; exit cost measured | compiled and run (nested virtualization inflates the cost; the note says so) |
| RCU | unsafe free is observable; read-side throughput vs mutex | compiled and run |
| Kernel build | module builds against installed headers; modinfo output; kernel config summary | run on the host kernel |

## Level 3: statements from memory of the documentation (re-verify when it matters)

- Kernel allocators note: slab size classes (8, 16, 32, 64, 96, 128, 192, ...), watermark behavior, `SLAB_TYPESAFE_BY_RCU` semantics.
- Latency note: PREEMPT_RT merged in mainline 6.12; `sched_rt_runtime_us` default 950000; SCHED_RR default time slice 100 ms.
- Virtualization note: EPT worst-case 24 memory references per TLB miss; Firecracker boot time and overhead figures; Java FFM API final in Java 22.
- Kubernetes note: default NodePort range 30000-32767; `ndots:5` default; EndpointSlice and kube-proxy mode descriptions.
- Embedded C note: Cortex-M vector table layout (word 0 initial SP, word 1 reset handler).
- FFI note: Rust, Go, Java, Node.js descriptions (no such toolchains were available to compile examples).
- DSA notes: historical and complexity statements (Karger-Stein bound, Harvey-van der Hoeven, SA-IS linear time, Cuckoo 50 % load threshold) from the cited papers' abstracts.

## Process

- Source URLs are checked by `scripts/check-links.py`; the latest report is `docs/link-check.md`. Run it before a release and weekly via the `Checks` workflow.
- When a generated note is rebuilt, the output changes for timing-dependent labs; prose around such output must use ranges, not run-specific numbers.
