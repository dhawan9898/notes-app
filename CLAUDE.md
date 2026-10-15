# CLAUDE.md — DeepNotes project context

This file is the hand-off document for any Claude Code session working in this repository. Read it fully before changing anything. It records what the project is, every decision made so far, how to work in the repo, the current status, the traps already hit, and the detailed plan for what comes next.

---

## 1. What this project is

**DeepNotes** is a static website of senior-level, source-backed engineering notes for one author (the repo owner, who is the only user for now) on four subject areas:

1. **Networking (L2 & L3 and related technologies)**
2. **Linux**
3. **C programming**
4. **Data structures & algorithms (implemented in C)**

Why it exists: the owner could not find one place that covers these topics in depth at *senior-engineer* level; reaching that depth normally means reading RFCs, IEEE 802 standards, ISO C, POSIX and kernel docs directly. The notes distil those primary sources into one consistent, navigable site. Audience range: intermediate to veteran engineers. The owner plans to publish it eventually.

Delivery constraints decided with the owner:

- **Static site on GitHub Pages** (repo: `dhawan9898/notes-app`, so the site base path is `/notes-app`).
- **Later: port to Android.** The static build can be wrapped with Capacitor; nothing so far depends on a server.
- **No progress tracking and no accounts for now** ("stick with static pages till we get all info for notes"). Progress tracking, quizzes, accounts and sync were deliberately deferred.
- The **content is the main workload**. Depth over interactivity. Notes must be detailed, cover topics in depth, and cite primary sources.
- Look: clean, minimal, friendly; dark/light theme; search; per-note outline and prev/next navigation.

---

## 2. Technology and repository layout

**Stack:** Astro 5 (`astro@5`, `@astrojs/mdx@4`), content collections, `astro-pagefind` full-text search, Mermaid (client-side) for diagrams, Shiki for code highlighting, plain CSS with design tokens (no Tailwind). Astro 5 is pinned because the authoring machine has **Node 20** and Astro 7 needs Node 22; CI uses Node 22.

```text
.
├── CLAUDE.md                    this file
├── README.md                    run/deploy instructions, note front-matter schema, conventions, status table
├── astro.config.mjs             site/base from env SITE and BASE; mdx + pagefind integrations; Shiki dual themes
├── package.json                 scripts: dev, build (= check-notes --fix && astro build), preview, check
├── .github/workflows/deploy.yml GitHub Pages deploy on push to main (Node 22; SITE/BASE derived from the repo)
├── scripts/check-notes.mjs      validates every note's YAML front matter; --fix auto-quotes `label:` values
├── docs/                        topic maps with [x]/[ ] status per note
│   ├── networking-outline.md    (all done)
│   ├── linux-outline.md         (all done)
│   ├── c-outline.md             (all done)
│   └── dsa-outline.md           (all done)
├── src/
│   ├── content.config.ts        note schema (zod): title, track, module, moduleOrder, order, level, summary, sources[]
│   ├── lib/tracks.ts            TRACKS metadata, notesForTrack, groupByModule, noteSlug/noteHref, url()
│   ├── layouts/Base.astro       header, theme toggle, search, mermaid loader
│   ├── pages/index.astro        track cards with note counts
│   ├── pages/[track]/index.astro   per-track roadmap (modules → notes)
│   ├── pages/[track]/[...slug].astro   note page: sidebar, TOC, sources box, prev/next
│   ├── styles/global.css
│   └── content/notes/<track>/<NN-module>/<NN-note>.md   THE CONTENT (see below)
└── authoring/                   tooling for the C and DSA tracks (see section 5)
    ├── build.py                 assembles templates → src/content/notes
    ├── README.md
    ├── templates/<track>/<module>/<note>.md   note sources with {{include}}/{{run}} directives
    └── examples/<track>/<topic>/…             the tested C/Python/shell sources the notes embed
```

**URL scheme:** `/{track}/{module-slug}/{note-slug}/` — numeric prefixes (`01-`, `02-`) in directory and file names only control ordering and are stripped from URLs. Tracks: `c`, `dsa`, `linux`, `networking` (enum in `content.config.ts`; add new tracks in both that enum and `lib/tracks.ts`).

**Note front matter (all fields required except `sources`):**

```yaml
---
title: ...                  # quote it if it contains ": "
track: networking           # c | dsa | linux | networking
module: Layer 2             # the module title; the SAME string in every note of the module
moduleOrder: 2              # module position within the track
order: 1                    # note position within the module
level: intermediate         # intermediate | advanced | expert
summary: One sentence.      # no ": " unless quoted
sources:
  - label: "RFC 826 — ARP"  # primary sources first; labels are auto-quoted by scripts/check-notes.mjs
    url: https://www.rfc-editor.org/rfc/rfc826
---
```

---

## 3. Content status (as of the last commit)

Total: **122 notes** (site builds to ~127 pages: 122 notes + 4 track pages + home). All four tracks are content-complete against their outlines.

| Track | Notes | Modules (moduleOrder) |
|---|---|---|
| **Networking** | 40 | 1 Fundamentals (3) · 2 Layer 2 (9) · 3 IPv4 (6) · 4 IPv6 (3) · 5 Routing (8) · 6 Resilience (3) · 7 Overlays and VPN (4) · 8 Multicast (2) · 9 QoS (1) · 10 Linux Networking Labs (1) |
| **Linux** | 31 | 1 Boot and Foundations (3) · 2 Shell and Text Processing (2) · 3 Processes (5) · 4 Memory (2) · 5 Storage and Filesystems (4) · 6 Kernel Interfaces (3) · 7 systemd and Services (2) · 8 Containers (3) · 9 Performance and Observability (3) · 10 Network Stack (2) · 11 Security (2) |
| **C** | 23 | 1 Toolchain and Language Foundations (3) · 2 Memory and Pointers (4) · 3 Data Modeling (2) · 4 Advanced Language (4) · 5 Linking and Building (2) · 6 Debugging and Quality (3) · 7 Systems-Level C (5) |
| **DSA** | 28 | 1 Foundations (2) · 2 Linear Structures (3) · 3 Searching and Sorting (3) · 4 Hashing (2) · 5 Trees (6) · 6 Graphs (3) · 7 Strings (1) · 8 Algorithm Design (4) · 9 Systems-Flavored Structures (4); every note has tested code under `authoring/examples/dsa/` and real captured output |

Each note's exact file list is visible with `find src/content/notes -name '*.md' | sort`, and per-note status is in `docs/*-outline.md`.

### Note format (the quality bar every note must meet)

- Starts from the **standard/mechanism** (RFC, IEEE clause, ISO C section, POSIX/man page, kernel doc) and explains *why* it is designed that way.
- Tables for field layouts, rules and comparisons; Mermaid diagrams for flows (see traps in section 7); real commands and real output.
- A **pitfalls / failure modes / troubleshooting** section and a **Key takeaways** list at the end.
- A `sources` list citing RFC numbers, IEEE clauses, ISO sections, man pages, papers and books.
- Depth: roughly 2,500–6,000 words per note. The whole corpus is ~170,000+ words.

### How the content was verified (important when judging trust)

- **Networking and Linux notes** were written directly in `src/content/notes/`. Their commands and outputs were captured from real runs where possible (e.g. network namespaces, bridges, `tc`, nftables, `runc` rootless container, `unshare`, overlayfs whiteouts, SSH CA certificates, systemd units/timers, `/proc` data). Command syntax was tested in throwaway user+network namespaces (`unshare -Urn`, `unshare -Urnm`). **Protocol/standard facts (field widths, timers, defaults, clause numbers) come from the author's knowledge, not from fetching the standards** (IEEE documents are paywalled). They need a human spot-check before publishing; a few uncertain claims were removed during writing.
- **C notes** are generated from `authoring/templates/` and embed code that was actually compiled with **GCC 14.2** and run (ASan/UBSan/TSan, `-fanalyzer`, gcov, cmake/make, g++, Python ctypes). Outputs such as timings differ run to run.
- **Not run/verified in the authoring environment:** `gdb`, `clang`/libFuzzer, `valgrind`, `perf`, `strace`, `bpftrace`, kernel headers/module builds, `fio`, `-m32` builds. Notes that show these use clearly labeled command references or "illustrative" code.
- **Never visually checked:** there is no browser on the authoring machine. The rendered site, Mermaid diagrams (syntax was normalized by script, never rendered), and layout have not been seen. A human should open the site and skim every track.

---

## 4. Git and publishing status

- Local repo: `~/notes-app` (also a working copy at `~/deepnotes`, which was the original scratch project; **`~/notes-app` is the source of truth that holds commits**). Sync direction used so far: edit in `~/deepnotes`, then `rsync -a --delete --exclude node_modules --exclude dist --exclude .astro --exclude .git ~/deepnotes/ ~/notes-app/` then `git clean -fdXq authoring/examples` to drop ignored build artifacts, then commit in `~/notes-app`. If you work directly in `~/notes-app`, you don't need the sync (but `node_modules` must be installed there with `npm ci`/`npm i` to build).
- Remote: `https://github.com/dhawan9898/notes-app` was an **empty repository**. A minimal `Initial commit` (README + .gitignore) exists locally on `main`. All work is on branch **`add-deepnotes-site`** (commits: site + networking notes; then Linux + C + authoring tools; then this file).
- **Nothing has been pushed.** The Claude Code remote session has no GitHub write credentials (`could not read Username for 'https://github.com'`) and no `gh` CLI. The user said they cannot use a terminal right now and will push/commit later. A bundle of both branches is at `~/deepnotes-pr.bundle` (`git fetch ~/deepnotes-pr.bundle main add-deepnotes-site:add-deepnotes-site` restores them elsewhere).
- To finish publishing (manual steps for the owner): push `main`, push `add-deepnotes-site`, open a PR (or merge) so `main` contains the site, then in GitHub Settings → Pages choose **GitHub Actions** as the source. The workflow `.github/workflows/deploy.yml` builds with `SITE=https://<owner>.github.io` and `BASE=/<repo>` and deploys `dist/`.
- Commit conventions: the attribution trailer `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>` is added to commits made in these sessions. Do **not** put model identifiers in code comments, notes, or other repo artifacts. Do not create PRs unless asked.

---

## 5. The authoring pipeline (C and DSA tracks)

The C track (and the planned DSA track) is built from templates so that every listing and output on the site is real:

1. Put sources in `authoring/examples/<track>/<topic>/` (C files, Makefile, scripts). Compile and run them; iterate until correct and warning-clean (`-Wall -Wextra`; ASan/UBSan where relevant).
2. Write the note as `authoring/templates/<track>/<NN-module>/<NN-note>.md` with the normal front matter and these directives, one per line:
   - `{{include c/dir/file.c}}` or `{{include c/dir/file.c 10-30}}` — fenced code block (language from extension; `.map` etc. render as `text`).
   - `{{run c/dir :: shell command}}` — runs the command in `authoring/examples/c/dir` (bash, 60 s timeout) and embeds `$ command` plus combined stdout/stderr.
3. `python3 authoring/build.py [path-substring…]` renders templates into `src/content/notes/…`. Runs are sequential within one template, so a later `{{run}}` may use artifacts built by an earlier one. For the DSA track, directory names under `examples/` use `dsa/<topic>`; the include/run paths are relative to `authoring/examples/` (e.g. `{{include dsa/sorting/quicksort.c}}`).
4. Run `node scripts/check-notes.mjs --fix` (or `npm run build`), then `npm run build` to verify the whole site and the Pagefind index.
5. Update the `[x]` status in `docs/<track>-outline.md`.

Rules learned the hard way (section 7 has more): print results in separate statements (argument evaluation order is unspecified); flush or use `stderr` when a program may crash; avoid `pkill -f` in tool commands; keep demo programs deterministic (fixed seeds) or describe results qualitatively; clean artifacts before committing (`authoring/examples/.gitignore` already ignores them).

Networking/Linux notes are plain Markdown written directly under `src/content/notes/` (no templates).

---

## 6. Environment facts of the authoring machine

Debian 13 VM, kernel 6.12, 8 cores (AMD EPYC), **Node 20.19**, npm 9, GCC/G++ 14.2, glibc 2.41, Python 3.13, make, cmake 3.31, nftables, iproute2, systemd 257, OpenSSH 10, runc 1.1, busybox, getfacl, lsof, ASan/UBSan/TSan, gcov. **Missing:** gdb, clang, valgrind, perf, strace, ethtool, tcpdump, xxd, shellcheck, cppcheck, kernel headers, a web browser. Unprivileged user namespaces work (`unshare -Urnm`, rootless runc, unprivileged overlayfs); real root/sudo does not. Outbound network was not used for content.

---

## 7. Traps already hit (do not repeat)

- **YAML front matter:** a colon + space inside an unquoted value (`label: RFC 5120 — M-ISIS: Multi…`, titles, summaries) breaks parsing and the whole build. `scripts/check-notes.mjs --fix` quotes `label:` values automatically (and runs in `npm run build`), but **quote `title:` and `summary:` yourself** when they contain `: `.
- **Mermaid:** in `sequenceDiagram` a `;` inside a message is a statement separator → syntax error; in `flowchart` always quote node labels `A["text, with (parens) / slashes -> arrows"]`; avoid `<` `>` in state-diagram labels. The diagrams have never been rendered here — verify in a browser.
- **`pkill -f <pattern>`** in a tool command also matches the tool's own command line and kills the shell (happened twice). Use `pgrep -x name` + `kill PID`.
- **Argument evaluation order** (`printf("%d %d", f(), g())`) is unspecified and was bitten repeatedly in demos; compute into variables first.
- **Buffered stdout is lost on crash/`_exit`:** print demo results to `stderr` or flush when a crash is part of the demo; for `system()` demos use `setvbuf(stdout, NULL, _IONBF, 0)`.
- **glibc stdio does not call the interposable `write()` symbol**, so `LD_PRELOAD` can't count stdio writes; count via `/proc/self/io` (`syscw`).
- **GCC optimizations hide effects:** `-O1+` turns data-dependent branches into `cmov` (use `-fno-if-conversion -fno-if-conversion2` to show branch misprediction), removes unused `malloc/free`, folds `static` constants (use `-O0` for symbol-table demos), vectorizes float reductions only with `-ffast-math`.
- **gcov:** when compiling several files in one command the data files get combined names; compile objects separately.
- **mawk (Debian default awk)** rejects `--` before the filename; `ip tunnel add … mode ipip` fails in user namespaces where `ip link add … type ipip` works (notes use the latter).
- **`ldd`/Compiler output paths:** strip absolute home paths from embedded sanitizer output (`sed -e 's#/[^ ]*/##'`).
- **Personal data in output:** don't embed hostnames, UIDs, usernames or host key fingerprints from the authoring machine into notes (they were replaced with generic values).
- **Privacy of examples:** the SSH CA example uses throwaway keys generated on the spot; never embed real keys.

---

## 8. What to do next — detailed plan

### 8.1 DSA track: DONE (kept for reference)

All 28 DSA notes are written (see `docs/dsa-outline.md`, every box checked). Each was produced with the authoring pipeline: C sources in `authoring/examples/dsa/<topic>/`, templates in `authoring/templates/dsa/<NN-module>/`, brute-force/differential tests, sanitizer runs, and measured output embedded via `{{run}}`. Things learned while writing them that future notes should follow:

- Every algorithm is checked against an exhaustive or naive oracle on thousands of small random inputs; several real bugs were caught this way and are documented in the notes (Floyd-Warshall infinity plus negative edge, `(a % m + m) % m` overflow, ABA through node recycling in the Michael-Scott queue, an undersized Huffman bit buffer found by ASan, an unbounded `rand` oracle that hung).
- Timing prose must be written from the numbers in the *regenerated* note (each `build.py` run re-measures): after building, grep the output block and make sure every ratio quoted in the text is still true; use ranges and "typically" rather than single-run values.
- Run sanitizer builds for correctness (`| head -N` the output) and a separate `-O2` build for timing, as the templates do.
- Do not name the authoring hardware in the notes; describe cache effects generally.

Possible DSA follow-ups (not required): suffix arrays/automata implementation, persistent and functional structures, geometry (convex hull, sweep line), randomized algorithms (reservoir sampling, skip-list analysis), external sorting in code, a capstone "projects" module.

### 8.1a Beginner on-ramp (done in the P0 pass)

Each track now has a module 0 "Start Here" (`moduleOrder: 0`, `level: beginner`): C (5 notes: start here, C From Zero, Binary/Hex/Bits, Pointers for Beginners, exercises), Linux (4), Networking (3), DSA (3). New schema bits: level `beginner`; optional `prerequisites: [note titles]` rendered as a "Before you read this" box (entries that contain a colon must be quoted; `scripts/check-notes.mjs` validates this); `/glossary/` page driven by `src/data/glossary.json` (every `see` title must exist). Exercise notes hide answers in `<details>` and the answer programs/scripts are compiled/run via `{{run}}` (examples under `authoring/examples/<track>/beginner/`; networking/linux notes also use the pipeline for sandboxed runs: `unshare -Urnm` for namespaces).

**Caution when rebuilding:** `python3 authoring/build.py` with a broad pattern regenerates every output block, re-measuring timings, so prose that quotes ratios can go stale. Rebuild only the note you edit (`build.py <note-substring>`), or restore old output with git and re-apply only intended changes.

### 8.1a-2 Essentials modules (continuity pass)

After a learner-path review, every track got a beginner **Essentials** module (`moduleOrder: 0.5`): C (5 notes, templates in `authoring/templates/c/00-essentials`), DSA (4, `authoring/templates/dsa/00-essentials`), Linux (6) and Networking (5), the last two plain Markdown with real captured output (loopback-only or documentation addresses; no machine-specific values). The Linux module order is now Essentials, Shell and Text, Boot and Foundations. `learningPath()` builds Start here, Essentials (interleaved) and a Core path that takes five notes from each track in turn. Details and the findings behind them are in `docs/coverage-review.md` section 5. When editing front matter of a templated C/DSA note, change the template and the generated note together (or rebuild only that template).

### 8.1b Coverage review

A full coverage review (intern view and expert view) is in `docs/coverage-review.md`; its prioritised backlog (P0 beginner on-ramp, P1 expert gaps, P2 thin notes, P3 verification) supersedes the older backlog in 8.3 where they overlap.

### 8.2 Next: hardening and verification (high value, do in this order)

1. **Visual verification pass** (needs a browser; the owner or a session with Playwright/Chromium): run `npm run build && npm run preview`, open every track page and a sample from every module; check Mermaid rendering (fix syntax errors in the offending diagrams), table overflow on mobile width (320–390 px), code block horizontal scroll, dark mode contrast, TOC, search results, prev/next links, base-path correctness under `/notes-app`. Consider adding a Playwright smoke test script (`scripts/smoke.mjs`) that loads each page and fails on console errors or mermaid error nodes.
2. **Fact-check pass of the standards claims** in Networking and Linux (and the C-standard statements): check numbers/timers/defaults/clause references against the RFCs and IEEE/ISO/POSIX documents (RFC editor and kernel docs are free; IEEE 802 documents are available via "Get IEEE 802" six months after publication). Keep a `docs/fact-check-log.md` listing each verified/changed claim. Prioritize: STP/RSTP/MSTP timers and costs, OSPF/IS-IS/BGP defaults and attribute codes, IPv6 NDP details, DHCP/IPsec/EVPN numbers, QoS DSCP values, Linux sysctl defaults, systemd directive semantics, kernel version-dependent statements (EEVDF 6.6, PREEMPT_RT 6.12, `openat2` 5.6, etc.).
3. **Link checking:** verify every `sources[].url` resolves (a script with `curl -I`/`lychee`); replace dead or guessed URLs (a few were provided from memory, e.g. IEEE and some blog URLs); remove URLs that cannot be verified rather than leaving guesses.
4. **Cross-linking:** add a "Related notes" block (title-based, using real route paths) at the end of notes that reference other notes in prose (e.g. ARP ↔ IPv6 NDP ↔ L2 security; OSPF ↔ DSA shortest paths; C malloc ↔ Linux memory ↔ DSA allocators). Possibly add a generated "Related" section driven by an optional `related:` front-matter array and a small Astro component.
5. **Consistency/editorial pass:** unify terminology (e.g. "FIB/RIB"), heading levels, the tone of "Key takeaways", remove leftover phrasing such as "this host" (a sweep was done; re-grep for ` here`, "this machine"), check that all promised cross-references exist, fix any note whose front-matter `module` string differs from siblings (it creates duplicate module groups).
6. **Site polish (keep it minimal):** active-section highlight in the TOC, "copy" button on code blocks, per-note reading time and level badge, last-updated date, print stylesheet, favicon/OG metadata, a 404 page, `sitemap.xml` (add `@astrojs/sitemap`), `robots.txt`, search filters by track. Keep the design clean and low-dependency.
7. **CI hardening:** add a workflow job that runs `npm ci && npm run build` on PRs (not just deploy on `main`), runs the front-matter checker, optionally `python3 authoring/build.py` and fails if generated notes differ from committed ones (reproducibility), and a link checker on a schedule.

### 8.3 Content expansion backlog (after the above; owner to prioritize)

- **Networking additions:** Wi-Fi 802.11 basics (CSMA/CA, frame types, security WPA3), segment routing/SRv6 in depth, TCP deep-dive note (state machine, congestion control algorithms with packet traces), DNS in depth, TLS/QUIC handshake, SD-WAN/overlay design, data-center fabric design (Clos/EVPN case study), network automation (NETCONF/YANG, gNMI, Ansible, Batfish), packet capture analysis (Wireshark cookbook), OSPFv3/IS-IS lab with FRR containers (containerlab topologies in `authoring`).
- **Linux additions:** package management and build from source, Linux kernel development basics (patch workflow, kernel modules in practice with real builds), KVM/virtualization (libvirt, QEMU, virtio, SR-IOV), SELinux policy writing, bash scripting advanced patterns, `strace`/`perf` captured sessions (needs those tools installed), real-time Linux tuning, filesystem internals deep dive (ext4 on-disk layout with `debugfs`), cgroups v1 vs v2 migration, observability stack (Prometheus/Grafana/OpenTelemetry basics).
- **C additions:** C++ interop deep dive, writing a shell / HTTP server / interpreter capstone projects (each a tested multi-file project under `authoring/examples`), embedded C topics (volatile registers, linker scripts, startup code, interrupts), lock-free programming in C11 atomics (could live in DSA module 9), generic programming patterns, memory-safe subset techniques and Rust FFI.
- **New tracks to consider (owner decision):** Operating-system internals, Distributed systems, Databases, Security engineering, Python/Go systems programming. Add by extending the track enum, `TRACKS`, an outline in `docs/`, and notes.
- **Capstone projects section:** a `projects` module per track with build-it-yourself guides (mini shell, malloc, TCP echo/HTTP server with epoll, ring buffer, hash table, LRU cache, packet sniffer with raw sockets/libpcap, ARP/ICMP responder, routing-table trie) tying notes together; each with tests and a rubric.

### 8.4 Product features (deferred by the owner; do **not** start without being asked)

Progress tracking (per-note done state, module/track progress rings, "continue where you left off"), quizzes, bookmarks/notes, spaced repetition, accounts/sync, and the **Android app** (wrap the static build with Capacitor; offline support via a service worker; check Pagefind index works offline; app icons and splash). When started, begin with local-only storage (`localStorage`/IndexedDB, wrapped in try/catch) before any backend.

---

## 9. Working agreements and conventions for future sessions

- **Always build and check before declaring done:** `node scripts/check-notes.mjs --fix` and `npm run build` must pass. For C/DSA templates also run `python3 authoring/build.py <note>` and read the generated output blocks.
- **Be honest about verification:** say what was run and what was not; never invent command output. If a tool is missing, label the block as a reference/illustrative.
- **Don't expose personal data or machine-specific values** in notes (hostnames, UIDs, home paths, key material, IPs of the authoring VM).
- **Every note above beginner level needs at least one prerequisite** that is the exact title of another note (enforced by `scripts/check-notes.mjs`); choose the notes it actually builds on, cross-track where that fits.
- **Prefer extending existing notes' conventions** (module names/orders, level badges, sources format) so the site structure stays coherent. When adding a note to an existing module, set `order` correctly and update the outline `[x]`.
- **Commit/push policy:** commit only when asked (the owner asked for a commit after the C track; pushes need credentials the remote session lacks). Never force-push or rewrite the owner's history. If asked to raise a PR and credentials are missing, say so and offer the bundle.
- **Before a long writing run, state the plan** (module list), write notes in batches of 2–4, build after each batch, and keep the outline files current so work can resume after context loss.
- **Scale expectations:** a deep note takes substantial effort; prefer fewer, correct, tested notes over many shallow ones.

---

## 10. Quick command reference

```bash
npm install                                  # first time (Node ≥ 20; Astro 5)
npm run dev                                  # http://localhost:4321  (base path "/" in dev)
npm run build                                # check front matter (+fix labels) then astro build + pagefind
npm run preview                              # serve dist/
node scripts/check-notes.mjs [--fix]         # front-matter validation only
python3 authoring/build.py [substring…]      # regenerate templated C/DSA notes
SITE=https://dhawan9898.github.io BASE=/notes-app npm run build   # reproduce the Pages build locally
find src/content/notes -name '*.md' | awk -F/ '{print $4}' | sort | uniq -c       # notes per track
git log --oneline --all | head                                                      # history
git bundle create ~/deepnotes-pr.bundle main add-deepnotes-site                     # portable copy of the work
```

## 9. Learning app: roadmap, progress tracking and UI

- **Order.** Each track is ordered by `moduleOrder` then `order`. `scripts/check-notes.mjs` fails the build if a note's prerequisite (a note of the same track) comes later in that track. `src/lib/tracks.ts: learningPath()` builds the global curriculum used by `/roadmap/`: every track's *Start here* module first, then C, DSA, Linux, Networking; a note whose prerequisites (across tracks) are not placed yet is deferred until they are.
- **Progress** is stored only in the browser (`localStorage`, key `deepnotes:progress:v1`): completed note ids, last-read note, reading position per note, active reading seconds per day (for streak and hours). `src/scripts/progress.ts` is the store (tested by `npm test`); `src/scripts/app.ts` renders it. Pages declare what they need with data attributes: `data-note-id` (an element that reflects completion via `data-done`), `data-progress-group` (+ `data-ids`) with `data-progress-text|bar|ring-fg`, `data-toggle`, `data-complete`, `data-continue`, `data-filter-*`, `data-stat`. Pages that need the ordered list embed it with `<NotesData>`.
- **UI.** `src/styles/glass.css` (loaded after `global.css`) holds the liquid-glass material (translucent gradient + backdrop blur + specular edge + cursor-lit highlight), the animated aurora backdrop, the progress components and all motion. Page transitions use the cross-document View Transitions API (`@view-transition`), reveal animations use `IntersectionObserver`, and everything respects `prefers-reduced-motion`. Floating layers (`.dock`, `.toast`, search results) use the denser `--float-bg` for readability.
- **Offline/installable.** `public/manifest.webmanifest` and `public/sw.js` (network-first pages, cache-first assets). The service worker is registered from `Base.astro` relative to the manifest URL, so it works under the GitHub Pages base path.
- **Glossary links.** `src/plugins/rehype-glossary.mjs` links the first occurrence of each glossary term in beginner and intermediate notes.
- **Screenshots in headless Firefox** freeze CSS animations at the first frame; use `ui.prefersReducedMotion=1` in the profile, and load pages through a server that delays the `load` event if JS state must be visible.

## 10. In-app C runner, playground and the Android app

- **Runner.** `public/runner/cc.js` runs a real clang/LLD (LLVM 8, WebAssembly build from binji/wasm-clang) inside a Web Worker (`public/runner/worker.js`): compile → link against a wasi-libc sysroot → run with captured stdout/stderr/stdin, 20 s run limit. The ~60 MB toolchain is not in git: `npm run fetch-runner` (SHA-256 pinned) downloads it into `public/runner/`; CI workflows call it before building. Target is wasm32: pointers and `long` are 4 bytes; no threads, sockets, signals, `mmap`, `<stdatomic.h>`.
- **Which snippets get a Run button.** `npm run snippets` (`scripts/validate-snippets.mjs`) finds every complete C program (a ```c block with `main`) in the notes, resolves `#include "x.h"` against `authoring/examples`, compiles and runs it with the same toolchain in Node, and records results in `src/data/runnable.json` (cached by program hash) and header bundles in `public/runner/bundles.json`. `src/plugins/rehype-runnable.mjs` marks passing blocks (`data-runnable`); `src/scripts/runner.ts` adds Run / Edit / stdin / output UI. Re-run `npm run snippets` after editing C blocks. Quoted includes are flattened to the file's base name at run time.
- **Exercise starters.** Every practice exercise that asks for a function to be written or fixed has a *starter program* (a collapsible "Starter program with tests" block after the exercise): the function as a stub or buggy version between `/* ===== Your code: … */` and `/* ===== End of your code ===== */`, then a `main()` that prints `PASS`/`FAIL` lines and a summary. Readers press Run, Edit code and Run again. Sources: `authoring/examples/<track>/starters/<topic>/NAME.c`, with the reference solution for the marked region in `NAME.solution.c` and optional plausible wrong answers in `NAME.wrong-*.c`. `npm run starters` (`scripts/verify-starters.mjs`, also in CI) checks that each starter compiles warning-free and reports a FAIL as shipped, that the solution passes every check under ASan/UBSan, and that every wrong answer is rejected. Exercises that need threads, files, pipes or a shell have no starter (the in-app compiler cannot run them). Write tests that would catch the classic wrong answer, not only the empty stub, and check expected values with the solution before trusting them.
- **In-app compiler quirk.** The LLVM 8 wasm toolchain crashes ("unreachable") on uninitialised globals emitted as common symbols, so `public/runner/cc.js` passes `-fno-common`.
- **Playground.** `/playground/` with curated examples in `src/data/playground.json`.
- **Android.** Capacitor 6 (Node 20 compatible; Capacitor 7+ needs Node 22) wraps the static site: `capacitor.config.json`, project in `android/`. `npm run app:sync` builds the site with base `/` and copies it into the app; `cd android && ./gradlew assembleDebug` produces `app/build/outputs/apk/debug/app-debug.apk`. Needs JDK 17 and the Android SDK (platform 34, build-tools 34.0.0). `.github/workflows/android.yml` builds the APK in CI (manual run or a `v*` tag). Launcher icons come from `scripts/make-app-icons.mjs`. The service worker is not registered inside the app (assets are bundled).

### Release pipeline (`.github/workflows/release.yml`)
Every push to `main` (except docs/markdown-only changes) computes the next version from git tags, builds a signed release APK, tags the commit `vX.Y.Z` and publishes a GitHub Release with the APK and auto-generated notes. Bump rule: `BREAKING CHANGE`, `#major` or `type!:` in any commit since the last tag → major; `feat:` or `#minor` → minor; otherwise patch (first release is 1.0.0). The workflow can also be run by hand with a chosen bump. `versionCode` is the commit count (always increasing). Nothing is committed back, so there are no loops. Signing: the committed `android/app/deepnotes.keystore` (stable sideload key; same signature every build so updates install over each other); to use a private key instead, add repository secrets `ANDROID_KEYSTORE_BASE64`, `ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS`, `ANDROID_KEY_PASSWORD`. Switching keys later requires uninstalling the old app once.

## 11. Practice problems (LeetCode-style pages)

- **What.** Every exercise that has a starter program (52 today: 16 C, 36 DSA) is a problem page at `/practice/<track>/<slug>/`, listed at `/practice/`. The page has the task, examples (input/output/why), constraints and edge cases, tabs for hints (revealed one at a time), solution (behind a button) and submissions, and a **function-only editor**: the reader edits only the region between the `Your code` markers; the given code (before it) and the tests (after it) are added around it at run time. Run shows every test as a chip (expected vs got); Submit also records the verdict and saves the code. Phones get a Description / Code / Result tab layout, a symbol row above the keyboard and a Run/Submit bar; the page is a fixed-height screen that follows the visual viewport (`--pp-h`), so the keyboard never covers the editor. The Android manifest uses `adjustResize`.
- **Data flow.** `authoring/examples/<track>/starters/**` (stub + tests + `.solution.c`) and `authoring/practice/meta.mjs` (slug, title, statement, difficulty, tags, examples, constraints, hints; key = starter path) → `node scripts/build-practice.mjs` → `src/data/practice.json` (committed; `npm run build` regenerates it). To add a problem: write the starter and solution as usual, include it from a template, add its entry to `meta.mjs`, run `npm run practice`, then `npm run practice:verify`.
- **Verdicts** (`src/scripts/practice-judge.ts`, pure and tested): the starters print `PASS …` / `FAIL label: expected X, got Y` and a `N of M checks passed` summary; no summary = Runtime Error, compile phase = Compile Error (line numbers are mapped back to the editor), runner timeout = Time Limit Exceeded, zero tests never counts as Accepted.
- **Saved state** (`src/scripts/practice-store.ts`, key `deepnotes:practice:v1`, localStorage only): autosaved draft per problem (restored on reopen), the last 25 submissions with their code, solved timestamp. Submitting also sets the draft. `npm test` covers the store and judge; `npm run practice:verify` compiles every starter (must not be Accepted) and solution (must be Accepted) with the in-app WebAssembly clang and the same judge code (all 52 pass); CI runs both.
- **In the notes.** `src/plugins/rehype-practice.mjs` replaces each collapsible "Starter program with tests" block in the practice notes with a card linking to the problem page (matched by the SHA-1 of the starter code). Trap: the notes write `<details>` as raw HTML, so at the rehype stage it is a run of sibling `raw` nodes around the code block, not one element. After changing a plugin, delete `.astro` and `node_modules/.astro`: Astro caches rendered markdown.
- **Mobile glass.** The final `@media (hover: none), (pointer: coarse)` block of `glass.css` now keeps the liquid-glass look on phones and in the app: static drifting light backdrop (gradients, no blur filter), translucent gradient panels with bright rims and inner glow, a specular glint that follows the finger (`.lit`, set by `initTouchLight()` in `app.ts`), press feedback, and `backdrop-filter` blur only on the small fixed bars (header, dock, toast, practice bars). `practice.css` is imported before `glass.css` so the touch rules win.
- **Not verified here:** the pages were checked with a jsdom smoke test against the real in-app compiler (editor, run, submit, save/restore, reset, hints, solution, symbol bar), never in a real browser or on a device. Layout, the editor overlay alignment (transparent textarea over a highlighted `pre`), the soft-keyboard behaviour and the glass look still need a look on a phone or in the APK.
