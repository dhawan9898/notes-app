---
title: A Firewall and Traffic Control Lab
track: linux
module: Network Stack
moduleOrder: 10
order: 1
level: expert
minutes: 40
env: machine
summary: "Four exercises that run on one machine in a throwaway network namespace: dropping ICMP with an nftables rule while TCP keeps working, reading rule counters, adding delay with tc netem, and simulating packet loss, with captured output and an explanation of where each mechanism sits in the packet path."
prerequisites:
  - "The Linux Packet Path: NIC, NAPI, Softirqs, Netfilter and Qdisc"
  - "Sockets, TCP Behavior and Linux Network Tuning"
  - "Linux Network Labs: Namespaces, veth, Bridges, VLANs, tc and nftables"
sources:
  - label: "nftables wiki — Quick reference and rule syntax"
    url: https://wiki.nftables.org/wiki-nftables/index.php/Quick_reference-nftables_in_10_minutes
  - label: "man 8 tc, man 8 tc-netem, man 8 tc-tbf"
    url: https://man7.org/linux/man-pages/man8/tc-netem.8.html
  - label: "Linux Foundation — netem"
    url: https://wiki.linuxfoundation.org/networking/netem
---

Firewalls and traffic shaping are the two things you can *do* to packets in the Linux stack, and both are easy to experiment with because a network namespace is a free, disposable machine. The lab connects a client namespace `c` (`10.9.0.1`) to a server namespace `s` (`10.9.0.2`) with a virtual cable; nothing touches your real network. Setup (the same technique as *Practice: A Static Routing Lab*):

```bash
unshare -Urnm bash
mount -t tmpfs tmpfs /run && ip link set lo up
ip netns add c; ip netns add s
ip link add vc netns c type veth peer name vs netns s
ip -n c addr add 10.9.0.1/24 dev vc; ip -n s addr add 10.9.0.2/24 dev vs
ip -n c link set vc up; ip -n s link set vs up
x() { ip netns exec "$@"; }          # shorthand: run a command inside a namespace
```

## Exercises

**1. Block ping, keep the web.** On `s`, create an nftables table and an `input` chain (hook `input`, policy `accept`) and add one rule that **drops ICMP echo requests** and counts them. Ping `s` from `c` twice, then show the ruleset. Does a TCP connection to a listener on port 8080 still work?

**2. Read the counters.** What do the numbers in `counter packets 2 bytes 168` mean, and why is each ping 84 bytes?

**3. Add latency.** Remove the firewall rules and add an 80 ms delay to `c`'s interface with `tc qdisc ... netem`. What round-trip time do you expect, and why does it differ from what a naive reading suggests?

**4. Simulate a lossy link.** Replace the qdisc with `netem loss 30%` and send 300 pings. How much loss do you expect to see, and how does that compare with the measured figure? Why does the loss on one direction only look like 30% and not 51%?

## Answers

Try before opening. The output below is from one real run (loss and delay values are random and differ slightly each time).

<details>
<summary>Show answers and captured output</summary>

**1.**

```text
$ x s nft add table inet lab
$ x s nft add chain inet lab input '{ type filter hook input priority 0 ; policy accept ; }'
$ x s nft add rule inet lab input icmp type echo-request counter drop
$ x c ping -c 2 -W 1 10.9.0.2 | grep transmitted
2 packets transmitted, 0 received, 100% packet loss, time 1009ms
$ x s nft list ruleset
table inet lab {
	chain input {
		type filter hook input priority filter; policy accept;
		icmp type echo-request counter packets 2 bytes 168 drop
	}
}
$ (x s sh -c 'echo hello-from-server | nc -l -p 8080 -q1' &) ; x c sh -c 'nc -w 2 10.9.0.2 8080'
hello-from-server
```

A **table** groups chains, a **chain** attached to a **hook** (`input`: packets addressed to this host, after routing) is where rules run, and the **policy** is what happens to packets that no rule decided (`accept`). The one rule matches only ICMP *echo requests* and drops them, so pings time out while everything else, including the TCP connection, is untouched. Dropping means the sender gets **silence**, not an error; `reject` would send back an ICMP "unreachable" or TCP reset, which is friendlier to legitimate clients and faster to fail. The `inet` family handles IPv4 and IPv6 in one table. In production, set the input policy to `drop` and add explicit `accept` rules (loopback, established connections via `ct state established,related accept`, SSH, your services), never the other way around, and test on the console before closing your SSH session.

**2.** `counter packets 2 bytes 168` shows the rule **matched 2 packets totalling 168 bytes**, i.e. **84 bytes each**: 20 bytes of IPv4 header, 8 bytes of ICMP header and 56 bytes of ping payload (the "56(84) bytes of data" `ping` announces). Counters are the quickest way to confirm that traffic reaches a rule at all: if the counter stays at 0 the packets are being dropped or handled elsewhere earlier (a routing problem, another chain, a different interface), which is more useful than reading the rule again. Reset them with `nft reset counters` or watch them rise while testing.

**3.**

```text
$ x c tc qdisc add dev vc root netem delay 80ms
$ x c ping -c 3 -i 0.2 -W 2 10.9.0.2 | grep rtt
rtt min/avg/max/mdev = 80.066/106.767/160.144/37.742 ms
$ x c tc qdisc show dev vc
qdisc netem 804d: root refcnt 9 limit 1000 delay 80ms seed ...
```

`tc` attaches a **qdisc** (queueing discipline) to an interface's *transmit* side; `netem` is the network-emulation qdisc that delays, drops, reorders or corrupts packets. The delay applies only to packets **leaving** `c`, so the ping request is delayed by 80 ms but the reply (leaving `s`) is not: the round trip is **about 80 ms**, not 160 ms (the minimum measured was 80.07 ms). The first sample is also slower because the very first packet triggers ARP resolution, which is delayed too, and the average here (107 ms) is pulled up by it. To delay both directions, add the qdisc on both sides. Use netem to test how your application behaves on a slow link (a mobile network or a transatlantic path) *before* users find out.

**4.**

```text
$ x c tc qdisc replace dev vc root netem loss 30%
$ x c ping -c 300 -i 0.01 -W 1 10.9.0.2 | grep transmitted | sed 's/, time.*//'
300 packets transmitted, 217 received, 27.6667% packet loss
```

`loss 30%` discards each outgoing packet on `c` with probability 0.30. With 300 pings the measured **27.7%** is close to 30% (random variation of a few percent is normal; with only 20 pings the result can be far off, which is why a loss test needs hundreds of samples). Loss is seen as 30% rather than "51% for a round trip" because the qdisc sits only on `c`'s egress, so only the *requests* can be lost; the replies travel back untouched. If you impaired both directions, a ping would fail with probability 1 − 0.7 × 0.7 = **51%**. This is a useful sanity check on any loss measurement: the loss you see end to end is the combination of both directions. TCP reacts to such loss by retransmitting and cutting its sending rate sharply, so even 1% loss can slash throughput on long paths (see *Sockets, TCP Behavior and Linux Network Tuning*); real shaping (rate limits with `tbf`, fair queueing with `fq_codel`) uses the same `tc qdisc` mechanism.

</details>

## Where these two sit in the packet path

An incoming packet goes through the NIC, the driver and routing; **netfilter hooks** (`prerouting`, `input`, `forward`, `output`, `postrouting`) run nftables rules at fixed points; on the way out the packet reaches the interface's **qdisc**, which decides when and whether it is transmitted. So *firewall rules decide what is allowed* and *qdiscs decide how fast and in what order*: filtering happens in the netfilter hooks and shaping on egress (ingress shaping is done by policing or by redirecting to an `ifb` device). The path is described step by step in *The Linux Packet Path: NIC, NAPI, Softirqs, Netfilter and Qdisc*.

## Key takeaways

1. An nftables rule lives in a chain attached to a hook; use counters to prove traffic reaches it, and prefer a default-drop policy with explicit accepts (including established connections).
2. `drop` is silent and `reject` answers; ICMP echo can be blocked without touching TCP.
3. `tc qdisc ... netem` impairs only the egress direction of the interface it is attached to, so delay and loss measured end to end combine both directions.
4. Loss tests need hundreds of packets, and even small loss rates hurt TCP throughput badly.
