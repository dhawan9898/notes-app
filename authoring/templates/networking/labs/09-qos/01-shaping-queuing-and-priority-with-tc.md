---
title: Shaping, Queuing and Priority with tc
track: networking
module: QoS
moduleOrder: 9
order: 1
level: advanced
minutes: 45
env: machine
summary: "A lab on one machine that rate-limits a link with a token bucket, then adds an HTB scheduler with a priority class selected by DSCP, and measures what a saturated link does to latency for best-effort and priority traffic: 173 ms against 0.1 ms, with the commands, helper scripts and captured output."
prerequisites:
  - "QoS — DiffServ, Classification, Queuing, Policing and Shaping"
sources:
  - label: "RFC 2474 — Definition of the Differentiated Services Field (DS Field)"
    url: https://www.rfc-editor.org/rfc/rfc2474
  - label: "RFC 3246 — An Expedited Forwarding PHB (Per-Hop Behavior)"
    url: https://www.rfc-editor.org/rfc/rfc3246
  - label: "man 8 tc, tc-htb, tc-tbf, tc-u32"
    url: https://man7.org/linux/man-pages/man8/tc-htb.8.html
  - label: "Gettys, Nichols — Bufferbloat: Dark Buffers in the Internet (ACM Queue, 2011)"
    url: https://queue.acm.org/detail.cfm?id=2071893
---

QoS is often described with diagrams; here you will watch it. The experiment shows the two facts at the centre of the subject: a **saturated link adds delay to everyone** because of queuing, and a **scheduler can give selected traffic a short queue** so that it does not suffer. Everything runs between two virtual hosts in a throwaway namespace (set up as in *Practice: A Firewall and Traffic Control Lab*): client `c` (`10.7.0.1`) and server `s` (`10.7.0.2`).

## Setup

This lab needs **root and Linux network namespaces with traffic control** (`tc`), which the in-browser Linux does not have: run it on your own Linux machine or virtual machine. Compile the two programs with `cc -O2 -o qos_rx qos_rx.c` and `cc -O2 -o qos_tx qos_tx.c` (their listings are under Exercise 1). The setup below is the standard recipe for two namespaces joined by a virtual cable; it is **not** the exact script behind the transcripts, which were captured earlier on a Linux machine with equivalent Python programs, so expect different numbers on your machine.

```bash
ip netns add srv; ip netns add cli
ip link add vc type veth peer name vs
ip link set vc netns cli; ip link set vs netns srv
ip -n cli addr add 10.7.0.1/24 dev vc; ip -n srv addr add 10.7.0.2/24 dev vs
for ns in cli srv; do ip -n $ns link set lo up; done
ip -n cli link set vc up; ip -n srv link set vs up
ip netns exec srv ./qos_rx 10.7.0.2 9300 &        # the receiver waits for one connection
ip netns exec cli ./qos_tx 10.7.0.2 9300          # the sender; add tc rules with: ip netns exec cli tc … dev vc …
```

## Exercise 1: limit the rate with a token bucket

A **token bucket filter** (`tbf`) lets traffic out at a configured average rate: tokens accumulate at the rate, each packet spends tokens, and a packet that finds no tokens waits (or is dropped if its queue is full). The receiver and sender are two tiny C programs (`qos_rx` and `qos_tx`, listed below); the sender writes 2 MiB as fast as it can.

{{include vm/qos_rx.c}}

{{include vm/qos_tx.c}}

Run the transfer once with no shaping, then add `tc qdisc add dev vc root tbf rate 8mbit burst 20kb latency 100ms` on the client's interface and run it again. What do you expect for the second run, what do `burst` and `latency` mean, and what do the qdisc counters show?

```text
--- unshaped
received 2097152 bytes in 0.00 s = 8857.0 Mbit/s
--- tbf rate 8 Mbit/s
received 2097152 bytes in 2.17 s = 7.7 Mbit/s
qdisc tbf 8051: root refcnt 9 rate 8Mbit burst 20Kb lat 100ms
 Sent 2193332 bytes 1456 pkt (dropped 5, overlimits 2813 requeues 0)
```

<details>
<summary>Answer</summary>

Without shaping, the virtual cable is as fast as the CPU (the measured time rounds to 0.00 s because the first chunk was already queued when timing began: ignore the absurd number, the point is "no limit"). With `tbf` the transfer takes **2.17 s for 2 MiB, 7.7 Mbit/s**, just under the configured 8 Mbit/s (the 2.19 MB of sent bytes includes TCP/IP headers, and the receiver's rate counts payload only). **`rate`** is the long-term average; **`burst`** (20 KB) is the bucket size: the amount that may go out *at once* at full speed after an idle period, so a bucket must hold at least one full packet and, for a high rate, at least rate/HZ bytes or the shaper cannot reach its rate; **`latency`** (100 ms) sets how long a packet may wait in the queue, which determines the queue length (here about 100 ms × 8 Mbit/s = 100 KB). The counters: 1,456 packets sent, **5 dropped** because the queue was full (TCP noticed the loss and slowed down: that is how the shaper's rate limit becomes visible to the sender) and 2,813 *overlimits* (times a packet had to wait for tokens). **Shaping delays** packets to keep within a rate (what you do on *egress*); **policing** drops or marks traffic over the rate (what is done on ingress, where you cannot hold packets back).

</details>

## Exercise 2: what a full queue does to latency, and how priority fixes it

Replace the qdisc with an **HTB** (hierarchical token bucket) scheduler that has an 8 Mbit/s root, a high-priority class for voice-like traffic and a default class for everything else, and select traffic by its **DSCP** value (the top six bits of the IP ToS byte; **EF**, "expedited forwarding", is DSCP 46, ToS byte `0xb8`):

```bash
tc qdisc add dev vc root handle 1: htb default 20
tc class add dev vc parent 1:  classid 1:1  htb rate 8mbit
tc class add dev vc parent 1:1 classid 1:10 htb rate 2mbit ceil 8mbit prio 0     # priority class (EF)
tc class add dev vc parent 1:1 classid 1:20 htb rate 6mbit ceil 8mbit prio 1     # everything else
tc qdisc add dev vc parent 1:10 handle 10: pfifo limit 20                        # short queue for priority traffic
tc qdisc add dev vc parent 1:20 handle 20: pfifo limit 200                       # long queue for bulk traffic
tc filter add dev vc parent 1: protocol ip prio 1 u32 match ip tos 0xb8 0xfc flowid 1:10
```

Start a bulk upload that saturates the 8 Mbit/s link for six seconds, and measure `ping` round-trip times (8 pings) on the idle link, then during the transfer with default traffic, then during the transfer with `ping -Q 0xb8` (DSCP EF). Predict the three results.

```text
--- idle link
min 0.0  avg 0.1  max 0.1 ms
--- while a bulk transfer saturates the 8 Mbit/s link
default class (best effort)       min 137.9  avg 173.1  max 204.4 ms
DSCP EF (-Q 0xb8) -> priority class min 0.0  avg 0.1  max 0.1 ms
class htb 1:10 parent 1:1 leaf 10: prio 0 rate 2Mbit ceil 8Mbit burst ...
class htb 1:20 parent 1:1 leaf 20: prio 1 rate 6Mbit ceil 8Mbit burst ...
```

<details>
<summary>Answer</summary>

On the idle link the round trip is a tenth of a millisecond. While a TCP upload saturates the link, **best-effort pings take 138 to 204 ms (average 173 ms)**: TCP keeps filling the queue (200 packets, about 300 KB, which at 8 Mbit/s is 300 ms of waiting), and a ping must wait behind all of it. This is **bufferbloat**: large buffers on a bottleneck turn into large delay, even though no packet is lost, so a video call sharing the link with a download becomes unusable. Pings marked **DSCP EF** go to class `1:10`, which has its own short queue and the **higher priority** (`prio 0`), so the scheduler serves it first: **0.1 ms**, unchanged from the idle link, even though the link is full. The two other numbers in the design: `rate 2mbit` is the **guaranteed** share for the voice class even under contention, and `ceil 8mbit` lets either class **borrow** unused bandwidth up to the link rate, so bulk traffic is not wasted when no voice is present and the guarantee costs nothing when idle.

What to take from it: (1) QoS **does not create bandwidth**, it decides who waits; (2) the delay of best-effort traffic does not go away, so priority is only valuable for small, delay-sensitive flows (voice, interactive, control traffic) and starves everyone if overused, which is why the priority class is **rate-limited by its `rate`/`ceil` and by admission control** in real designs; (3) **marking** (setting DSCP) is meaningless unless every device on the path agrees to honour it: marks are normally *trusted only inside your network* and re-marked at the edge, because the internet generally ignores or clears them; (4) modern Linux defaults to **`fq_codel`**, an *active queue management* scheduler that keeps queues short by dropping packets early and gives each flow its own queue, which cures most bufferbloat with no configuration, and is the first thing to try before building class hierarchies (`tc qdisc replace dev eth0 root fq_codel`). See *QoS — DiffServ, Classification, Queuing, Policing and Shaping* for the PHB definitions and the standard DSCP values (EF 46, AF classes, CS values).

</details>

## Exercise 3: short answers

**a.** Which direction can you shape and which can you only police? **b.** Why is a *priority* class limited by `ceil` rather than allowed to take everything? **c.** What is the difference between shaping and policing for TCP traffic?

<details>
<summary>Answers</summary>

**a.** You can **shape on egress** (transmit): the device owns the queue and can hold packets. On **ingress** the packets have already arrived over the wire, so the device can only **police** (drop or mark traffic that exceeds a rate); to shape incoming traffic you redirect it to an `ifb` virtual device and shape *its* egress, which approximates control but cannot stop the sender from sending. **b.** A strict priority class served first can starve all other traffic if a flow in it sends continuously: a misbehaving or malicious sender marking packets EF could block the link. Limiting its rate (`rate`/`ceil`) and policing the marking at the network edge keeps the guarantee for well-behaved voice flows without allowing a denial of service. **c.** **Shaping** *delays* excess packets in a queue and smooths bursts (TCP sees higher delay and adapts gently); **policing** *drops* the excess immediately (TCP sees loss, retransmits and cuts its rate sharply, which makes policed TCP flows sawtooth and waste capacity). Prefer shaping for TCP when you control the egress queue.

</details>

## Key takeaways

1. A saturated link with a long queue adds hundreds of milliseconds to every flow (bufferbloat); loss is not required for terrible latency.
2. `tbf` enforces an average rate with a bucket (`burst`) and a queue limit (`latency`); excess packets wait or are dropped, and TCP slows to match.
3. HTB with a priority class selected by DSCP gives delay-sensitive traffic its own short queue and the first turn, keeping its latency at idle levels even when the link is full.
4. QoS reorders and prioritises, it does not add bandwidth; marks are only trusted inside your own network, and `fq_codel` is the best first fix for bufferbloat.
5. Shape on egress, police on ingress; shaping is gentler on TCP than policing.
