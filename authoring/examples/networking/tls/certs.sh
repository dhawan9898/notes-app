#!/bin/sh
# Make a throwaway CA and a server certificate (ECDSA P-256), then inspect them. Files go in ./pki (ignored by git).
set -e
rm -rf pki; mkdir pki; cd pki
openssl ecparam -name prime256v1 -genkey -noout -out ca.key
openssl req -x509 -new -key ca.key -sha256 -days 30 -subj "/CN=Demo Root CA" -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign" -out ca.pem
openssl ecparam -name prime256v1 -genkey -noout -out server.key
openssl req -new -key server.key -subj "/CN=www.example.test" -out server.csr
printf "subjectAltName=DNS:www.example.test\nbasicConstraints=CA:FALSE\nkeyUsage=digitalSignature\nextendedKeyUsage=serverAuth\n" > ext.cnf
openssl x509 -req -in server.csr -CA ca.pem -CAkey ca.key -CAcreateserial -days 7 -sha256 -extfile ext.cnf -out server.pem 2>/dev/null
echo '$ openssl x509 -in server.pem -noout -subject -issuer -ext subjectAltName,keyUsage,extendedKeyUsage'
openssl x509 -in server.pem -noout -subject -issuer -ext subjectAltName,keyUsage,extendedKeyUsage | sed 's/^/  /'
echo '$ openssl verify -CAfile ca.pem server.pem'; openssl verify -CAfile ca.pem server.pem
echo '$ openssl verify server.pem   (no trust anchor given)'; openssl verify server.pem 2>&1 | head -3 | sed 's/^/  /'
echo '$ openssl x509 -in server.pem -noout -text | grep -E "Public Key Algorithm|ASN1 OID|Signature Algorithm" | sort -u'
openssl x509 -in server.pem -noout -text | grep -E "Public Key Algorithm|ASN1 OID|Signature Algorithm" | sort -u | sed 's/^ */  /'
