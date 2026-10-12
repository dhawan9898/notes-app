#!/usr/bin/env python3
"""TLS server and timing client. Usage: tls_lab.py server | tls_lab.py client"""
import os, socket, ssl, sys, threading, time

HOST, PORT = "10.0.0.2", 4433

def server():
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); ctx.load_cert_chain("pki/server.pem", "pki/server.key"); ctx.set_alpn_protocols(["h2", "http/1.1"])
    ls = socket.socket(); ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); ls.bind((HOST, PORT)); ls.listen(16)
    def handle(c):
        try:
            with ctx.wrap_socket(c, server_side=True) as t: t.recv(100); t.sendall(b"ok")
        except Exception: pass
    while True:
        c, _ = ls.accept(); threading.Thread(target=handle, args=(c,), daemon=True).start()

def make_ctx(version):
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT); ctx.load_verify_locations("pki/ca.pem")
    ctx.minimum_version = ctx.maximum_version = version; ctx.set_alpn_protocols(["h2", "http/1.1"]); return ctx

def one(ctx, session=None):
    t0 = time.perf_counter(); raw = socket.create_connection((HOST, PORT)); raw.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, int(os.environ.get("NODELAY", "1"))); t1 = time.perf_counter()   # NODELAY: otherwise Nagle can hold the request back behind the TLS Finished message
    s = ctx.wrap_socket(raw, server_hostname="www.example.test", session=session); t2 = time.perf_counter()   # wrap_socket performs the handshake
    s.sendall(b"GET /"); s.recv(10); t3 = time.perf_counter()
    info = (s.version(), s.cipher()[0], s.selected_alpn_protocol(), s.session_reused); sess = s.session; s.close()
    return (t1 - t0, t2 - t1, t3 - t0), info, sess

def client():
    one(make_ctx(ssl.TLSVersion.TLSv1_3))                          # warm-up: the very first packets also pay for ARP resolution
    print(f"{'case':<26} {'TCP connect':>12} {'TLS handshake':>14} {'first byte':>11}   protocol / cipher / ALPN / resumed")
    for label, ver in (("TLS 1.2, full handshake", ssl.TLSVersion.TLSv1_2), ("TLS 1.3, full handshake", ssl.TLSVersion.TLSv1_3)):
        ctx = make_ctx(ver); (a, b, c), info, sess = one(ctx)
        print(f"{label:<26} {a * 1e3:9.0f} ms {b * 1e3:11.0f} ms {c * 1e3:8.0f} ms   {info[0]} / {info[1]} / {info[2]} / {info[3]}")
        if ver == ssl.TLSVersion.TLSv1_3:
            time.sleep(0.2)                                          # TLS 1.3 session tickets arrive after the handshake; reuse one (same context)
            (a, b, c), info, _ = one(ctx, sess)
            print(f"{'TLS 1.3, resumed session':<26} {a * 1e3:9.0f} ms {b * 1e3:11.0f} ms {c * 1e3:8.0f} ms   {info[0]} / {info[1]} / {info[2]} / {info[3]}")

(server if sys.argv[1] == "server" else client)()
