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
