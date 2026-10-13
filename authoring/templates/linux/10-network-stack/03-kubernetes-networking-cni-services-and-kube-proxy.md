---
title: "Kubernetes Networking: Pods, CNI, Services, kube-proxy and NetworkPolicy"
track: linux
module: Network Stack
moduleOrder: 10
order: 3
level: expert
summary: "Kubernetes networking explained as the Linux primitives it is built from: pod namespaces and veth pairs, CNI plugins and pod-CIDR routing or overlay, Services as DNAT rules chosen per connection (rebuilt in a two-node nftables lab), kube-proxy modes, eBPF replacements, DNS, Ingress and NetworkPolicy, and a troubleshooting method."
prerequisites:
  - Linux Namespaces — The Isolation Primitive Behind Containers
  - "The Linux Packet Path: NIC, NAPI, Softirqs, Netfilter and Qdisc"
  - "Linux Network Labs: Namespaces, veth, Bridges, VLANs, tc and nftables"
  - "NAT — Static, NAPT, Port Forwarding, Hairpinning and CGNAT"
sources:
  - label: Kubernetes documentation — Cluster Networking and the Kubernetes network model
    url: https://kubernetes.io/docs/concepts/cluster-administration/networking/
  - label: Kubernetes documentation — Service, EndpointSlices, Virtual IPs and Service Proxies
    url: https://kubernetes.io/docs/reference/networking/virtual-ips/
  - label: Kubernetes documentation — Network Policies
    url: https://kubernetes.io/docs/concepts/services-networking/network-policies/
  - label: Container Network Interface (CNI) specification
    url: https://github.com/containernetworking/cni/blob/main/SPEC.md
  - label: Kubernetes documentation — DNS for Services and Pods
    url: https://kubernetes.io/docs/concepts/services-networking/dns-pod-service/
  - label: Cilium documentation — kube-proxy replacement with eBPF
    url: https://docs.cilium.io/en/stable/network/kubernetes/kubeproxy-free/
  - label: nftables wiki — NAT and the nat hooks
    url: https://wiki.nftables.org/wiki-nftables/index.php/Performing_Network_Address_Translation_(NAT)
---

Kubernetes networking looks like a large system, but it is a short list of Linux features arranged by convention. If you understand network namespaces, veth pairs, bridges, routing, NAT and netfilter (earlier notes), you already understand how it works; what Kubernetes adds is automation and a set of rules about what must be reachable.

## The model: four rules

1. Every **pod** gets its own IP address, in its own network namespace.
2. All pods can reach all other pods **without NAT**, on any node.
3. Agents on a node (kubelet) can reach all pods on that node.
4. A pod sees its own IP as others see it.

These rules are a contract, not an implementation. Something has to create the namespaces, assign addresses and make cross-node pod traffic work; that is the **CNI plugin**. Beyond pod-to-pod there are three more concerns: **Services** (stable virtual IPs for sets of pods), **name resolution** (cluster DNS), and **access control** (NetworkPolicy), plus **ingress** from outside.

## A pod is a namespace

A pod is one network namespace shared by all its containers (that is why containers in a pod talk over `localhost`). The container runtime creates an empty "pause" container that holds the namespace, then calls the CNI plugin with the namespace path. A typical plugin does what the labs in this series did by hand:

```mermaid
graph LR
  subgraph node1
    subgraph podA["pod A namespace"]
      A0["eth0 10.244.1.11"]
    end
    subgraph podB["pod B namespace"]
      B0["eth0 10.244.1.12"]
    end
    BR["bridge cni0 10.244.1.1"]
    A0 --- BR
    B0 --- BR
    RT["routing / NAT / netfilter in the host namespace"]
    BR --- RT
  end
  RT -- "pod CIDR route or tunnel" --> N2["node2: 10.244.2.0/24"]
```

1. Create a veth pair; move one end into the pod namespace as `eth0`.
2. Attach the other end to a bridge (or give it a routed /32 with proxy ARP, as Calico does).
3. Assign the pod an address from the node's **pod CIDR** (IPAM) and set its default route to the node.

## CNI: the plugin interface and the main designs

The **CNI specification** defines how the runtime invokes a plugin: executable file or daemon called with `ADD`, `DEL`, `CHECK` and a JSON config naming the namespace, interface name and IPAM settings; the plugin returns the interfaces and addresses it set up. Plugins differ in how they connect nodes:

| Design | How cross-node traffic moves | Examples | Notes |
|---|---|---|---|
| **Routed, no encapsulation** | each node has a route to every other node's pod CIDR (static routes, or BGP) | Flannel `host-gw`, Calico BGP, Cilium native routing | best performance; nodes must be L2-adjacent or run BGP with the fabric |
| **Overlay** | pod packets are encapsulated in UDP between nodes (VXLAN, Geneve, WireGuard) | Flannel VXLAN, Calico VXLAN/IPIP, Cilium tunnel mode, Weave | works on any underlay; 50-byte overhead (lower the pod MTU), no visibility for the underlay |
| **Cloud-native** | the cloud's VPC routes or assigns pod addresses directly (secondary ENI IPs, alias ranges) | AWS VPC CNI, Azure CNI, GKE | pods are first-class VPC citizens; limited by the cloud's IP quotas |
| **eBPF dataplane** | eBPF programs at `tc`/XDP route and filter, replacing iptables/bridges | Cilium, Calico eBPF mode | fewer hops, identity-based policy, no kube-proxy |

Common problems: **MTU mismatch** with overlays (large packets silently dropped; the pod MTU must be the node MTU minus the encapsulation), exhausted pod CIDRs, and conflicting CIDRs between cluster and the corporate network.

## Services: a virtual IP implemented by rewriting packets

Pods are ephemeral, so clients should not use pod IPs. A **Service** has a stable **ClusterIP** (a virtual address from a separate range, e.g. 10.96.0.0/12, that no interface owns) and a selector; the control plane maintains the set of ready pod addresses as **EndpointSlices**. The ClusterIP does not exist on any interface: it is implemented entirely in the data plane by **kube-proxy** on each node (or by the CNI), which programs the kernel so that packets addressed to the ClusterIP are **destination-NAT'ed** to one of the endpoints.

- **iptables mode** (long the default): a chain per Service and per endpoint in the `nat` table; selection via `-m statistic --mode random --probability`. Rule evaluation is linear, so with tens of thousands of Services updates and lookups slow down.
- **IPVS mode**: uses the kernel's IP Virtual Server load balancer (hash tables, multiple schedulers such as round robin or least connections), scales to very large Service counts.
- **nftables mode**: the successor to iptables mode using verdict maps and sets, with efficient updates.
- **eBPF replacement** (Cilium): the translation happens at socket level (`connect()` is rewritten in a cgroup BPF hook so packets never carry the ClusterIP) or at the tc layer; no conntrack entry per connection for in-cluster traffic.

Where the rule is applied: for traffic from a pod, in the node's `PREROUTING` after it leaves the pod's veth; for traffic originating on the node itself, in `OUTPUT`. Because DNAT happens once, when the connection's first packet is processed, and the connection tracker remembers the choice, **all packets of a connection go to the same endpoint**; the load balancing is **per connection, not per packet**.

### Lab: a Service built from nftables

The script builds two nodes with pod bridges (`cni0`), two pods per node, a routed pod network between the nodes (the *host-gw* design: each node has a route to the other's pod CIDR), then implements a Service with the same mechanism as kube-proxy's nftables mode: a chain that picks an endpoint with `numgen random` and jumps to a chain that DNATs to it. Three pods run a tiny responder that returns its own name and the client address it saw.

{{include linux/k8snet/lab.sh}}

{{run linux/k8snet :: unshare -Urnm sh lab.sh 2>&1}}

What to take from it:

- **Pod to pod across nodes** works by plain IP routing (`ttl=62`: pod to node, node to node, node to pod). No NAT, as the model requires.
- The Service chain shows the whole mechanism: `numgen random mod 3 vmap {...}` selects one of three endpoint chains, each ending in a `dnat`. There is no process in the data path and no interface owns `10.96.0.10`.
- **30 new connections** spread across the three pods roughly evenly (random, so not exactly 10/10/10). A real kube-proxy also keeps this stable per connection through conntrack, as the second experiment's single connection shows: the client believes it is talking to the VIP (`getpeername` shows the ClusterIP) while the reply comes from a pod.
- For endpoints on **other nodes** the endpoint sees the **real pod IP of the client** (`10.244.1.11`), because the DNAT happens on the client's node and the reply route passes back through that same node, where conntrack restores the Service address. An endpoint on the **client's own node** (`p12` here) is different: after DNAT the request travels on the same bridge and the reply would go straight back to the client, bypassing the node, so the client would see an unexpected source address and drop it. kube-proxy fixes this with a **masquerade** rule for that hairpin case, which is what the `post` chain does; the endpoint then sees the node's bridge address.
- **Removing an endpoint** is only a rule change: with one pod taken out of the rule set, new connections avoid it, while established connections stay in conntrack. This is exactly what happens on a rolling update; draining depends on readiness probes and the pod's termination handling, because connections already in conntrack keep going to the old pod until they end.

### Service types and traffic policy

| Type | Meaning |
|---|---|
| `ClusterIP` | virtual IP reachable inside the cluster only |
| `NodePort` | additionally opens the same port (30000-32767 by default) on every node; traffic to any node's IP:port is DNAT'ed to an endpoint, possibly on another node (extra hop plus SNAT, hiding the client IP unless `externalTrafficPolicy: Local`) |
| `LoadBalancer` | asks the cloud (or MetalLB/ BGP speaker) for an external IP that forwards to the NodePorts or directly to pods |
| `ExternalName` | a DNS CNAME to an external name, no proxying |
| Headless (`clusterIP: None`) | no virtual IP; DNS returns the pod IPs directly (used by StatefulSets) |

`externalTrafficPolicy: Local` keeps traffic on the node that received it and preserves the client source IP, at the cost of uneven balance (a node without a local endpoint drops the traffic, so the load balancer's health check must exclude it). `internalTrafficPolicy` and topology-aware routing prefer endpoints in the same node or zone to save latency and cross-zone cost.

## DNS

CoreDNS runs as a Service (typically `10.96.0.10`) and answers names like `web.default.svc.cluster.local` (A record = ClusterIP, or all pod IPs for a headless Service) and `10-244-1-12.default.pod.cluster.local`. Each pod's `/etc/resolv.conf` has `nameserver <CoreDNS ClusterIP>`, `search default.svc.cluster.local svc.cluster.local cluster.local` and **`options ndots:5`**. With `ndots:5`, a name with fewer than five dots such as `api.example.com` is first tried with every search suffix (`api.example.com.default.svc.cluster.local`, ...) before the literal name, producing several useless NXDOMAIN queries per lookup, a frequent cause of DNS load and latency. Mitigations: fully qualified names with a trailing dot, a lower `ndots`, NodeLocal DNSCache, and CoreDNS autoscaling. Conntrack races on UDP DNS (parallel A and AAAA queries from the same source port) were a well-known source of 5-second delays on older kernels.

## Ingress and Gateway API

External HTTP(S) enters through a **load balancer to an ingress controller** (NGINX, Envoy-based such as Contour/Emissary/Istio gateway, HAProxy, Traefik), a normal pod that reads `Ingress` or **Gateway API** resources (`Gateway`, `HTTPRoute`) and configures routing by host and path, TLS termination and retries. Gateway API is the newer, role-oriented replacement for Ingress. Non-HTTP traffic uses `LoadBalancer` Services or `TCPRoute`/`UDPRoute`.

## NetworkPolicy

By default every pod can talk to every other pod. A **NetworkPolicy** selects pods (by label) and defines allowed **ingress** and/or **egress** peers (pod labels, namespaces, IP blocks) and ports; a pod selected by at least one policy is default-deny for that direction, and policies are additive allows. Kubernetes only stores the object; the **CNI plugin enforces it**: with iptables/nftables sets (Calico), eBPF programs matching on **identity** (Cilium), or not at all (Flannel alone ignores policies). Typical baseline: a default-deny policy per namespace, then explicit allows (DNS egress to CoreDNS, app tiers, ingress from the controller). Policies are L3/L4; L7 rules need Cilium's L7 policy or a service mesh.

## Service mesh

Sidecar or ambient meshes (Istio, Linkerd, Cilium service mesh) add mTLS between pods, L7 routing, retries, observability and policy. Sidecar proxies are inserted with `iptables` REDIRECT rules in the pod's namespace (`istio-init`) so that all pod traffic passes through a local Envoy; ambient modes move that into per-node proxies. The cost is latency, memory and operational complexity; use it when you need mTLS and traffic management uniformly.

## Troubleshooting method

Work the path in order:

1. **Pod networking**: `kubectl exec` into the pod (or `nsenter -t <pid> -n`), then `ip addr`, `ip route`, `ping` the node gateway and another pod. Failure: CNI problem, MTU, pod CIDR route missing.
2. **Cross-node**: from node A, `ip route get <pod IP on B>`, `tcpdump -i any host <pod IP>` on both nodes; check overlay UDP ports (8472 VXLAN, 6081 Geneve, 51820 WireGuard) are allowed between nodes.
3. **Service**: does the Service have endpoints (`kubectl get endpointslices`)? Does the readiness probe pass? On the node, `nft list ruleset | grep <ClusterIP>` or `iptables-save -t nat | grep <ClusterIP>`, `ipvsadm -Ln`, `conntrack -L -d <ClusterIP>`.
4. **DNS**: `kubectl exec ... -- nslookup kubernetes.default`, check CoreDNS pods and logs, `resolv.conf`, the `ndots` behavior.
5. **Policy**: look for a default-deny; test with the policy removed; use the CNI's flow log (`hubble observe`, Calico flow logs).
6. **Ingress/external**: the load balancer's health checks, `externalTrafficPolicy`, security groups, node ports.

Symptoms: connection hangs only for some requests (a bad endpoint in the rule set), 5-second DNS delays (conntrack race or missing UDP retry), large requests fail while small succeed (MTU), Service works from some nodes only (kube-proxy not running there, or the route/`rp_filter`), external clients show node IPs (SNAT from NodePort or `externalTrafficPolicy: Cluster`).

## Key takeaways

- A pod is a network namespace connected by a veth pair; a CNI plugin sets up the interface, address and cross-node reachability (routes, BGP, or an overlay) so that pods reach each other without NAT.
- A Service's ClusterIP is not an interface address: kube-proxy (iptables, IPVS, nftables) or an eBPF dataplane rewrites packets with destination NAT to a ready endpoint, chosen per connection and remembered by conntrack.
- Hairpin traffic and NodePort need masquerade; `externalTrafficPolicy: Local` preserves client IPs at the cost of balance.
- DNS (`ndots:5`), MTU and endpoint readiness are the usual sources of mysterious latency and partial failures.
- NetworkPolicy is enforced by the CNI, not by Kubernetes itself; start from default-deny per namespace.
