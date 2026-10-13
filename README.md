# DeepNotes

Senior-level, source-backed notes on **C, DSA, Linux and L2/L3 Networking**. Static site (Astro) → GitHub Pages; the same build can later be wrapped for Android with Capacitor.

## Run

```bash
npm install
npm run dev       # http://localhost:4321
npm run build     # static output in dist/, search index via Pagefind
```

Deploy: push to `main`; `.github/workflows/deploy.yml` builds with `SITE`/`BASE` set for GitHub Pages (enable Pages → "GitHub Actions" in the repo settings).

Note: Astro 5 is pinned because the local Node is 20; CI uses Node 22.

## Writing a note

Create `src/content/notes/<track>/<NN-module>/<NN-note>.md` (`track`: `c | dsa | linux | networking`). The numeric prefixes only control order; URLs drop them.

```yaml
---
title: ...
track: networking
module: Layer 2          # module title (same string in every note of the module)
moduleOrder: 2
order: 1                 # position in the module
level: intermediate      # intermediate | advanced | expert
summary: One sentence.
sources:                 # primary sources first (RFC / IEEE / ISO / POSIX / kernel docs)
  - label: RFC 826 — ARP
    url: https://www.rfc-editor.org/rfc/rfc826
---
```

Conventions:

1. Start from the standard's mechanism and explain *why* it is designed that way.
2. Tables for field layouts and rules; `mermaid` code fences for flows; real, compiled code and real CLI output.
3. Call out pitfalls and failure modes, and show how to observe the behavior (Linux commands, Wireshark filters).
4. End with **Key takeaways**. Cite exact clauses/sections in `sources`.

## Tracks and status

| Track | Notes | Topic map |
|---|---|---|
| Networking (L2, L3 and related) | 53 | `docs/networking-outline.md` |
| Linux | 35 | `docs/linux-outline.md` |
| C programming | 29 | `docs/c-outline.md` |
| Data structures & algorithms (in C) | 31 | `docs/dsa-outline.md` |

## Authoring tools

The C and DSA notes are assembled from tested sources; see [`authoring/README.md`](authoring/README.md).

## Quality checks

`npm run build` first runs `scripts/check-notes.mjs --fix`, which validates every note's front matter and quotes source labels (a colon inside an unquoted YAML label breaks parsing).
