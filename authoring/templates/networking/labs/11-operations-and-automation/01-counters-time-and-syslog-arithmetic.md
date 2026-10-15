---
title: Counters, Time and Syslog Arithmetic
track: networking
module: Operations and Automation
moduleOrder: 11
order: 1
level: intermediate
minutes: 40
env: browser
terminal:
  - ip link set lo up
  - ./ops
summary: "Four exercises on the operations module with a C program: turning two polls of interface counters into rates and error figures, handling 32-bit counter wrap, computing clock offset and delay from the four NTP timestamps, and decoding syslog priority values, each checked against real data from the loopback interface."
prerequisites:
  - "Network Monitoring: SNMP, Flow Export and Streaming Telemetry"
  - "NTP, PTP and Time Synchronization"
  - "Network Automation: Declarative State, NETCONF/YANG, gNMI and Safe Changes"
sources:
  - label: "RFC 5424 — The Syslog Protocol"
    url: https://www.rfc-editor.org/rfc/rfc5424
  - label: "RFC 5905 — Network Time Protocol Version 4 (offset and delay formulas)"
    url: https://www.rfc-editor.org/rfc/rfc5905
  - label: "RFC 2863 — The Interfaces Group MIB (counters and ifHCInOctets)"
    url: https://www.rfc-editor.org/rfc/rfc2863
  - label: "man 5 proc (/proc/net/dev)"
    url: https://man7.org/linux/man-pages/man5/proc.5.html
---

Monitoring tools are mostly arithmetic on counters, timestamps and numeric codes. Doing the arithmetic once by hand makes every dashboard readable, and shows where it can mislead you. The C program below runs on one machine (in the in-browser Linux, run `ip link set lo up` and then `./ops`): it generates a known amount of traffic on the loopback interface so that the numbers can be checked.

{{include vm/ops.c}}

{{vm :: ip link set lo up ;; ./ops}}

## Exercises

**1. From counters to rates.** The program sent 1000 UDP datagrams of 1000 bytes of payload. What did the interface counters say, why is a single reading useless, and how do you compute a rate? Why are about 1028 bytes counted per packet and not 1000 or 1042? Which counters would you alert on?

**2. Counter wrap.** A 32-bit byte counter read 4,294,967,000 and then 500. How many bytes were transferred? How often does such a counter wrap on a 1 Gbit/s link, and what is the practical consequence for SNMP polling?

**3. NTP arithmetic.** A client sends a request at T1 = 100.000, the server receives it at T2 = 100.130, replies at T3 = 100.131, and the client receives the reply at T4 = 100.061. Compute the client's clock offset and the round-trip delay, and the error bound.

**4. Decode syslog.** Decode the syslog priority values `<13>`, `<34>`, `<165>` and `<190>`. Which would you page on?

## Answers

Try before opening.

<details>
<summary>Show the explanations</summary>

**1.** Interface counters are **cumulative since boot** (or since the interface was created), so one reading says nothing about the *present*; you must **poll twice and divide the difference by the time between the polls**: rate = (counter₂ − counter₁) / (t₂ − t₁). Here the interface moved 1,000 packets in the measured interval, which gives the average rate the program prints (it differs from machine to machine and from run to run; the figure includes the 0.5 s sleep, so it understates the rate during the burst: the *averaging window* always hides peaks, which is why monitoring systems keep short intervals for bursty links). The counters showed **about 1,028 bytes per packet** (a little less when other small packets crossed the loopback during the test, which pulls the average down): 1000 bytes of payload plus the 8-byte UDP header and 20-byte IP header; Linux counts the **IP packet** (the byte counters in `/proc/net/dev` and `ip -s link` leave out the Ethernet header and trailer, while some switches and SNMP agents include them, so numbers from different devices can disagree by 2 to 4%). Packets were counted once in each direction (sent and received on the same loopback interface). The counters to alert on are **errors** (CRC, framing: a bad cable, a duplex mismatch, a failing optic), **drops** (queues overflowing or the host unable to keep up) and **utilisation relative to the link speed**: sustained 80% or more means saturation is near, and the *rate of change* of errors matters more than the absolute value (a total of 40 errors since boot is nothing; 40 errors per minute is a fault).

**2.** The correct difference is **(new − old) modulo 2³²**, which gives **796 bytes**; the naive subtraction gives a huge negative number that many scripts turn into a nonsense spike or silently drop. A 32-bit counter holds 4.29 GB, so at **1 Gbit/s (125 MB/s) it wraps every 34.4 seconds**: at 10 Gbit/s about every 3.4 seconds. If you poll less often than the wrap interval you cannot even know *whether* the counter wrapped once or several times, and the result is silently wrong. The fix is to use **64-bit counters** (SNMP `ifHCInOctets` and `ifHCOutOctets` from the high-capacity IF-MIB, which any 1 Gbit/s or faster interface needs) or to poll frequently enough. Also handle **counter resets** (a device reboot sets the counters back to zero: a new value *smaller* than the old by a huge margin and unrelated to wrapping), by discarding that sample and rate-limiting the alarms.

**3.** The standard NTP formulas (RFC 5905) use the four timestamps:

- **offset = ((T2 − T1) + (T3 − T4)) / 2 = ((0.130) + (0.070)) / 2 = 0.100 s = 100 ms**: the client clock is **behind** the server by 100 ms (so the client should step or slew its clock forward).
- **round-trip delay = (T4 − T1) − (T3 − T2) = 0.061 − 0.001 = 0.060 s = 60 ms**: the total time spent on the network, excluding the 1 ms the server spent processing.

The offset assumes the path delay is **symmetric** (equal outbound and return time), which cannot be known: if all 60 ms of delay were on the outbound leg the true offset would be 30 ms smaller, and if all on the return leg 30 ms larger. So the offset is only trustworthy to **± delay/2 = ± 30 ms**, and this is why NTP prefers servers (and samples) with the *smallest delay*, takes many samples, and why a long asymmetric path (a satellite link, a congested uplink in one direction) limits accuracy. PTP exists to remove this limit using hardware timestamps and boundary clocks in the switches (*NTP, PTP and Time Synchronization*). Time matters beyond display: wrong clocks break TLS certificate validity, Kerberos (a 5-minute tolerance), log correlation and database replication.

**4.** The syslog priority value is **facility × 8 + severity**, so divide by 8: the quotient is the facility and the remainder the severity. `<13>` = 1 × 8 + 5: facility 1 (user), severity 5 (notice). `<34>` = 4 × 8 + 2: facility 4 (auth), severity **2 (critical)**. `<165>` = 20 × 8 + 5: facility 20 (local4), severity 5 (notice), the example in RFC 5424. `<190>` = 23 × 8 + 6: facility 23 (local7), severity 6 (info). The severities run `0 emergency, 1 alert, 2 critical, 3 error, 4 warning, 5 notice, 6 informational, 7 debug`, and **lower is worse**. Page on `<34>`: a *critical* event in the authentication facility. The others are routine. Rules of thumb for alerting: alert on severity (usually 0 to 3, plus selected warnings) rather than on keywords, route logs by facility to different destinations, keep the *facility local0 to local7* for your own devices and applications, and remember that the syslog protocol over UDP is **unreliable and unauthenticated**: use TCP with TLS (RFC 5425) or a modern collector when you need guaranteed delivery, and sync clocks (exercise 3) so that log lines from different devices can be put in order.

</details>

## Key takeaways

1. Counters are cumulative: poll twice, subtract and divide by the elapsed time; short intervals show peaks, and error and drop rates matter more than totals.
2. Always compute counter deltas modulo the counter width and use 64-bit counters on fast links; a 32-bit byte counter wraps in 34 seconds at 1 Gbit/s.
3. NTP offset is ((T2 − T1) + (T3 − T4)) / 2 and delay is (T4 − T1) − (T3 − T2); the offset is only accurate to half the delay because path symmetry is assumed.
4. Syslog priority is facility × 8 + severity; lower severity numbers are more serious, and UDP syslog can lose messages.
