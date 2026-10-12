#!/usr/bin/env python3
"""Slotted simulation of 802.11 DCF (CSMA/CA with binary exponential backoff), saturated stations, ideal channel (no hidden nodes, no noise).
Every station always has a frame to send. Contention window: CWmin = 15, CWmax = 1023 (OFDM PHY)."""
import random, sys

CW_MIN, CW_MAX, RETRY_LIMIT = 15, 1023, 7
T_SUCCESS, T_COLLISION = 40, 40       # slots a successful exchange / a collision occupies the medium (data + SIFS + ACK, in slot units)

def run(n, slots=2_000_000, seed=1):
    rnd = random.Random(seed)
    cw = [CW_MIN] * n; backoff = [rnd.randint(0, CW_MIN) for _ in range(n)]; retries = [0] * n
    attempts = collisions = successes = drops = 0; busy = 0; per_station = [0] * n; t = 0
    while t < slots:
        m = min(backoff)
        t += m; backoff = [b - m for b in backoff]                       # idle slots while every station counts down
        tx = [i for i in range(n) if backoff[i] == 0]
        attempts += len(tx)
        if len(tx) == 1:
            i = tx[0]; successes += 1; per_station[i] += 1; busy += T_SUCCESS; t += T_SUCCESS
            cw[i] = CW_MIN; retries[i] = 0; backoff[i] = rnd.randint(0, cw[i])
        else:
            collisions += len(tx); t += T_COLLISION
            for i in tx:                                                   # collision: double the contention window and retry
                retries[i] += 1
                if retries[i] > RETRY_LIMIT: drops += 1; cw[i] = CW_MIN; retries[i] = 0
                else: cw[i] = min(2 * cw[i] + 1, CW_MAX)
                backoff[i] = rnd.randint(0, cw[i])
    jain = sum(per_station) ** 2 / (n * sum(x * x for x in per_station))   # Jain's fairness index: 1.0 = perfectly equal shares
    return collisions / attempts, busy / t, jain, drops / max(1, successes + drops)

print(f"{'stations':>8} {'P(collision) per attempt':>26} {'channel efficiency':>20} {'fairness (Jain)':>16} {'frames dropped':>15}")
for n in (1, 2, 5, 10, 20, 50):
    pc, eff, fair, drop = run(n)
    print(f"{n:>8} {pc * 100:>25.1f}% {eff * 100:>19.1f}% {fair:>16.3f} {drop * 100:>14.2f}%")
