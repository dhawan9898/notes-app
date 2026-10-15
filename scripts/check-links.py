#!/usr/bin/env python3
"""Check every source URL in src/content/notes and src/content/labs. Usage: scripts/check-links.py [--out docs/link-check.md]
Treats 2xx/3xx as alive. 401/403/429 (bot blocking) are reported separately as 'blocked', not as broken."""
import concurrent.futures, pathlib, re, sys, urllib.request, urllib.error, collections, datetime

ROOT = pathlib.Path(__file__).resolve().parent.parent
urls = collections.defaultdict(list)
for base in (ROOT / "src/content/notes", ROOT / "src/content/labs"):
    for f in sorted(base.rglob("*.md")):
        for m in re.finditer(r"^\s+url:\s*(https?://\S+)\s*$", f.read_text(), re.M):
            urls[m.group(1).strip('"')].append(str(f.relative_to(base.parent)))

def check(url):
    for method in ("HEAD", "GET"):
        req = urllib.request.Request(url, method=method, headers={"User-Agent": "Mozilla/5.0 (DeepNotes link checker)"})
        try:
            with urllib.request.urlopen(req, timeout=20) as r:
                return url, r.status, ""
        except urllib.error.HTTPError as e:
            if method == "HEAD" and e.code in (400, 403, 405, 501): continue      # some servers reject HEAD
            return url, e.code, e.reason
        except Exception as e:
            if method == "HEAD": continue
            return url, 0, type(e).__name__ + ": " + str(e)[:80]
    return url, 0, "unreachable"

with concurrent.futures.ThreadPoolExecutor(16) as ex:
    results = list(ex.map(check, urls))
ok = [r for r in results if 200 <= r[1] < 400]
blocked = [r for r in results if r[1] in (401, 403, 418, 429)]
bad = [r for r in results if r not in ok and r not in blocked]
out = [f"# Source link check\n\nChecked {len(results)} unique URLs on {datetime.date.today()}: {len(ok)} reachable, {len(blocked)} blocked to automated clients (401/403/418/429), {len(bad)} broken or unreachable.\n"]
for title, group in (("Broken or unreachable", bad), ("Blocked to bots (verify manually)", blocked)):
    out.append(f"\n## {title}\n")
    for url, code, why in sorted(group):
        out.append(f"- `{code}` {url} ({why}) — {', '.join(urls[url][:3])}")
text = "\n".join(out) + "\n"
dest = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else None
(pathlib.Path(dest).write_text(text) if dest else print(text))
print(f"{len(ok)} ok, {len(blocked)} blocked, {len(bad)} bad", file=sys.stderr)
