#!/usr/bin/env python3
"""A tiny declarative network configurator for Linux. It reads the desired state from JSON, reads the actual state with `ip -j`,
computes a plan (the minimal set of changes), and applies it idempotently. Usage: reconcile.py desired.json [--check] [--strict] [--rollback-ping IP]"""
import ipaddress, json, subprocess, sys

def sh(*cmd, check=True):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if check and r.returncode: raise RuntimeError(f"{' '.join(cmd)}: {r.stderr.strip()}")
    return r.stdout

def validate(d):                                                              # reject bad input BEFORE touching the device
    for i in d.get("interfaces", []):
        assert isinstance(i["name"], str) and len(i["name"]) <= 15, f"bad interface name {i['name']!r}"
        for a in i.get("addresses", []): ipaddress.ip_interface(a)           # raises ValueError for malformed addresses
        assert 68 <= i.get("mtu", 1500) <= 9216, f"bad MTU on {i['name']}"
    for r in d.get("routes", []): ipaddress.ip_network(r["dst"]); ipaddress.ip_address(r["via"])

def actual():
    links = {l["ifname"]: l for l in json.loads(sh("ip", "-j", "link"))}
    addrs = {a["ifname"]: {f"{x['local']}/{x['prefixlen']}" for x in a["addr_info"] if x["family"] == "inet"} for a in json.loads(sh("ip", "-j", "addr"))}
    routes = {r["dst"] + "|" + r.get("gateway", "") for r in json.loads(sh("ip", "-j", "route")) if "gateway" in r}
    return links, addrs, routes

def plan(d, strict):
    links, addrs, routes = actual(); acts = []
    for i in d.get("interfaces", []):
        n = i["name"]
        if n not in links: acts.append((f"create dummy interface {n}", ["ip", "link", "add", n, "type", "dummy"], ["ip", "link", "del", n]))
        have = addrs.get(n, set()); want = {str(ipaddress.ip_interface(a)) for a in i.get("addresses", [])}
        for a in sorted(want - have): acts.append((f"add address {a} to {n}", ["ip", "addr", "add", a, "dev", n], ["ip", "addr", "del", a, "dev", n]))
        if strict:
            for a in sorted(have - want): acts.append((f"remove unexpected address {a} from {n}", ["ip", "addr", "del", a, "dev", n], ["ip", "addr", "add", a, "dev", n]))
        if links.get(n, {}).get("mtu") != i.get("mtu", 1500) and "mtu" in i: acts.append((f"set MTU {i['mtu']} on {n}", ["ip", "link", "set", n, "mtu", str(i["mtu"])], ["ip", "link", "set", n, "mtu", str(links.get(n, {}).get("mtu", 1500))]))
        if (("UP" in links.get(n, {}).get("flags", [])) != i.get("up", True)): acts.append((f"set {n} {'up' if i.get('up', True) else 'down'}", ["ip", "link", "set", n, "up" if i.get("up", True) else "down"], ["ip", "link", "set", n, "down" if i.get("up", True) else "up"]))
    for r in d.get("routes", []):
        if str(ipaddress.ip_network(r["dst"])) + "|" + r["via"] not in {x.replace("default", "0.0.0.0/0") for x in routes}: acts.append((f"add route {r['dst']} via {r['via']}", ["ip", "route", "replace", r["dst"], "via", r["via"]], ["ip", "route", "del", r["dst"], "via", r["via"]]))
    return acts

def main():
    desired = json.load(open(sys.argv[1])); check = "--check" in sys.argv; strict = "--strict" in sys.argv
    validate(desired); acts = plan(desired, strict)
    if not acts: print("no changes: the device already matches the desired state"); return 0
    for text, cmd, undo in acts: print(("would " if check else "") + text)
    if check: return 0
    for text, cmd, undo in acts: sh(*cmd)
    if "--rollback-ping" in sys.argv:                                         # safe change: verify reachability, otherwise undo (like "commit confirmed")
        peer = sys.argv[sys.argv.index("--rollback-ping") + 1]
        if subprocess.run(["ping", "-c", "1", "-W", "1", peer], capture_output=True).returncode:
            print(f"health check FAILED: {peer} unreachable after the change -> rolling back")
            for text, cmd, undo in reversed(acts): sh(*undo, check=False)           # undo every action, last first
            return 2
        print(f"health check passed: {peer} reachable")
    print(f"applied {len(acts)} change(s)"); return 0

sys.exit(main())
