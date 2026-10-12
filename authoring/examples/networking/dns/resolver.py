"""An iterative caching resolver: starts at the root and follows referrals, as a real recursive resolver does."""
import socket, sys
from dnslib_mini import *

ROOT = "10.1.1.1"; cache = {}; zones = {}; clock = 0; queries = 0; qid = 100    # zones: delegation cache  zone -> (server address, expiry)
def ask(server, name, qtype):
    global queries, qid
    qid += 1; queries += 1
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.settimeout(2)
    s.sendto(build_query(qid, name, qtype), (server, 53)); data, _ = s.recvfrom(512); return parse(data)
def fmt(rr): return f"{rr[0]} {rr[2]}s {rr[1]} {rr[3]}"
def resolve(name, qtype, depth=0):
    key = (name, qtype)
    if key in cache and cache[key][1] > clock:
        print(f"  [cache hit] {name} {qtype}: {[fmt(r) for r in cache[key][0]]}"); return cache[key][0]
    # start at the deepest zone we already know a server for (real resolvers cache NS records and glue, so the root and TLD are rarely asked)
    best = max((z for z in zones if (name == z or name.endswith('.' + z) or z == '.') and zones[z][1] > clock), key=len, default=None)
    server = zones[best][0] if best else ROOT
    for _ in range(10):
        r = ask(server, name, qtype)
        if r["answers"]:
            print(f"  ask {server:<9} {name} {qtype} -> ANSWER (aa={r['aa']}): " + "; ".join(fmt(x) for x in r["answers"]))
            cn = [x for x in r["answers"] if x[1] == "CNAME"]; final = [x for x in r["answers"] if x[1] == qtype]
            ttl = min(x[2] for x in r["answers"]); cache[key] = (r["answers"], clock + ttl); return r["answers"]
        if r["rcode"] == "NXDOMAIN":
            print(f"  ask {server:<9} {name} {qtype} -> NXDOMAIN"); cache[key] = ([], clock + 30); return []
        if r["authority"]:
            zone = r["authority"][0][0]; glue = {a[0]: a[3] for a in r["additional"]}; ns = r["authority"][0][3]
            print(f"  ask {server:<9} {name} {qtype} -> REFERRAL to zone {zone} via {ns} ({glue.get(ns)})"); server = glue[ns]; zones[zone] = (server, clock + r['authority'][0][2]); continue
        print(f"  ask {server:<9} {name} {qtype} -> empty answer (NODATA)"); return []
def lookup(label, name, qtype):
    global queries
    queries = 0; print(f"{label}: {name} {qtype}"); res = resolve(name, qtype); print(f"  => {len(res)} record(s), {queries} network queries\n")

if __name__ == "__main__":
    lookup("1. cold cache", "www.example.test.", "A")
    lookup("2. same name again (answers cached by TTL)", "www.example.test.", "A")
    clock += 61
    lookup("3. 61 seconds later: the 60 s TTL on the A records expired, but the delegation to example.test. (TTL 172800 s) is still cached", "web.example.test.", "A")
    lookup("4. different type, same zone", "example.test.", "MX")
    lookup("5. a name that does not exist", "nothing.example.test.", "A")
