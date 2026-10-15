---
title: Building GRE and VXLAN Tunnels and Counting the MTU Cost
track: networking
module: Overlays and VPN
moduleOrder: 7
order: 1
level: advanced
minutes: 50
env: machine
summary: "A lab that builds a GRE tunnel and a VXLAN tunnel between two virtual hosts, shows what each carries (an IP network versus an Ethernet segment), and measures the MTU cost of encapsulation with ping and the don't-fragment bit, with the full script and captured output."
prerequisites:
  - "Tunneling Fundamentals — GRE, IP-in-IP and GRE-in-UDP"
  - "IPsec and IKEv2"
  - "VXLAN and BGP EVPN"
sources:
  - label: "RFC 2784 — Generic Routing Encapsulation (GRE)"
    url: https://www.rfc-editor.org/rfc/rfc2784
  - label: "RFC 7348 — Virtual eXtensible Local Area Network (VXLAN)"
    url: https://www.rfc-editor.org/rfc/rfc7348
  - label: "man 8 ip-link (gre and vxlan sections)"
    url: https://man7.org/linux/man-pages/man8/ip-link.8.html
---

An overlay network is a network built on top of another one by wrapping packets inside packets. The physical network (the **underlay**) only has to deliver the outer packets; the **overlay** carries the inner ones between endpoints that may be far apart. This lab builds two kinds between two virtual hosts and measures what the wrapping costs. It runs in a throwaway namespace and needs no root on the host; the setup uses documentation addresses (`192.0.2.0/24`) for the underlay.

## The lab

```bash
unshare -Urnm bash
mount -t tmpfs tmpfs /run && ip link set lo up
ip netns add a; ip netns add b
ip link add va netns a type veth peer name vb netns b
ip -n a addr add 192.0.2.1/24 dev va; ip -n b addr add 192.0.2.2/24 dev vb
ip -n a link set va up; ip -n b link set vb up; ip -n a link set lo up; ip -n b link set lo up
x() { ip netns exec "$@"; }
echo "--- underlay reachability"; x a ping -c 1 -W 1 192.0.2.2 | grep transmitted
echo "--- 1. GRE tunnel carrying private 10.1.1.0/30"
ip -n a link add gre1 type gre local 192.0.2.1 remote 192.0.2.2 ttl 64
ip -n b link add gre1 type gre local 192.0.2.2 remote 192.0.2.1 ttl 64
ip -n a addr add 10.1.1.1/30 dev gre1; ip -n b addr add 10.1.1.2/30 dev gre1
ip -n a link set gre1 up; ip -n b link set gre1 up
x a ping -c 2 -W 1 10.1.1.2 | grep -E "transmitted"
ip -n a -d link show gre1 | grep -oE "gre remote [0-9.]+ local [0-9.]+|mtu [0-9]+" | tr '\n' ' '; echo
echo "--- 2. VXLAN (VNI 100) carrying an Ethernet segment 10.2.2.0/24"
ip -n a link add vx100 type vxlan id 100 local 192.0.2.1 remote 192.0.2.2 dstport 4789 dev va
ip -n b link add vx100 type vxlan id 100 local 192.0.2.2 remote 192.0.2.1 dstport 4789 dev vb
ip -n a addr add 10.2.2.1/24 dev vx100; ip -n b addr add 10.2.2.2/24 dev vx100
ip -n a link set vx100 up; ip -n b link set vx100 up
x a ping -c 2 -W 1 10.2.2.2 | grep transmitted
ip -n a -d link show vx100 | grep -oE "vxlan id [0-9]+ remote [0-9.]+ local [0-9.]+|dstport [0-9]+|mtu [0-9]+" | tr '\n' ' '; echo
ip -n a neigh show dev vx100 | awk '{print "inner ARP entry:", $1, "lladdr", "<MAC>"}'
echo "--- 3. MTU cost of encapsulation"
for d in va gre1 vx100; do printf "%-6s MTU " $d; ip -n a -o link show $d | grep -oE "mtu [0-9]+" | cut -d' ' -f2; done
echo "--- 4. ping with the don't-fragment bit: largest payload that fits through each path"
for target in "192.0.2.2" "10.1.1.2" "10.2.2.2"; do
  best=0; for size in 1472 1452 1450 1448 1426 1422 1400; do
    if x a ping -c 1 -W 1 -M do -s $size $target >/dev/null 2>&1; then best=$size; break; fi; done
  echo "to $target: largest working payload in my list = $best bytes"
done
```

Output of that script:

```text
--- underlay reachability
1 packets transmitted, 1 received, 0% packet loss, time 0ms
--- 1. GRE tunnel carrying private 10.1.1.0/30
2 packets transmitted, 2 received, 0% packet loss, time 1002ms
mtu 1476 mtu 0 mtu 0 gre remote 192.0.2.2 local 192.0.2.1 
--- 2. VXLAN (VNI 100) carrying an Ethernet segment 10.2.2.0/24
2 packets transmitted, 2 received, 0% packet loss, time 1026ms
mtu 1450 mtu 68 mtu 65535 vxlan id 100 remote 192.0.2.2 local 192.0.2.1 dstport 4789 
inner ARP entry: 10.2.2.2 lladdr <MAC>
--- 3. MTU cost of encapsulation
va     MTU 1500
gre1   MTU 1476
vx100  MTU 1450
--- 4. ping with the don't-fragment bit: largest payload that fits through each path
to 192.0.2.2: largest working payload in my list = 1472 bytes
to 10.1.1.2: largest working payload in my list = 1448 bytes
to 10.2.2.2: largest working payload in my list = 1422 bytes
```

## Exercises

**1. Build and test GRE.** Two hosts `a` and `b` are on the underlay `192.0.2.0/24`. Create a GRE tunnel between them carrying the private network `10.1.1.0/30`. Which two addresses define the tunnel *outer* header, which are used *inside*, and what MTU does the kernel give the tunnel device?

**2. Build and test VXLAN.** Create a VXLAN tunnel with VNI 100 between the same hosts and give each end an address in `10.2.2.0/24`. What does VXLAN carry that GRE (as built here) does not, which UDP port does it use, and what is a VNI?

**3. Count the bytes.** Work out why the tunnel MTUs are 1476 and 1450. How many bytes does each encapsulation add? What do the three "largest payload" results (1472, 1448 and 1422 bytes) tell you, and what goes wrong if you ignore this in production?

**4. Why `dev va`?** The VXLAN device was created with `dev va` (and `dev vb`). In a first attempt without it, the VXLAN device had an MTU of 1500. Why is that dangerous?

**5. Short answers.** (a) What does IPsec tunnel mode add that GRE and VXLAN do not? (b) Why does VXLAN use UDP source ports that vary? (c) What would you need to add to make this a multi-site VXLAN network instead of a point-to-point one?

## Answers

Try before opening.

<details>
<summary>Show the explanations</summary>

**1. GRE.** The outer addresses are the **underlay** addresses given with `local 192.0.2.1 remote 192.0.2.2`: every packet through the tunnel is wrapped in an IP header between those two. The inner packets use the **tunnel** addresses `10.1.1.1` and `10.1.1.2` (the `/30`), and pings between them succeed (2 of 2). GRE adds a small header (4 bytes, more with keys or sequence numbers) and the new 20-byte outer IP header, so the kernel set the tunnel interface MTU to **1476** = 1500 − 20 − 4. GRE is a Layer 3 tunnel here (it carries IP packets; `gretap` carries Ethernet frames). It does no encryption and no authentication, so anyone who can inject packets towards the endpoint can inject into the tunnel; run it inside IPsec when it crosses untrusted networks (see the answer to question 5).

**2. VXLAN.** VXLAN carries **complete Ethernet frames**: the two ends behave as if they were plugged into one switch, a **Layer 2 segment** (hence ARP works across it: the output shows the inner ARP entry for `10.2.2.2`, and it would carry broadcast, non-IP protocols and VLAN-tagged frames too). The frames are wrapped in a VXLAN header, then **UDP destination port 4789** (the IANA-assigned VXLAN port), then IP. The **VNI** (VXLAN Network Identifier) is a 24-bit number in the header that says *which* virtual segment a frame belongs to: with 24 bits there are 16 million segments, against the 4,094 of VLAN IDs, so one underlay can carry many separate tenants whose overlays may even reuse the same IP ranges. VNI 100 here is one such segment. That scale and the fact that it runs over any routed IP network are why VXLAN is the standard overlay in data centres and in Kubernetes networking (*Data-Centre Fabrics: Leaf-Spine Clos, eBGP Underlay and ECMP*, *VXLAN and BGP EVPN*).

**3. The MTU arithmetic.** (In the output the first `mtu` figure on each `ip -d link` line is the current MTU; the extra `mtu 68` and `mtu 65535` are only the range the kernel allows for the device.) The underlay MTU is 1500. Each tunnel's inner packets must fit after the extra headers are added:

| Path | Overhead | Device MTU | Largest ping payload (MTU − 20 IP − 8 ICMP) |
|---|---|---|---|
| underlay `va` | none | 1500 | **1472** |
| GRE | 20 outer IP + 4 GRE = 24 | 1476 | 1476 − 28 = **1448** |
| VXLAN | 20 outer IP + 8 UDP + 8 VXLAN + 14 inner Ethernet = 50 | 1450 | 1450 − 28 = **1422** |

The measured results (1472, 1448, 1422) match exactly, found with `ping -M do -s SIZE`, which sets the **Don't Fragment** bit so that a packet that is too big fails instead of being silently fragmented. Ignoring this is a classic production fault: a path that works for small packets (a login, a DNS lookup, a `ping`) but **hangs for large ones** (a file transfer, a TLS handshake with a big certificate). If ICMP "fragmentation needed" messages are blocked, Path MTU Discovery fails and the connection stalls (a *PMTU black hole*). The fixes are to lower the MTU on the tunnel (as the kernel did here) or the hosts, to **clamp the TCP MSS** at the tunnel endpoint (`iptables -t mangle ... --clamp-mss-to-pmtu`, or the `nft` equivalent), or to raise the underlay MTU (jumbo frames, 9000 bytes, which is what data centres do for VXLAN fabrics).

**4.** Without `dev va`, the kernel does not know which underlay interface the tunnel will use, so it cannot subtract the overhead and leaves the VXLAN device at **1500**. Pinging 1472 bytes still worked in the lab, but only because the *outer* packets (1500 + 50 = 1550 bytes) were **fragmented** by the underlay: slower, and it fails outright wherever the outer packets have the Don't Fragment bit or fragments are dropped. With `dev va` the device MTU is **1450**, so inner packets are limited and outer packets fit. Always bind the tunnel to an underlay device (or set the MTU explicitly) and verify with `ping -M do`.

**5.** (a) **IPsec tunnel mode** adds **authentication and encryption** (ESP, with keys negotiated by IKEv2) around the whole inner packet, so the data stays confidential and untampered across the underlay; it costs more bytes (about 50 to 70) and CPU, and it carries only IP unicast traffic by itself, which is why GRE-over-IPsec is used when routing protocols or multicast must cross it. GRE and VXLAN are pure encapsulation. (b) The VXLAN sender derives the **UDP source port from a hash of the inner flow headers**, so routers and switches in the underlay that load-balance with ECMP over the (source, destination, ports) tuple spread different inner flows over different paths, while packets of one flow always take the same path and stay in order. A fixed source port would put all the traffic between two tunnel endpoints on one path. (c) A **point-to-point** tunnel has one fixed remote. For many sites you give each endpoint (**VTEP**) a way to learn where each inner MAC address lives: flood-and-learn using an underlay **multicast group** per VNI (`group 239.1.1.1 dev eth0`), static forwarding entries, or a control plane such as **BGP EVPN** that distributes MAC and IP reachability so no flooding is needed (*VXLAN and BGP EVPN*).

</details>

## Key takeaways

1. An overlay wraps packets in packets; the underlay only needs to deliver the outer packets between endpoints.
2. GRE (here) tunnels IP, VXLAN tunnels whole Ethernet frames over UDP port 4789 with a 24-bit VNI identifying the virtual segment.
3. Encapsulation costs bytes (GRE 24, VXLAN 50), so the tunnel MTU is smaller than the underlay's; test with `ping -M do -s SIZE` and fix with MTU, MSS clamping or jumbo frames.
4. Bind VXLAN to an underlay device so the MTU is set correctly; encapsulation alone gives no confidentiality, which IPsec adds.
