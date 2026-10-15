---
title: "Network Monitoring: SNMP, Flow Export and Streaming Telemetry"
track: networking
module: Operations and Automation
moduleOrder: 11
order: 1
level: intermediate
summary: "How networks are measured: SNMP (OIDs, PDUs, v2c versus v3 security) with a live agent and manager, counter arithmetic and the 32-bit wrap, agent caching and averaging pitfalls, flow export (NetFlow, IPFIX, sFlow) with a sampling-accuracy simulation, syslog and streaming telemetry (gNMI, YANG), and how to choose."
prerequisites:
  - What Is a Network?
  - "Packet Capture and Analysis"
sources:
  - label: "RFC 3411-3418 — SNMPv3 architecture, message processing, USM (RFC 3414), VACM (RFC 3415), protocol operations (RFC 3416), MIB-II (RFC 3418)"
    url: https://www.rfc-editor.org/rfc/rfc3411
  - label: "RFC 2863 — The Interfaces Group MIB (ifTable, ifXTable, ifHCInOctets)"
    url: https://www.rfc-editor.org/rfc/rfc2863
  - label: "RFC 7011 — Specification of the IP Flow Information Export (IPFIX) Protocol"
    url: https://www.rfc-editor.org/rfc/rfc7011
  - label: "RFC 3954 — Cisco Systems NetFlow Services Export Version 9"
    url: https://www.rfc-editor.org/rfc/rfc3954
  - label: "RFC 3176 — InMon Corporation's sFlow"
    url: https://www.rfc-editor.org/rfc/rfc3176
  - label: "RFC 5424 — The Syslog Protocol"
    url: https://www.rfc-editor.org/rfc/rfc5424
  - label: "RFC 6020 / 7950 — YANG; RFC 8040 RESTCONF; RFC 6241 NETCONF"
    url: https://www.rfc-editor.org/rfc/rfc7950
  - label: "gNMI specification (OpenConfig) and gRPC"
    url: https://github.com/openconfig/reference/blob/master/rpc/gnmi/gnmi-specification.md
  - label: "snmpd.conf(5), snmpget(1), snmpwalk(1) — Net-SNMP manual pages"
    url: http://www.net-snmp.org/docs/man/
---

You cannot manage what you cannot see. Network monitoring answers four kinds of questions: **Is it up?** (availability), **How much is flowing?** (utilisation and errors), **Who and what is using it?** (traffic composition), and **What just happened?** (events). Different protocols are good at different questions:

| Question | Typical tool | Data model |
|---|---|---|
| Is the device alive; how loaded are its interfaces, CPUs, sensors? | **SNMP polling**, streaming telemetry | counters and gauges, polled or streamed |
| Which conversations use the bandwidth? | **NetFlow / IPFIX / sFlow** | per-flow records |
| What happened and when? | **syslog**, SNMP **traps**, telemetry events | event messages |
| What exactly was on the wire? | **packet capture** | raw packets |
| Does the path work end to end? | probes: ping, traceroute, active tests | synthetic measurements |

## SNMP

The **Simple Network Management Protocol** (SNMP) is the oldest and still the most widely supported way to read device state. A **manager** (the monitoring system) talks to **agents** (the software in each router, switch, server, printer) over UDP port 161 (traps go to 162).

### MIBs and OIDs

Everything an agent exposes is a leaf in a global tree, identified by an **object identifier (OID)**, a dotted sequence of numbers. A **MIB** (Management Information Base) is a file describing a part of the tree with human-readable names. Examples (all standard, from MIB-II and the interfaces MIB):

| OID | Name | Type | Meaning |
|---|---|---|---|
| `1.3.6.1.2.1.1.1.0` | `sysDescr.0` | string | what the device is |
| `1.3.6.1.2.1.1.3.0` | `sysUpTime.0` | TimeTicks (1/100 s) | time since the agent restarted |
| `1.3.6.1.2.1.1.5.0` | `sysName.0` | string | hostname |
| `1.3.6.1.2.1.2.2.1.2.N` | `ifDescr.N` | string | name of interface number N |
| `1.3.6.1.2.1.2.2.1.10.N` | `ifInOctets.N` | **Counter32** | bytes received (32-bit) |
| `1.3.6.1.2.1.31.1.1.1.6.N` | `ifHCInOctets.N` | **Counter64** | bytes received (64-bit) |
| `1.3.6.1.2.1.2.2.1.14.N` | `ifInErrors.N` | Counter32 | receive errors |

The trailing `.0` marks a *scalar*; a trailing index (`.N`) selects a row of a *table*. (This system has no MIB files installed, so tools show numeric OIDs.)

### Operations and versions

| PDU | Use |
|---|---|
| **GET** | read specific OIDs |
| **GETNEXT / GETBULK** | walk a subtree (GETBULK fetches many rows per request: much faster, v2c and v3) |
| **SET** | change a value (configuration; rarely used because of the security risk) |
| **TRAP / INFORM** | the agent notifies the manager of an event (link down, fan failure); INFORM is acknowledged |

| Version | Security | Notes |
|---|---|---|
| **v1** | community string in clear text | obsolete (32-bit counters only) |
| **v2c** | community string in clear text | still the most common; a "password" anyone on the path can read |
| **v3** | **USM**: usernames, authentication (HMAC-SHA), optional encryption (AES), and **VACM** access control per view | the only acceptable choice off a trusted management network |

### A live agent and manager

The lab starts `snmpd` (the Net-SNMP agent) in one network namespace and uses the standard `snmpget`/`snmpwalk` tools and a small Python poller from another, joined by a virtual cable. The agent allows the v2c community `demo` from the manager's address only, and a v3 user with SHA authentication and AES privacy.

```text
agentaddress udp:10.0.0.2:161
sysdescr Demo router (lab)
sysname demo-router
syslocation Lab rack 1
syscontact noc@example.test
rocommunity demo 10.0.0.1
createUser nmsuser SHA authpass123 AES privpass123
rouser nmsuser priv
```

```python
#!/usr/bin/env python3
"""A minimal SNMP poller: reads interface counters from an agent twice, with traffic in between, and computes the rate the way monitoring systems do."""
import socket, subprocess, time

AGENT = "10.0.0.2"
def get(oid, v3=False):
    cmd = ["snmpget", "-Oqv", "-On"] + (["-v3", "-l", "authPriv", "-u", "nmsuser", "-a", "SHA", "-A", "authpass123", "-x", "AES", "-X", "privpass123"] if v3 else ["-v2c", "-c", "demo"]) + [AGENT, oid]
    return subprocess.run(cmd, capture_output=True, text=True).stdout.strip()
def walk(oid):
    out = subprocess.run(["snmpwalk", "-Oqn", "-v2c", "-c", "demo", AGENT, oid], capture_output=True, text=True).stdout.strip().splitlines()
    return [(l.split()[0], l.split(None, 1)[1].strip('"')) for l in out]

print("sysDescr.0  =", get("1.3.6.1.2.1.1.1.0")); print("sysName.0   =", get("1.3.6.1.2.1.1.5.0")); print("sysUpTime.0 =", get("1.3.6.1.2.1.1.3.0"))
idx = [o.rsplit(".", 1)[1] for o, v in walk("1.3.6.1.2.1.2.2.1.2") if v == "va"][0]
print(f"interface 'va' has ifIndex {idx}")
IN32, IN64 = f"1.3.6.1.2.1.2.2.1.10.{idx}", f"1.3.6.1.2.1.31.1.1.1.6.{idx}"          # ifInOctets (Counter32), ifHCInOctets (Counter64)
a32, a64, t0 = int(get(IN32)), int(get(IN64)), time.time()
s = socket.create_connection((AGENT, 9000)); sent = 0
while sent < 40_000_000: s.sendall(b"x" * 65536); sent += 65536                          # push about 40 MB toward the agent host as fast as possible
s.close()
early = int(get(IN64))                                                                    # poll immediately: the agent has not refreshed its interface cache yet
time.sleep(7)                                                                             # net-snmp refreshes its interface table every few seconds
b32, b64, t1 = int(get(IN32)), int(get(IN64)), time.time()
print(f"\nthe poller pushed {sent} payload bytes to the agent host in well under a second (a burst of several Gbit/s on this virtual cable)")
print(f"a poll immediately after the burst returned a delta of {early - a64} bytes: the agent serves counters from a cache that is only refreshed every few seconds")
print(f"ifHCInOctets delta = {b64 - a64} bytes over the {t1 - t0:.1f} s polling interval  ->  {(b64 - a64) * 8 / (t1 - t0) / 1e6:.0f} Mbit/s AVERAGE (the counter includes Ethernet and IP headers, so a little more than the payload)")
print(f"ifInOctets   delta = {(b32 - a32) % 2 ** 32} bytes (32-bit counter, taken modulo 2^32)")
print("\n32-bit counter wrap: a Counter32 wraps after 2^32 bytes = 4.29 GB; at 1 Gbit/s that takes", round(2 ** 32 * 8 / 1e9, 1), "s; at 10 Gbit/s", round(2 ** 32 * 8 / 10e9, 1), "s.")
print("Poll more slowly than that and a delta is ambiguous (it may have wrapped several times): use the 64-bit ifHC* counters.")
print("\nSNMPv3 authPriv read of the same 64-bit counter:", get(IN64, v3=True))
```

```bash
#!/bin/sh
# An SNMP agent (snmpd) in one namespace polled by a manager in another, over a virtual cable. Needs net-snmp (snmpd, snmpget, snmpwalk).
mount -t tmpfs tmpfs /run; mkdir -p /run/netns /tmp/snmp-lab
ip netns add nms; ip netns add agent; ip link add vn type veth peer name va; ip link set vn netns nms; ip link set va netns agent
ip -n nms addr add 10.0.0.1/24 dev vn; ip -n agent addr add 10.0.0.2/24 dev va
for x in "nms vn" "agent va"; do set -- $x; ip -n $1 link set $2 up; ip -n $1 link set lo up; done
SNMP_PERSISTENT_DIR=/tmp/snmp-lab ip netns exec agent snmpd -f -C -c snmpd.conf -Lf /tmp/snmp-lab/log -p /tmp/snmp-lab/pid > /dev/null 2>&1 &
SP=$!
ip netns exec agent python3 -c '
import socket
l = socket.socket(); l.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); l.bind(("10.0.0.2", 9000)); l.listen(1)
c, _ = l.accept()
while c.recv(1 << 20): pass' &
sleep 2
echo "== a v2c get with the community string, and a v3 get with a wrong password =="
ip netns exec nms snmpget -v2c -c demo -On 10.0.0.2 1.3.6.1.2.1.1.5.0
ip netns exec nms snmpget -v3 -l authNoPriv -u nmsuser -a SHA -A wrongpassword 10.0.0.2 1.3.6.1.2.1.1.5.0 2>&1 | head -1
ip netns exec nms snmpget -v2c -c notthecommunity -t 1 -r 0 10.0.0.2 1.3.6.1.2.1.1.5.0 2>&1 | head -1
echo; echo "== poll interface counters =="
ip netns exec nms python3 poll.py
echo; echo "== cross-check against the kernel's own counter on the agent (ip -s link) =="
ip -n agent -s link show va | sed -n 3,4p
kill $SP 2>/dev/null; wait 2>/dev/null
```

```text
$ unshare -Urnm sh lab.sh 2>&1
== a v2c get with the community string, and a v3 get with a wrong password ==
.1.3.6.1.2.1.1.5.0 = STRING: "demo-router"
snmpget: Authentication failure (incorrect password, community or key)
Timeout: No Response from 10.0.0.2.

== poll interface counters ==
sysDescr.0  = "Demo router (lab)"
sysName.0   = "demo-router"
sysUpTime.0 = 0:0:00:03.13
interface 'va' has ifIndex 7

the poller pushed 40042496 payload bytes to the agent host in well under a second (a burst of several Gbit/s on this virtual cable)
a poll immediately after the burst returned a delta of 0 bytes: the agent serves counters from a cache that is only refreshed every few seconds
ifHCInOctets delta = 40116491 bytes over the 7.1 s polling interval  ->  45 Mbit/s AVERAGE (the counter includes Ethernet and IP headers, so a little more than the payload)
ifInOctets   delta = 40116491 bytes (32-bit counter, taken modulo 2^32)

32-bit counter wrap: a Counter32 wraps after 2^32 bytes = 4.29 GB; at 1 Gbit/s that takes 34.4 s; at 10 Gbit/s 3.4 s.
Poll more slowly than that and a delta is ambiguous (it may have wrapped several times): use the 64-bit ifHC* counters.

SNMPv3 authPriv read of the same 64-bit counter: 40117506

== cross-check against the kernel's own counter on the agent (ip -s link) ==
    RX:  bytes packets errors dropped  missed   mcast           
      40117970    1132      0       0       0       0 
```

What happened:

- **Authentication.** The v2c GET with the right community succeeds. A v3 request with the wrong password fails with *Authentication failure*, and a v2c request with a wrong community gets *no response at all*: SNMP agents silently ignore bad communities, so "timeout" can mean "wrong credentials". With v3 the key is derived from the password and never sent, and with authPriv the payload is encrypted.
- **Identity and uptime.** `sysDescr`, `sysName`, `sysUpTime` (in hundredths of a second) are the first things any poller reads; a falling `sysUpTime` between polls reveals a reboot even if the device was never "down" in your monitoring.
- **Interface index.** Interfaces are identified by `ifIndex`, a number the agent assigns (here 7 for `va`); the poller finds it by walking `ifDescr`. Indexes can change after a reboot on some platforms, so robust systems re-map by name or alias.
- **Counters accumulate; the rate is a difference.** `ifHCInOctets` only ever grows. To get bits per second, poll twice and compute `(delta × 8) / seconds`. The delta here was about 40 MB, matching the 40 MB the poller sent plus header overhead, and it agrees with the kernel's own receive-byte counter (`ip -s link`, last lines) to within the traffic of the SNMP requests themselves.
- **The 32-bit counter wraps.** A `Counter32` counts modulo 2³², about 4.29 GB. At 1 Gbit/s that is every 34 seconds; at 10 Gbit/s every 3.4 s. If a poll interval contains more than one wrap, the arithmetic silently gives a wrong (too small) value, which is why every modern interface uses the 64-bit `ifHC*` counters (which wrap in centuries). The poller prints the 32-bit and 64-bit deltas side by side; they agree only because fewer than 4.29 GB moved.
- **Agent caching.** The poll made immediately after the burst returned a delta of **zero**: the agent keeps interface statistics in a cache and refreshes it only every few seconds. A poller that asks more often than the agent updates sees flat lines followed by spikes. Match the polling interval to the agent's update granularity.
- **Averages hide bursts.** The 40 MB moved in a fraction of a second, but the 7-second polling interval turns it into an average of about 45 Mbit/s. SNMP at 1 to 5 minute intervals cannot see microbursts that overflow buffers and cause drops while the link graph shows 10 % utilisation. Look at **drop/discard counters** (`ifOutDiscards`, queue statistics) and finer-grained telemetry for that.

### Practical SNMP rules

- Use **v3 authPriv** (or at least restrict v2c by source address and use a long random community) and never expose SNMP to the Internet: it is a classic reflection and information-leak target.
- Poll **bulk** (`snmpbulkwalk`) and in **parallel** with sensible intervals; SNMP is processed by the device's CPU, and aggressive polling can hurt a router's control plane.
- Compute rates from **64-bit counters** and handle **counter resets** (a counter that goes *down* means the agent restarted: discard that sample).
- Track `sysUpTime` and `ifLastChange` for restarts and flaps; use **traps/informs** for fast notification and polling for trends.
- Prefer the standard MIBs (`IF-MIB`, `ENTITY-MIB`, `BGP4-MIB`, `OSPF-MIB`); vendor MIBs cover CPU, memory, temperature and chassis details.

## Flow export: NetFlow, IPFIX and sFlow

SNMP counters say *how much* passes an interface; they cannot say *who* sent it. **Flow export** can. A router or switch groups packets into **flows**, usually by the 5-tuple (source and destination address, ports, protocol) plus input interface and often ToS, and exports a **flow record** per flow with the first/last timestamp and the packet and byte counts, TCP flags, next hop, AS numbers, and so on, to a **collector**. This is the data behind "top talkers", capacity planning, DDoS detection and billing.

| Technology | Mechanism | Notes |
|---|---|---|
| **NetFlow v5/v9** (Cisco) | flow cache on the device; template-based in v9 | the original; v5 fixed IPv4 format |
| **IPFIX** (RFC 7011) | the IETF standard, based on NetFlow v9; templates describe the fields | vendor-neutral; supports IPv6, MPLS, custom fields |
| **sFlow** (RFC 3176) | **packet sampling**: the device forwards a sample of packet headers plus interface counters | no flow cache, scales to very high speeds |
| **Linux** | `nfacct`, `softflowd`, `pmacct`, `ntopng`, Open vSwitch, eBPF exporters | build flow records from the kernel or packet capture |

Records are produced when a flow ends or after an **active timeout** (long flows are reported in slices, typically every 1 to 5 minutes) and an **inactive timeout** (a flow with no packets for 15 to 60 s is closed). At high speed most devices do **sampling**: examine only one packet in *N* and scale the counts up by *N*.

### How good is sampling? A simulation

Network traffic is heavy-tailed: a few "elephant" flows carry most of the bytes while most flows are "mice" of a packet or two. The simulation generates 100,000 flows with Pareto-distributed sizes and measures what random 1-in-*N* packet sampling reveals:

```python
#!/usr/bin/env python3
"""Simulation: how accurate are sampled flow records (NetFlow/IPFIX/sFlow packet sampling)?  Flow sizes follow a heavy-tailed (Pareto) distribution,
as real traffic does: a few elephant flows carry most bytes, many mice carry little."""
import random
rnd = random.Random(7)
NFLOWS = 100_000
flows = [max(1, int(rnd.paretovariate(1.1))) for _ in range(NFLOWS)]            # packets per flow
PKT = 800                                                                       # bytes per packet, to keep the arithmetic simple
total_pkts = sum(flows); order = sorted(range(NFLOWS), key=lambda i: -flows[i])
print(f"{NFLOWS} flows, {total_pkts} packets. Top 1% of flows carry {100 * sum(flows[i] for i in order[:NFLOWS // 100]) / total_pkts:.0f}% of the packets; the median flow has {sorted(flows)[NFLOWS // 2]} packet(s).\n")
print(f"{'sampling':>9} {'flows seen':>12} {'total est. error':>17} {'top-10 flows: mean error':>26} {'flows with <10 packets seen':>29}")
for N in (1, 10, 100, 1000):
    seen = [0] * NFLOWS
    for i, n in enumerate(flows):                                               # random 1-in-N packet sampling (each packet independently with probability 1/N)
        if N == 1: seen[i] = n
        else: seen[i] = sum(1 for _ in range(n) if rnd.random() < 1 / N) if n < 200 else int(rnd.gauss(n / N, (n / N) ** 0.5))
    est = [s * N for s in seen]
    top_err = sum(abs(est[i] - flows[i]) / flows[i] for i in order[:10]) / 10
    small = [i for i in range(NFLOWS) if flows[i] < 10]; small_seen = sum(1 for i in small if seen[i] > 0)
    print(f"{'1:' + str(N):>9} {sum(1 for s in seen if s) / NFLOWS * 100:>11.1f}% {abs(sum(est) - total_pkts) / total_pkts * 100:>16.1f}% {top_err * 100:>25.1f}% {small_seen / len(small) * 100:>28.1f}%")
```

```text
$ python3 flowsample.py
100000 flows, 686673 packets. Top 1% of flows carry 52% of the packets; the median flow has 1 packet(s).

 sampling   flows seen  total est. error   top-10 flows: mean error   flows with <10 packets seen
      1:1       100.0%              0.0%                       0.0%                        100.0%
     1:10        23.5%              0.2%                       2.1%                         18.2%
    1:100         3.8%              1.3%                      10.1%                          2.0%
   1:1000         0.5%             10.0%                      19.1%                          0.2%
```

- The **total traffic estimate** is excellent at moderate sampling (errors of a percent or so at 1:100) because it aggregates millions of samples.
- **Large flows are measured well, small ones are invisible.** At 1:100 only a few percent of the flows are seen at all, and almost no flows of fewer than 10 packets: sampled data is the right tool for finding elephants and top talkers and the wrong tool for counting connections or catching a small attack that uses many tiny flows.
- Accuracy for the biggest flows still degrades as the sampling rate falls (errors of tens of percent at 1:1000 for the heaviest flows here), so choose *N* from the link speed and the smallest flow you care about, not from habit.
- **Unsampled flow export** (1:1) is exact but needs a flow cache and CPU; it is common on software and modern ASIC platforms for edge links.

(This is a simulation with synthetic flows, not a measurement of a real network; its purpose is to show the shape of the accuracy trade-off.)

## Syslog and events

**Syslog** (RFC 5424) carries log messages from devices to a collector over UDP/514 or, better, TCP/TLS: each message has a **facility**, a **severity** (0 emergency to 7 debug), a timestamp, a hostname, and text. Link up/down, routing adjacency changes, authentication failures and configuration commits arrive as syslog; the practical problems are loss over UDP, clock differences between devices (everything depends on correct time: see *NTP and Time Synchronization*), message formats that vary by vendor, and volume. Central log systems parse, index and alert on them.

## Streaming telemetry

Polling has fundamental limits: it is request/response, per-device CPU-heavy, coarse in time, and the data models are inconsistent. **Streaming telemetry** inverts it: the device *pushes* selected data to a collector at a configured interval or whenever it changes, over a modern transport.

- **gNMI** (gRPC Network Management Interface) uses gRPC over HTTP/2 with TLS and Protobuf encoding. A client **subscribes** to paths: `STREAM` mode with `SAMPLE` (every N seconds, down to 1 s or less) or `ON_CHANGE` (only when the value changes, ideal for interface state and BGP session state), or `ONCE`/`POLL`. Efficient binary encoding and a single long-lived connection replace thousands of SNMP requests.
- **YANG data models** describe the structure, and **OpenConfig** provides vendor-neutral models (`/interfaces/interface[name=eth0]/state/counters/in-octets`), so the same query works across vendors. The same models are used for configuration over NETCONF, RESTCONF and gNMI Set (see the *Network Automation: Declarative State, NETCONF/YANG, gNMI and Safe Changes* note).
- **Counters are 64-bit** and timestamps come from the device, which solves the wrap and jitter problems.
- Transport, models and exporter support vary: many deployments still use SNMP for legacy devices and gNMI for newer ones, with an adapter (such as an SNMP exporter feeding a time-series database like Prometheus) in between.

## Choosing and combining

| Need | Use |
|---|---|
| Up/down and basic utilisation for everything, including old gear | SNMP polling (v3) with 64-bit counters |
| Fast, fine-grained, on-change interface and protocol state | streaming telemetry (gNMI) |
| Who is using the bandwidth; capacity planning; DDoS detection | IPFIX/NetFlow/sFlow with appropriate sampling |
| What happened at 02:13? | syslog and device event logs, correlated by time |
| Why is this one flow slow? | packet capture at both ends |
| Is the path itself working? | active probes (ping, TCP/HTTP probes, traceroute) from several vantage points |

A healthy monitoring design stores **raw counters**, derives rates in the query layer, alerts on *symptoms users feel* (loss, latency, error rate) as well as on causes, and **monitors the monitoring**: missing data is an alert.

## Failure modes

| Symptom | Likely cause |
|---|---|
| Utilisation graph shows impossible values or drops to zero | counter wrap (32-bit) or reset after a reboot not handled |
| Rate is flat then spikes | polling faster than the agent's cache refresh |
| "Timeout" from SNMP | wrong community or v3 user (agents stay silent), ACL blocking the manager, UDP/161 firewalled, agent overloaded |
| Link looks idle but users complain | microbursts and drops invisible at 5-minute averages |
| Flow totals disagree with SNMP | sampling, flows still in the cache not yet exported, or unobserved traffic (local/control plane) |
| Gaps in telemetry | collector or transport overloaded, device rate-limits exports |
| Alerts storm after an upgrade | `ifIndex` renumbering or changed OIDs |

## Key takeaways

1. SNMP exposes device state as OIDs read by managers; use v3 (authentication and encryption) and bulk operations, and remember that agents silently ignore wrong credentials.
2. Counters only grow: a rate is a delta over time; use 64-bit `ifHC*` counters (a 32-bit counter wraps in 34 s at 1 Gbit/s) and detect resets via `sysUpTime`.
3. Poll no faster than the agent refreshes, and remember that long-interval averages hide microbursts; watch drops.
4. Flow export (IPFIX, NetFlow, sFlow) answers "who": sampling finds elephants well and mice poorly.
5. Syslog carries events; correct time (NTP) is essential to correlate them.
6. Streaming telemetry with YANG/OpenConfig and gNMI pushes fine-grained, on-change data efficiently; most networks run a mix of SNMP and telemetry.
