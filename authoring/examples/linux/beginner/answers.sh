#!/bin/bash
# Solutions to the practice exercises, run in a throwaway directory.
set -u
work=$(mktemp -d); cd "$work"
run() { echo "\$ $*"; eval "$@" 2>&1; }

# Fixture: a small web-server-like log
cat > access.log <<'LOG'
10.0.0.5 GET /index.html 200
10.0.0.7 GET /about.html 200
10.0.0.5 POST /login 302
10.0.0.9 GET /missing 404
10.0.0.5 GET /index.html 200
10.0.0.7 GET /missing 404
10.0.0.9 GET /index.html 500
LOG
echo "Ex 1:"; run 'wc -l < access.log'
echo "Ex 2:"; run 'grep -c " 404$" access.log'
echo "Ex 3:"; run 'cut -d" " -f1 access.log | sort | uniq -c | sort -rn | head -1'
echo "Ex 4:"; run 'awk "\$4 >= 400 {print \$1, \$3, \$4}" access.log'
echo "Ex 5:"; run 'mkdir -p backup && cp access.log backup/access.log.bak && ls backup'
echo "Ex 6:"; run 'find . -name "*.log" -o -name "*.bak" | sort'
echo "Ex 7:"; run 'grep -v " 200$" access.log | wc -l'
echo "Ex 8:"; run 'sed "s/10\.0\.0\./net-/" access.log | head -2'
echo "Ex 9:"; run 'printf "#!/bin/bash\necho hello from a script\n" > hi.sh; chmod +x hi.sh; ./hi.sh; stat -c "%A" hi.sh'
echo "Ex 10:"; run 'for f in access.log hi.sh; do echo "$f has $(wc -l < $f) lines"; done'
cd /; rm -rf "$work"
