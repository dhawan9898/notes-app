# DeepNotes — product and platform design (v2)

Status: the target the refactor on branch `refactor/platform` builds. Written before the code; section 13 lists what was decided differently while building.

## 1. Who it is for, and what each person needs

| Reader | Arrives with | Needs | Served by |
|---|---|---|---|
| **Student** | Little or no systems background | A clear order, plain language, small steps, quick wins | Paths "Start from zero", Foundations and Essentials stages, easy problems |
| **Intern / junior** | Knows one language | Fill gaps fast, practise the tools used on the job | Paths by role, labs, medium problems |
| **Experienced engineer** | Years of practice | Depth with sources, a place to look things up, no hand-holding | Core and Advanced stages, glossary, search, expert notes |

Principles: one idea per place; the reader always knows where they are, what is next and how far they have come; the same thing looks the same everywhere; nothing blocks reading (no accounts, works offline); fast on a mid-range phone.

## 2. Content model: three kinds, each with exactly one home

| Kind | What it is | Lives in | Never contains |
|---|---|---|---|
| **Lesson** | Explanation of a concept from the standard/mechanism upward | `src/content/notes/<track>/<module>/NN-*.md` | Exercises, answers, starter code |
| **Lab** | Guided hands-on session with a real system (commands, captured output, short checks) | same collection, front matter `kind: lab`; shown only under Practice | Long concept teaching (links to lessons instead) |
| **Problem** | One exercise: **code** (judged, function-only editor) or **question** (think, then reveal the answer) | `authoring/practice/**` → `src/data/practice.json` | Anything a lesson already teaches |

Before: every module ended with a "Practice:" note that mixed paper questions, embedded starter programs and long answers, and the same starters also had problem pages. After: lessons are lessons; every exercise is a Problem page with identical structure; the 42 "Practice:" notes disappear as notes (their questions and answers become Problems, their lab-style content becomes Labs). The four "Start Here: How to use…" notes disappear too: their persona paths become the Paths feature and their "how the notes are built" text becomes the About page.

### Lesson template (fixed, enforced by the validator)

```
title · summary (one sentence)            front matter: track, module, moduleOrder, order, level, summary, prerequisites[], sources[]
[Before you read this]                    generated from prerequisites
Body                                      ## sections, tables, diagrams, listings with captured output
## Pitfalls                               optional, only real failure modes
## Key takeaways                          required, 3–6 bullets
[Practice this module]                    generated: problems and labs tagged with this module (never written by hand)
[Primary sources]                         generated from sources[]
[Continue with]                           generated from prerequisites of other notes
```

### Lab template

`## Goal` · `## Setup` (what you need, what is safe to run) · `## Steps` (numbered; each step: command, expected output, what it shows) · `## Check yourself` (questions, answers collapsed) · `## Key takeaways`. Front matter adds `minutes` and `needs` (tools or privileges).

### Problem record

`{ id, kind: code|question, track, module, order, title, difficulty, tags, statement(md), examples[], constraints[], hints[], explanation(md), code?: {prefix, starter, suffix, solution}, lesson: <note id> }`. Difficulty defaults from the lesson level (beginner→Easy, intermediate→Medium, advanced/expert→Hard) and can be overridden. Statements and explanations are Markdown rendered at build time with the same highlighter as lessons.

## 3. Information architecture

```
Home
Learn ─ Track (c · dsa · linux · networking)
          └ Stage: Foundations → Essentials → Core            (derived from moduleOrder: 0, <1, ≥1)
              └ Module ─ Lessons… ─ Practice block (problems + labs)
              └ Lesson   Lab
Paths ─ Path (curated ordered modules for a goal)
Practice ─ Problems (filters) · Labs · Problem page · Lab page · Playground
Search ─ everything: lessons, problems, labs, glossary
You ─ progress · streak · saved work · settings · data export/import
Reference ─ Glossary · About
```

Five primary destinations everywhere: **Home · Learn · Practice · Search · You**. Paths is reached from Home and Learn; Glossary and About from You and the footer.

### Paths (new, replaces the interleaved "roadmap")

A path is a goal and an ordered list of modules, defined in `src/data/paths.json`: `{ id, title, for, summary, minutes, modules: [{track, module}] }`. Initial set:

1. **Programming foundations** — C Foundations, C Essentials, DSA Foundations, DSA Essentials.
2. **Linux from zero to confident** — Linux Foundations, Essentials, Shell and Text, Boot and Foundations, Processes, systemd.
3. **Network engineer** — Networking Foundations, Essentials, Fundamentals, Layer 2, IPv4, IPv6, Transport, Routing.
4. **Systems programmer** — C core modules, Linux Processes, Memory, Kernel Interfaces.
5. **Performance and reliability** — DSA core, Linux Performance, Network Stack, Containers.

A path page shows progress (done/total, minutes left), the next lesson, and the modules with lessons. The validator checks every referenced module exists. There is no global interleaved order: "next" is always the next unfinished lesson of the current track or path.

## 4. Screens

Layout grid: 4 px spacing scale; content max-width 72ch for reading, 1120 px for lists; 16 px page gutter on phones.

**Shell.** Desktop: top bar (logo, five destinations, search field, theme, progress ring). Phone and app: slim top bar (logo, search, theme) and a bottom tab bar with the five destinations (glass, safe-area aware, current tab filled). Search opens a full-screen sheet on phones. The bar never changes between pages, so the reader never loses orientation.

**Home.** One screen: greeting line, **Continue** card (last lesson with progress, or "Start with Foundations"), four track cards (ring, next lesson), Paths strip, Practice strip (solved count, "Problem of the day" = next unsolved easy). No marketing blocks.

**Learn → Track.** Header (name, one line, ring, Continue). Stage tabs: Foundations · Essentials · Core. Each module is a card: number, title, lessons as rows (check, title, minutes, level badge), and a **Practice** row at the bottom (n problems · labs, solved count). Filters: All / To do / Done.

**Lesson.** Reading column with a sticky outline on desktop and a "Contents" sheet on phones; breadcrumb (Track › Module); meta row (level, minutes, step n of m in module); prerequisites box; body; Key takeaways; Practice block; Mark complete; prev/next cards; sources. Floating progress thin bar at the top; resume offer; code blocks have Copy and, for runnable C, Run/Edit.

**Practice hub.** Tabs: Problems · Labs · Playground. Problems: summary ring + difficulty counts, filters (track, module, difficulty, status, kind), search, list rows (state, title, tags, kind, difficulty). Labs: list with time and needs.

**Problem.** Same frame for code and question: header (back, title, difficulty, solved, prev/next); left pane tabs Description · Hints · Solution · Submissions; right pane: **code** → editor, result console; **question** → "Your notes" (saved text), Reveal answer, "I got it / Review again". Phone: Description · Work · Result segmented control, symbol row, bottom Run/Submit bar. Solution tab = explanation + reference code.

**Lab.** Lesson layout with a lab header (minutes, needs, safety note), steps, Check yourself.

**Paths.** Cards (goal, for whom, minutes, modules count, progress). Detail: modules in order with lessons.

**You.** Continue; stats (streak, hours, lessons done, problems solved, by track); saved work (drafts, submissions count); Settings (theme: system/light/dark, text size, reduce motion); Data (export, import, reset with confirmation); About links.

**Search.** Field with instant results grouped by kind; recent searches; keyboard `/` or Ctrl+K. Fully client-side over a build-time index (titles, summaries, headings, tags, glossary terms). Works offline in the app.

**Glossary.** Alphabet rail, term cards with the note to read next. **404:** search box and the five destinations.

## 5. Visual design

- **Look:** calm, light-on-white / near-black, one accent (blue), hairline borders. Liquid glass only on floating chrome (top bar, tab bar, sheets, toasts) and as a translucent tint on cards over a static soft light backdrop; no blur over large areas.
- **Tokens:** colour (surface, surface-2, text, muted, line, accent, ok, warn, bad, plus three track tints), type scale 12/14/16/18/22/28/36, radius 8/12/18/pill, elevation 1–3, motion 120/200/320 ms ease-out. Dark mode follows the system unless the reader chooses.
- **Components (one implementation each):** Button (primary, secondary, ghost, danger), Chip, Badge (level, difficulty), Card, Row, Progress (bar, ring), Tabs and Segmented control, Sheet, Toast, Callout, Empty state, Code block, Table, TabBar, TopBar.
- **Touch:** targets ≥ 44 px, `:active` press feedback, finger-following glint on cards, no hover-only affordances.
- **Motion:** only transitions of opacity, transform and colour; everything off under `prefers-reduced-motion`.
- **Accessibility:** semantic landmarks, visible focus ring, contrast ≥ 4.5:1 for text, tab/aria roles on tabs, `aria-live` for results and toasts, no information by colour alone.

## 6. Data and "backend"

There is no server and none is planned: the product is a static site plus a local data layer. "Backend" means (a) the build pipeline that turns authored sources into validated data, and (b) the in-browser storage layer. Both are designed to be strict.

**Build pipeline** (`scripts/`): `validate` (one command that fails the build on any inconsistency: front-matter schema, lesson template, prerequisites exist and come earlier, practice records complete, every starter has a solution and tests that fail as shipped, paths reference real modules, glossary links exist, internal links resolve) → `build-data` (practice.json, search-index.json, paths, counts) → `astro build`. Generated data is committed only where CI checks it is current.

**Storage layer** (`src/scripts/store.ts`): one versioned envelope `{v, data}` per key with migrations, validation on read (corrupt data falls back to empty and is kept aside as a backup), debounced writes with quota-failure handling (in-memory fallback, user-visible warning), cross-tab sync, and a single export/import format covering progress, practice and settings. Modules built on it: `progress` (lessons done, reading position, active time), `practice` (drafts, submissions, solved), `settings`. A `SyncAdapter` interface is defined (export/import today); accounts and cloud sync are out of scope.

## 7. Performance and size budget

- Pages ship no JavaScript except small per-page modules (shell ≈ 6 kB, lesson ≈ 6 kB, problem ≈ 20 kB, search ≈ 5 kB gzip); the C compiler (≈ 58 MB, WebAssembly) is fetched only when the reader first presses Run.
- One CSS bundle (target < 60 kB uncompressed, from ≈ 100 kB): tokens, base, components, pages; no override layers.
- Remove: the pagefind index and UI (replaced by the small search index), unused glass/aurora layers, dead selectors and scripts (a script lists selectors never used in markup and classes used but never defined).
- Service worker: versioned cache, app shell and visited pages cached, offline fallback page.
- Android: same static build; `adjustResize`, edge-to-edge safe areas, back gesture → history.

## 8. Quality gates

`npm run check` = validate + unit tests (store, judge, search, paths, content invariants) + CSS lint (dead/missing classes) + template-structure test + every starter compiled and judged with the in-app compiler. CI runs it on every PR and before deploy/release. Visual checks (layout, glass look, soft keyboard) need a device or browser and are listed in the release checklist in `docs/release-checklist.md`.

## 9. Migration plan (order of work)

1. **Content:** add `kind`; convert the 15 C/DSA practice notes into Problems (code + question); mark the lab-style practice notes `kind: lab`; delete the 42 practice notes and 4 intro notes (templates and generated files); move their useful text; fix prerequisites and titles that referenced them.
2. **Data layer:** schema, validator, practice/search/paths data, storage layer with tests.
3. **Frontend:** tokens and components, shell, pages (home, learn/track, lesson, lab, practice hub, problem, paths, you, search, glossary, 404, playground).
4. **Cleanup:** delete old CSS/scripts/components and the pagefind dependency; size report; docs.

## 10. Decisions and rationale

- *Why not leave labs inside lessons?* A lesson is read once; a lab is done with a terminal open. Different reading mode, different layout.
- *Why Problems for paper questions too?* One list, one progress model, one design. A question is a problem whose "editor" is a notes box.
- *Why own search instead of Pagefind?* It needs a native binary (not available on every build machine), adds megabytes to the app and cannot be tested here; a 100 kB index of titles, headings and tags answers the questions readers ask and works offline.
- *Why no interleaved global roadmap?* Five notes of C then five of DSA is not how anyone studies; goal-based Paths are.

## 11. Out of scope

Accounts, cloud sync, quizzes with scoring, spaced repetition, comments. (The storage layer leaves room for them.)

## 12. Release checklist

See `docs/release-checklist.md` (device checks the build cannot do).

## 13. Deviations while building

- **Labs are a separate collection** (`src/content/labs`) instead of `kind: lab` in the notes collection: cleaner types and routing.
- **Problems are Markdown files** (`authoring/practice/**`), not JSON/JS data: one authoring format for code problems and questions, rendered with the same highlighter as lessons.
- **Generated data is not committed** (`src/data/generated`, `public/search-index.json`); `npm run data` regenerates it before dev, build and tests.
- **Lab template**: labs keep their existing `## Exercises` / `## Answers` structure instead of a new "Check yourself" section; the validator only requires Key takeaways.
- **Header progress ring dropped**: it needed the full lesson manifest on every page. Progress lives on Home, Learn, track and You pages.
- **Per-page CSS**: `problem.css` and `reading.css` load only on the pages that use them.
- **A real Linux in the app** was added after the first release: the "no environment to run Linux commands" gap is closed with a WebAssembly PC emulator (v86) running Buildroot Linux 5.6 + BusyBox, started on demand on `/practice/terminal/`; labs say where they run (`env: browser` or `machine`). Its limits (no namespaces, bridges, `tc`, compiler) are stated on the page and in each affected lab.
- **Lab programs are C**, installed in the VM as static 32-bit executables; outputs in labs come from running them there (`{{vm}}` directive).
- **jsdom UI tests** were added because no browser is available on the authoring machine; they do not replace the release checklist.
