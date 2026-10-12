"""Three authoritative DNS servers (root, 'test' TLD, 'example.test') on separate addresses, each answering only for its own zone and referring downward."""
import socket, sys, threading
from dnslib_mini import *

ZONES = {
    "10.1.1.1": {"zone": ".", "records": {}, "delegations": {"test.": [("a.nic.test.", "10.1.1.2")]}},
    "10.1.1.2": {"zone": "test.", "records": {}, "delegations": {"example.test.": [("ns1.example.test.", "10.1.1.3")]}},
    "10.1.1.3": {"zone": "example.test.", "delegations": {}, "records": {
        "www.example.test.": [("CNAME", 300, "web.example.test.")],
        "web.example.test.": [("A", 60, "192.0.2.10"), ("A", 60, "192.0.2.11"), ("AAAA", 60, "2001:db8::10")],
        "example.test.": [("MX", 3600, (10, "mail.example.test.")), ("TXT", 3600, "v=spf1 -all")],
        "mail.example.test.": [("A", 3600, "192.0.2.25")]}},
}
def serve(addr, cfg):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.bind((addr, 53))
    while True:
        data, peer = s.recvfrom(512); q = parse(data); name = q["qname"].lower(); recs = cfg["records"]
        ans, auth, add, rcode, aa = [], [], [], 0, 0
        deleg = next((d for d in cfg["delegations"] if name == d or name.endswith("." + d)), None)
        if deleg:                                                   # not ours: refer the resolver to the child zone's name servers (with glue)
            for ns, ip in cfg["delegations"][deleg]: auth.append((deleg, "NS", 172800, ns)); add.append((ns, "A", 172800, ip))
        else:
            aa = 1; cur = name
            for _ in range(4):                                      # follow CNAME inside this zone
                found = recs.get(cur, [])
                match = [r for r in found if r[0] == q["qtype"]]
                cn = [r for r in found if r[0] == "CNAME"]
                if cn and q["qtype"] != "CNAME": ans.append((cur, "CNAME", cn[0][1], cn[0][2])); cur = cn[0][2]; continue
                ans += [(cur, r[0], r[1], r[2]) for r in match]; break
            if not ans and name not in recs: rcode = 3             # NXDOMAIN: the name does not exist at all
        s.sendto(build_response(q["id"], q["qname"], q["qtype"], rcode, aa, ans, auth, add), peer)
for a, c in ZONES.items(): threading.Thread(target=serve, args=(a, c), daemon=True).start()
threading.Event().wait()
