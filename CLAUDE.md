# CLAUDE.md — DeepNotes project context

Hand-off document for any Claude Code session in this repository. Read it before changing anything. The product and platform design is in `docs/design.md`; this file is the working manual.

## 1. What this is

**DeepNotes** is a static learning platform for one author today, built to be released to anyone from students to veterans: source-backed lessons, practice problems and labs on **C, data structures and algorithms (in C), Linux and networking (L2/L3)**. Static site on GitHub Pages (`dhawan9898/notes-app`, base path `/notes-app`), wrapped for Android with Capacitor. No server, no accounts: progress and saved code live in the browser's `localStorage` (and the app's WebView).

## 2. Content model (three kinds, one home each)

| Kind | Where | Notes |
|---|---|---|
| **Lesson** | `src/content/notes/<track>/<NN-module>/<NN-name>.md` | Explanation only. Must end with `## Key takeaways`. No exercises or answers. Front matter: `title, track, module, moduleOrder, order, level, summary, prerequisites[], sources[]` (quote `title`/`summary` that contain `: `). `module` is the same string in every lesson of a module; file number = `order`. |
| **Lab** | `src/content/labs/<track>/<NN-module>/<NN-name>.md` | Guided hands-on session; same front matter plus `minutes`. `module` names an existing lesson module. Needs ≥ 1 lesson prerequisite. |
| **Problem** | `authoring/practice/<track>/<NN-module>/<NN-slug>.md` | `kind: code` (a starter program under `authoring/examples/<track>/starters/**` with `.solution.c`; front matter has `starter`, `examples`, `constraints`, `hints`) or `kind: question` (statement, then `## Explanation` with the answer). Body = statement; text after `## Explanation` = explanation. |

Modules are derived from lessons; stages derive from `moduleOrder` (0 getting started, below 1 essentials, 1+ core). Learning paths are `src/data/paths.json` (ordered `{track, module}` lists). Glossary: `src/data/glossary.json` (`see` must be a lesson title). Old ids that were renamed are mapped in `src/data/id-aliases.json` so saved progress survives.

C and DSA lessons and the C labs are generated from `authoring/templates/<track>/…` (labs under `authoring/templates/<track>/labs/…`) by `python3 authoring/build.py [substring…]`, embedding tested sources and real command output. Edit the template, then rebuild only that note (a full rebuild re-measures timings and makes quoted ratios stale). Linux and networking notes are written directly in `src/content`.

## 3. Repository layout

```text
astro.config.mjs              base/site from env SITE and BASE; rehype: glossary links, runnable C blocks, shiki compaction
src/content.config.ts         types for the notes and labs collections (validate.mjs holds the real rules)
src/lib/content.ts            the one content API for pages: tracks, lessons, labs, problems, modules, paths, urls
src/layouts/Base.astro        shell: top bar (desktop), tab bar (phone/app), theme init, service worker
src/components/               Icon, Ring, TrackCard, PathCard, LessonRow, ProblemRow, PracticeNav, ReadingPage (lessons and labs)
src/pages/                    index, learn, [track], [track]/[...slug] (lesson), paths, practice (hub, problems, labs, playground), you, search, glossary, about, 404
src/scripts/                  shell.ts (every page), meters.ts (declarative progress), lesson.ts, problem.ts, practice-list.ts, playground.ts, search.ts, you.ts, runner.ts (C compiler worker client)
src/scripts/lib/              store.ts (versioned storage), progress.ts, practice.ts, settings.ts, backup.ts, judge.ts, editor.ts, search.ts, ui.ts, manifest.ts
src/styles/                   tokens.css, base.css, components.css (shared + list pages), reading.css, problem.css
src/plugins/                  rehype-glossary, rehype-runnable, shiki-compact
src/data/                     paths.json, glossary.json, playground.json, id-aliases.json, runnable.json (validated snippets), generated/ (git-ignored)
public/                       manifest, service worker, icon, runner/ (WebAssembly clang, fetched, git-ignored)
scripts/                      validate, build-data, check-dist, lint-ui, test-lib, test-ui, verify-starters, verify-practice, validate-snippets, fetch-runner, make-app-icons, check-links.py
android/                      Capacitor 6 project
docs/                         design.md, release-checklist.md, backlog.md, fact-check-log.md
```

URL scheme: lessons `/{track}/{module}/{lesson}/` (numeric prefixes stripped), tracks `/{track}/`, labs `/practice/labs/{track}/{lab}/`, problems `/practice/{track}/{slug}/`, paths `/paths/{id}/`, plus `/learn/ /practice/ /practice/labs/ /practice/playground/ /you/ /search/ /glossary/ /about/`.

## 4. Commands

```bash
npm run validate        # content gate: front matter, lesson template, problems, paths, glossary, references (--fix re-quotes label: values)
npm run data            # generates src/data/generated/{practice,search-index,manifest}.json and public/search-index.json
npm test                # unit tests: store, progress, practice, settings, backup, judge, search
npm run lint:ui         # classes used are defined, defined classes are used, data-* hooks match
npm run build           # validate --fix, data, astro build   (SITE=https://dhawan9898.github.io BASE=/notes-app for the Pages build)
npm run check:dist      # links, anchors, h1, titles, accessible names in dist/
npm run test:ui         # jsdom + real page scripts + real compiler (needs a build)
npm run starters        # every starter fails as shipped and passes with its solution under ASan/UBSan (native cc)
npm run practice:verify # every code problem in the in-app WebAssembly compiler: starter not accepted, solution accepted
npm run snippets        # which C blocks in lessons can run in the app (src/data/runnable.json); re-run after editing C blocks
npm run check           # validate + data + test + lint:ui
```

## 5. Rules for changes

- **UI**: tokens only (no raw colours outside `tokens.css`/syntax palettes); one implementation per component; no per-page override layers. Touch targets ≥ 44 px; glass blur only on small fixed chrome (top bar, tab bar, toast, sheets). Pages run no JS beyond `shell.ts` unless they import a page script; the C compiler (≈ 58 MB) is fetched only when a reader presses Run.
- **Data hooks**: pages declare behaviour with `data-*` (documented at the top of `meters.ts`); `lint-ui` fails on mismatches.
- **Storage**: only through `src/scripts/lib/store.ts` stores (`dn:progress`, `dn:practice`, `dn:settings`, `dn:playground`, `dn:recent-searches`); change a shape → bump `version` and add a migration plus a test.
- **Lessons**: concepts first, from the standard/mechanism; tables for layouts and rules; real commands with captured output; Pitfalls only for real failure modes; 3–6 Key takeaways; sources. Don't embed machine-specific values (hostnames, UIDs, home paths, keys). Every lesson above beginner level needs a prerequisite that is the exact title of another lesson.
- **Problems**: tests must catch the classic wrong answer, not only the empty stub; check expected values with the solution; the explanation must stand alone (no "the program above").
- **Be honest about verification**: say what was run and what was not; never invent command output.

## 6. Traps already hit

- YAML: a colon+space in an unquoted value breaks the build. `validate --fix` quotes `label:`; quote `title:`/`summary:` yourself.
- Mermaid: in `sequenceDiagram` a `;` in a message is a separator; in `flowchart` quote node labels. Diagrams have never been rendered on the authoring machine.
- Notes write `<details>` as raw HTML; at rehype time it is a run of sibling `raw` nodes. After changing a rehype plugin delete `.astro` and `node_modules/.astro` (rendered Markdown is cached).
- C demos: compute values into variables before `printf`; print to stderr or flush when a crash is part of the demo; `-O1+` hides branch effects; gcov needs separate object compilation.
- `pkill -f <pattern>` in a tool command kills the tool's own shell; use `pgrep -x` + `kill`.
- The in-app compiler is LLVM 8 for wasm32: pointers and `long` are 4 bytes, no threads/sockets/signals/`mmap`/`<stdatomic.h>`; it crashes on common symbols, so `cc.js` passes `-fno-common`.

## 7. Environment notes

Authoring happens on a Debian VM or in Termux on Android. In Termux: `find`/`grep`/`rg` are broken shell wrappers (use `command grep`), `npm run` cannot find `astro` (run `node node_modules/astro/astro.js build`), there is no browser (use `npm run test:ui` and the checks above), and there is no `/bin/bash`. The Android build needs JDK 17 and the Android SDK (platform 34); `.github/workflows/android.yml` and `release.yml` build the APK in CI.

## 8. Android app and releases

Capacitor 6 wraps `dist/` (`npm run app:sync`, then `cd android && ./gradlew assembleDebug`). The app uses `adjustResize`; the service worker is not registered inside it; folder links are rewritten to `…/index.html` by `shell.ts` because the in-app server answers extension-less paths with the home page. Every push to `main` (except docs-only) computes a version from commit messages (`BREAKING CHANGE`/`#major` → major, `feat:`/`#minor` → minor, else patch), builds a signed APK, tags and publishes a release (`release.yml`; keystore `android/app/deepnotes.keystore`, or the `ANDROID_*` secrets). Pages deploy via `deploy.yml`.

## 9. Git and publishing

Commit or push only when asked; never force-push. Commits made in these sessions end with the attribution trailer given in the session. Do not put model identifiers in code, comments or notes. Do not create PRs unless asked. The refactor described in `docs/design.md` was developed on branch `refactor/platform`.

## 10. Verification status (read before trusting)

Run and passing on the authoring machine: validator, unit tests, UI lint, dist check, jsdom UI tests (including the real compiler), starters and practice verification, full static build. **Never done:** a look at the rendered site in a real browser or on a device (layout, glass look, editor overlay alignment, soft-keyboard behaviour, Mermaid rendering), and the Android build in this environment. Protocol/standard facts in networking and Linux lessons come from the author's knowledge and cited sources, not from fetching the standards: spot-check before publishing (`docs/fact-check-log.md`).
