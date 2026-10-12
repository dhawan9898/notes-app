---
title: "NTP, PTP and Time Synchronization"
track: networking
module: Operations and Automation
moduleOrder: 11
order: 2
level: intermediate
summary: "Why correct time matters, the NTP packet exchange and the offset and delay formulas derived and demonstrated over real UDP (including the proof that path asymmetry causes an unfixable error), filtering, stratum, stepping versus slewing, NTS, leap seconds, and PTP for sub-microsecond accuracy."
prerequisites:
  - "TCP and UDP: The Transport Layer"
  - "Network Monitoring: SNMP, Flow Export and Streaming Telemetry"
sources:
  - label: RFC 5905 — Network Time Protocol Version 4 (Protocol and Algorithms Specification)
    url: https://www.rfc-editor.org/rfc/rfc5905
  - label: RFC 8915 — Network Time Security for the Network Time Protocol
    url: https://www.rfc-editor.org/rfc/rfc8915
  - label: IEEE Std 1588-2019 — Precision Time Protocol (PTP)
  - label: Mills — Computer Network Time Synchronization, The Network Time Protocol on Earth and in Space, 2nd ed.
  - label: chrony documentation — comparison of NTP implementations and chrony.conf
    url: https://chrony-project.org/documentation.html
  - label: Linux kernel documentation — PTP hardware clock infrastructure
    url: https://www.kernel.org/doc/html/latest/driver-api/ptp.html
---

Every computer has a clock, and no two clocks agree: crystals run slightly fast or slow (typically tens of parts per million, i.e. seconds per month to seconds per day depending on temperature and quality), and clocks that start the same time drift apart. Yet a surprising amount of networking and security depends on agreed time:

- **Logs and incident response**: events from different devices can only be ordered if their clocks agree to better than the time between events.
- **Security**: TLS certificates have validity dates (a clock years wrong makes every certificate "expired" or "not yet valid"); Kerberos rejects tickets if clocks differ by more than about five minutes; one-time passwords (TOTP) and DNSSEC signatures have time windows.
- **Distributed systems**: databases that order transactions by timestamp, lease and lock expiry, cache TTLs, certificate rotation, scheduled jobs.
- **Networking**: measuring one-way delay, correlating flow records and SNMP samples, BFD/ OAM timers, and in mobile networks and finance (MiFID II requires timestamps accurate to 100 µs in high-frequency trading) very tight accuracy.

Two protocols distribute time: **NTP** (milliseconds across the Internet, sub-millisecond on a LAN) and **PTP** (sub-microsecond with hardware support).

## NTP architecture

NTP (RFC 5905) works in a hierarchy of **strata**. **Stratum 0** devices are the reference clocks (GPS receivers, atomic clocks, radio clocks); a **stratum 1** server is directly attached to one; a stratum 2 server synchronises to stratum 1 servers, and so on up to stratum 15 (16 means "unsynchronised"). A client normally polls several servers (three to five is the rule of thumb) and chooses and combines them, which protects against one server being wrong. Public pools (`pool.ntp.org`), cloud providers and organisations' own servers (internal servers synchronised to GPS or to trusted upstreams) are typical choices. NTP runs on **UDP port 123** and uses 48-byte packets; the clients poll at intervals from 64 seconds (2⁶) to about 17 minutes (2¹⁰) or longer when stable.

### The packet

| Field | Meaning |
|---|---|
| Leap Indicator (2 bits) | warns of a coming leap second or an unsynchronised clock |
| Version, Mode | version 4; mode 3 = client request, 4 = server reply, 5 = broadcast |
| Stratum | distance from the reference clock |
| Poll, Precision | the poll interval and the clock's resolution (powers of two) |
| Root Delay, Root Dispersion | accumulated round-trip delay and uncertainty to the stratum-1 source |
| Reference ID | which source this server is synchronised to |
| Reference, **Origin, Receive, Transmit** timestamps | four 64-bit timestamps (32 bits of seconds since 1900 plus 32 bits of fraction, ≈ 233 ps resolution) |

The 32-bit seconds count wraps in **2036** (the end of "era 0"); NTPv4 timestamp handling keeps working beyond it by interpreting the era relative to the current time.

## Measuring offset and delay

NTP measures with a four-timestamp exchange:

- **T1**: the client sends the request, stamped with the *client's* clock.
- **T2**: the server receives it, stamped with the *server's* clock.
- **T3**: the server sends the reply, stamped with the server's clock.
- **T4**: the client receives the reply, stamped with the client's clock.

Let the unknown clock **offset** be `θ` (how far the server's clock is ahead of the client's), and suppose the one-way network delays are `d1` (client to server) and `d2` (server to client):

- `T2 = T1 + θ + d1` and `T4 = T3 − θ + d2`.
- Adding: `(T2 − T1) + (T3 − T4) = 2θ + (d1 − d2)`.
- Hence the **offset estimate** `θ̂ = ((T2 − T1) + (T3 − T4)) / 2 = θ + (d1 − d2)/2`.
- The **round-trip delay** is `δ = (T4 − T1) − (T3 − T2) = d1 + d2` (total time minus the server's processing time).

The result contains one fundamental limitation: **the estimate is exactly right only if the path is symmetric (`d1 = d2`)**. Any asymmetry produces an error of `(d1 − d2)/2` that **no protocol exchange can detect**, because the clocks and the network each contribute unknowns and there is one equation. The maximum possible error is bounded by half the round-trip delay `δ/2` (when all the delay is on one leg), which is why NTP prefers the samples with the *lowest* delay, and why asymmetric paths (cable and DSL upstream congestion, satellite links, some VPNs, load-balanced paths) limit accuracy.

## A live experiment

The program implements the exchange over real UDP between two network namespaces. The server answers with its true clock; the client's clock is deliberately **250 ms ahead**, so the correct offset to report is −250 ms. `tc netem` shapes the path in four ways: symmetric, asymmetric, jittery with one sample, and jittery with the best of sixteen samples.

{{include networking/ntp/ntp_toy.py}}

{{include networking/ntp/lab.sh}}

{{run networking/ntp :: unshare -Urnm sh lab.sh 2>&1}}

- **Symmetric 10 ms each way**: the delay measures 20 ms and the offset comes out at −250.0 ms: exact, as the formulas promise.
- **Asymmetric (2 ms out, 22 ms back)**: the delay is 24 ms, and the offset is wrong by **exactly (2 − 22)/2 = −10 ms** (it reports about −260 ms rather than −250). The measured delay (24 ms) cannot reveal the asymmetry; this is the unfixable error.
- **Jittery path, one sample**: with delays varying randomly by ±8 ms each way, a single sample can be off by several milliseconds (how much depends on that sample's luck; the run shown may happen to be lucky).
- **Jittery path, best of 16 samples**: the individual estimates spread over several milliseconds, but the sample with the **lowest delay** is the least contaminated by queueing, and the error shrinks to about a millisecond or two. This is NTP's *clock filter*: keep the last eight samples and trust the one with the smallest delay. Real NTP daemons then combine and weight several servers (selection, clustering and combining algorithms in RFC 5905) and discard "falsetickers" that disagree with the majority.

## Correcting the clock: step versus slew

Once the offset is known the daemon corrects the clock in one of two ways:

- **Slewing**: temporarily change the clock's *rate* by a small amount (ntpd limits it to 500 ppm, so correcting 100 ms takes 200 seconds), so time stays monotonic and never jumps. This is the normal mode.
- **Stepping**: set the clock instantly, used only when the offset is large (default threshold 128 ms in ntpd, and typically only at start-up). A step can move time **backwards**, which breaks software that assumes it never does (timeouts, databases, `make`). Applications should use the *monotonic* clock (`CLOCK_MONOTONIC`) for measuring durations and wall-clock time only for timestamps.

Between corrections the daemon also learns the local oscillator's **frequency error** and compensates for it, so the clock stays good between polls: this is the *discipline* algorithm (a phase- and frequency-locked loop).

## Implementations on Linux

| Daemon | Notes |
|---|---|
| **chrony** (`chronyd`) | default on many distributions (RHEL, Ubuntu server images, cloud); fast convergence, handles intermittent connectivity and virtual machines well; `chronyc tracking`, `chronyc sources -v`, `chronyc sourcestats` |
| **ntpd** (reference implementation) | the classic; very configurable; `ntpq -p` |
| **systemd-timesyncd** | a simple SNTP client in systemd (no server, no multi-source selection): fine for desktops, not for infrastructure that needs accuracy |
| `ntpdate`/`sntp` | one-shot setters |

Reference commands (chrony is not installed in the environment that produced the lab above, so these were not run for this note): `chronyc tracking` shows the **system time offset**, **frequency error** (ppm), **root delay** and **root dispersion** (the accumulated error bounds); `chronyc sources -v` lists servers with their reachability, stratum, last sample and estimated error; `timedatectl` shows whether the system clock is synchronised; a minimal client configuration is a few `server`/`pool` lines plus `makestep 1.0 3` (step the clock if it is off by more than a second during the first three updates).

## Security

NTP replies can be forged by an on-path attacker, shifting a clock and thereby breaking certificate validation, Kerberos and log integrity. **Network Time Security** (NTS, RFC 8915) adds authentication: an initial TLS-protected key-establishment (NTS-KE on TCP port 4460) yields cookies and keys, after which ordinary NTP packets carry authenticator extension fields. Other practical measures: use several independent sources, restrict who may query or control your servers (`restrict` / `allow`), do not expose NTP servers to the Internet unless you intend to serve it (**amplification**: old `monlist` queries were used in large DDoS attacks), and prefer internal stratum servers on isolated networks.

## Leap seconds

Earth's rotation is irregular, so UTC occasionally adds a **leap second** (23:59:60) to stay within 0.9 s of astronomical time; the last one was at the end of 2016. A system that does not handle it can see time repeat or software crash. Operators handle it by **stepping** (the clock repeats a second), **smearing** (spread the extra second over many hours, as several cloud providers do; do not mix smeared and non-smeared servers), or by using a time scale without leaps (TAI, GPS time). The international metrology community has decided to stop adding leap seconds by 2035.

## PTP: precision time

NTP's accuracy is limited by software timestamping and path asymmetry. The **Precision Time Protocol** (IEEE 1588, PTP) reaches **sub-microsecond to nanosecond** accuracy by (1) taking timestamps in **hardware** at the network interface, at the exact moment a packet leaves or arrives, and (2) making the network devices participate:

- A **grandmaster clock** (usually GPS-disciplined) is elected by the **Best Master Clock Algorithm**.
- **Boundary clocks** terminate PTP on each port and re-originate it (a switch that is itself a precise clock); **transparent clocks** measure each packet's **residence time** in the switch and add it to a correction field, so queueing delay does not distort the measurement.
- The same four-timestamp idea is used (Sync, Delay_Req/Delay_Resp, or peer-delay mechanisms), over UDP multicast or directly in Ethernet frames.

On Linux, NICs with hardware timestamping expose a **PTP hardware clock** (`/dev/ptp0`); `ptp4l` runs the protocol and `phc2sys` copies the hardware clock to the system clock (`ethtool -T eth0` shows capabilities). PTP is the standard for 5G transport, broadcast video (SMPTE ST 2110), power utilities, industrial automation and trading. Its precision depends on **every** device on the path supporting it and on stable, symmetric cabling: a single non-PTP switch in the path destroys it.

## Failure modes

| Symptom | Likely cause | Check |
|---|---|---|
| Clock off by hours/years; TLS errors | no NTP, VM clock not synchronised, dead battery/RTC, wrong time zone | `timedatectl`, `chronyc tracking` |
| Daemon never synchronises | UDP/123 blocked, DNS for the server fails, too few reachable sources, stratum 16 upstream | `chronyc sources`, firewall |
| Time jumps back after resume or migration | VM pause/live migration or step correction | use the monotonic clock; allow slew only (`-x`) in sensitive systems |
| Accuracy poor over a link with asymmetry | path asymmetry error `(d1 − d2)/2` | use closer or wired sources; PTP with hardware timestamping |
| Logs from two devices disagree | one device unsynchronised or in the wrong time zone | compare to a reference; log in UTC |
| Smeared and unsmeared servers mixed | clients disagree by up to half a second around leap events | use one policy |

## Key takeaways

1. Correct time underpins TLS, Kerberos, logs, and distributed systems; NTP provides milliseconds, PTP sub-microsecond.
2. NTP's four timestamps give `offset = ((T2−T1)+(T3−T4))/2` and `delay = (T4−T1)−(T3−T2)`; the offset is exact only on a symmetric path, with error `(d1−d2)/2` otherwise, as the experiment confirms.
3. Choosing the lowest-delay sample (the clock filter) and combining several servers makes the estimate robust against queueing jitter and wrong servers.
4. Clocks are normally slewed (rate-adjusted) and only stepped for large errors; measure durations with the monotonic clock.
5. Use authenticated time (NTS), several independent sources, and a consistent leap-second policy.
6. PTP gets its precision from hardware timestamps and PTP-aware network devices; one unaware device breaks it.
