---
title: IPv6 Addresses and Neighbour Discovery
track: networking
module: IPv6
moduleOrder: 4
order: 1
level: intermediate
minutes: 45
env: browser
terminal:
  - ./ipv6_practice
summary: "Five exercises on the IPv6 module: writing and compressing addresses, classifying address types, deriving an EUI-64 interface ID and a solicited-node multicast address, planning /64 subnets, and watching link-local addressing and neighbour discovery work in a two-host lab, with a verified C program and captured output."
prerequisites:
  - "IPv6 Header, Extension Headers and Addressing"
  - "ICMPv6, Neighbor Discovery, SLAAC and DAD"
sources:
  - label: "RFC 4291 — IP Version 6 Addressing Architecture"
    url: https://www.rfc-editor.org/rfc/rfc4291
  - label: "RFC 4861 — Neighbor Discovery for IP version 6 (IPv6)"
    url: https://www.rfc-editor.org/rfc/rfc4861
  - label: "RFC 5952 — A Recommendation for IPv6 Address Text Representation"
    url: https://www.rfc-editor.org/rfc/rfc5952
  - label: "RFC 3849 — IPv6 Address Prefix Reserved for Documentation"
    url: https://www.rfc-editor.org/rfc/rfc3849
---

IPv6 looks intimidating until you notice it is the same ideas as IPv4 with more bits and less machinery. These exercises train the notation (the hardest part at first) and then show the protocol working. Do the paper exercises, then check them with the program.

## Exercises

**1. Writing addresses.** Compress `2001:0db8:0000:0000:0000:ff00:0042:8329` and `2001:db8:0:0:1:0:0:1`, and expand `::1`. Which rules decide which run of zeros becomes `::`?

**2. What kind of address?** Classify `::1`, `fe80::1`, `fd12:3456:789a::1`, `2001:db8::1`, `ff02::1` and `2606:4700:4700::1111`. Which of them can appear as a destination on the public internet?

**3. EUI-64.** Build the interface ID that SLAAC derives from MAC `00:1a:2b:3c:4d:5e` and the resulting address in `2001:db8:1:2::/64`. Why do modern systems usually avoid this method?

**4. Solicited-node multicast.** To resolve `2001:db8::1a:2b3c:4d5e`, which multicast address does the neighbour solicitation go to, and why is that better than a broadcast?

**5. Subnet planning.** How many `/64` subnets does a `/48` contain, what are the first three, and how many addresses are in one `/64`? Why are `/64`s used even for a link between two routers?

**6. Lab: neighbours without configuration.** Connect two hosts with a virtual cable, change nothing, and show their addresses, ping one from the other, read the neighbour table and ping the all-nodes multicast address. (This part needs network namespaces and virtual Ethernet pairs, which the in-browser Linux does not have: do it on your own Linux machine or virtual machine; the transcript below was captured on one.)

## Answers

Try before opening.

<details>
<summary>Show the programs and captured output</summary>

**1 to 5.** One C program computes them all (run it with `./ipv6_practice` in the Linux terminal):

{{include vm/ipv6_practice.c}}

{{vm :: ./ipv6_practice}}

**1.** Leading zeros of each 16-bit group may be dropped (`0042` becomes `42`), and **one** run of consecutive all-zero groups may be replaced by `::`. Use `::` only once (two would be ambiguous), choose the **longest** run, and (RFC 5952) do not use it for a single zero group. In `2001:db8:0:0:1:0:0:1` the two runs have equal length, and the rule says to compress the **first**: `2001:db8::1:0:0:1`.

**2.** `::1` is loopback; `fe80::/10` is **link-local** (valid only on one link, never routed); `fc00::/7` (in practice `fd..`) is **unique local**, the counterpart of IPv4 private addresses; `2001:db8::/32` is reserved for **documentation**; `ff00::/8` is multicast (`ff02::1` is "all nodes on this link"); everything else in `2000::/3`, such as `2606:4700:4700::1111`, is **global unicast**, the only kind that is routed on the public internet.

**3.** EUI-64 splits the 48-bit MAC in two, inserts `ff:fe` in the middle, and **flips the universal/local bit** (the second-lowest bit of the first byte): `00:1a:2b:3c:4d:5e` becomes `021a:2bff:fe3c:4d5e`, giving `2001:db8:1:2:21a:2bff:fe3c:4d5e`. It is deterministic and needs no state, but the interface ID reveals the hardware address and stays the same on every network, so a device can be **tracked** wherever it goes. Modern systems use stable-privacy addresses (RFC 7217) and temporary random addresses (RFC 8981) instead.

**4.** The solicited-node address is the fixed prefix `ff02::1:ff00:0/104` plus the **last 24 bits** of the unicast address: here `ff02::1:ff3c:4d5e`. Only hosts whose address ends in the same 24 bits listen to that group, so a neighbour solicitation interrupts one or a few hosts instead of every host on the link, which is what an ARP broadcast does. Switches that snoop multicast can forward it to just those ports. This is how IPv6 neighbour discovery replaces ARP.

**5.** A `/48` has 2^(64−48) = 65,536 `/64` subnets (`2001:db8:abcd::/64`, `2001:db8:abcd:1::/64`, ...). One `/64` holds 2^64 addresses, so you never count hosts per subnet. The `/64` boundary is hard-wired into SLAAC, neighbour discovery and many implementations, so even point-to-point links usually get a `/64` (some networks use `/127` on router links, RFC 6164, to avoid ping-pong attacks and neighbour-table exhaustion). Plan with the *nibble boundary* (`/48`, `/52`, `/56`, `/60`, `/64`) so the prefix is readable in hexadecimal and reverse DNS (`ip6.arpa`) delegates cleanly.

**6. The lab.** Start a scratch environment, then run the commands (shown with the output of a real run; MAC-derived addresses differ every time, so they appear as `<...>`):

```bash
unshare -Urnm bash
mount -t tmpfs tmpfs /run && ip link set lo up
ip netns add a; ip netns add b
ip link add ea netns a type veth peer name eb netns b
ip -n a link set ea up; ip -n b link set eb up; ip -n a link set lo up; ip -n b link set lo up
sleep 2
echo "--- link-local addresses appear automatically (no DHCP, no configuration)"
ip -n a -6 addr show dev ea scope link | awk '/inet6/ {print "a:", $2, $3, $4}'
ip -n b -6 addr show dev eb scope link | awk '/inet6/ {print "b:", $2, $3, $4}'
LLB=$(ip -n b -6 addr show dev eb scope link | awk '/inet6/ {print $2}' | cut -d/ -f1)
echo "--- ping the neighbour by its link-local address (the interface must be named: %ea)"
ip netns exec a ping -6 -c 2 -W 1 "$LLB%ea" | grep -E "bytes from|transmitted" | sed 's/time=[0-9.]* ms/time=X ms/; s/from fe80[0-9a-f:]*/from <link-local of b>/'
echo "--- neighbour table (IPv6 uses ND instead of ARP)"
ip -n a -6 neigh show dev ea | awk '{print $1, $2, $3, $4, $5}' | sed 's/^fe80[0-9a-f:]*/<link-local of b>/; s/lladdr [0-9a-f:]*/lladdr <MAC of b>/'
echo "--- add a global address on each side and ping it"
ip -n a addr add 2001:db8:1::1/64 dev ea nodad; ip -n b addr add 2001:db8:1::2/64 dev eb nodad
ip netns exec a ping -6 -c 1 -W 1 2001:db8:1::2 | grep -E "transmitted"
echo "--- all-nodes multicast reaches every host on the link"
ip netns exec a ping -6 -c 1 -W 1 ff02::1%ea | grep -E "bytes from" | head -2 | sed 's/from [0-9a-f:]*%\?[a-z0-9]*/from <host>/; s/time=[0-9.]* ms/time=X ms/'
echo "--- routes"
ip -n a -6 route | sed 's/ proto kernel//; s/ pref medium//'
```

Output:

```text
--- link-local addresses appear automatically (no DHCP, no configuration)
a: fe80::a8ec:92ff:fe0c:c385/64 scope link
b: fe80::7422:f3ff:fe78:6dd6/64 scope link
--- ping the neighbour by its link-local address (the interface must be named: %ea)
64 bytes from <link-local of b>%ea: icmp_seq=1 ttl=64 time=X ms
64 bytes from <link-local of b>%ea: icmp_seq=2 ttl=64 time=X ms
2 packets transmitted, 2 received, 0% packet loss, time 1027ms
--- neighbour table (IPv6 uses ND instead of ARP)
<link-local of b> lladdr <MAC of b> REACHABLE
--- add a global address on each side and ping it
1 packets transmitted, 1 received, 0% packet loss, time 0ms
--- all-nodes multicast reaches every host on the link
64 bytes from <host>: icmp_seq=1 ttl=64 time=X ms
--- routes
2001:db8:1::/64 dev ea metric 256
fe80::/64 dev ea metric 256
```

What to notice: both interfaces **configured themselves** with a link-local `fe80::` address as soon as they came up (here derived from the MAC, which is why `ff:fe` appears in the middle), which is how IPv6 hosts can talk on a link before any router or DHCP server exists. A link-local address is ambiguous on a host with several interfaces (every link has an `fe80::/64`), so you must say which interface with `%ea`. The neighbour table shows IPv6's ARP replacement: `REACHABLE` means a recent confirmation that the neighbour answers. `ff02::1` reaches all hosts on the link. Adding `2001:db8:1::1/64` created the on-link route `2001:db8:1::/64` automatically; the `nodad` option skipped duplicate address detection to avoid a one-second wait in the lab (never do that on real networks).

</details>

## Key takeaways

1. Drop leading zeros, compress the longest zero run once with `::`, and recognise the prefix families: `fe80::/10` link-local, `fc00::/7` unique local, `2000::/3` global, `ff00::/8` multicast.
2. EUI-64 and solicited-node addresses are pure bit manipulation of the MAC or unicast address; EUI-64 leaks identity, so privacy addresses are used instead.
3. A `/48` holds 65,536 `/64`s, and the `/64` is the standard subnet size.
4. IPv6 hosts self-configure link-local addresses, find neighbours with multicast neighbour discovery instead of ARP, and need `%interface` for link-local destinations.
