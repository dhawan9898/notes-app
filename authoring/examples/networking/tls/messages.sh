#!/bin/sh
# The TLS 1.3 and 1.2 handshake messages, as listed by openssl s_client -msg (client sends >>>, server sends <<<).
sh certs.sh > /dev/null
for v in tls1_3 tls1_2; do
  openssl s_server -accept 4434 -cert pki/server.pem -key pki/server.key -$v -quiet -naccept 1 > /dev/null 2>&1 &
  sleep 0.5
  echo "== $v =="
  echo | openssl s_client -connect 127.0.0.1:4434 -$v -CAfile pki/ca.pem -servername www.example.test -msg 2>&1 | grep -E "^(>>>|<<<)" | sed -E 's/\[length [0-9a-f]+\],? ?//; s/^(>>>|<<<) /\1 /; s/ +$//' | grep -vE "^(>>>|<<<) (TLS 1.[23] Record Layer|ignore)" | awk '{ if ($1 == ">>>") d="client -> server"; else d="server -> client"; $1=""; print "  " d ":" $0 }' | grep -vE "RecordHeader|InnerContent" | head -14
  wait
done
