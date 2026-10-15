# Authoring tools

The **C** and **DSA** lessons and the C labs are generated from templates, so every listing and command output on the site comes from code that was actually compiled and run.

```text
authoring/
  templates/<track>/<module>/<note>.md        lesson source with {{include …}} and {{run …}} directives
  templates/<track>/labs/<module>/<lab>.md    lab source (written to src/content/labs)
  examples/<track>/<topic>/*                  the C sources, Makefiles and scripts the notes embed
  examples/vm/*                               programs and data for the in-browser Linux (/labs): *.c compiled for 32-bit Linux, *.sh and data copied
  examples/<track>/starters/<topic>/*.c       practice starters: stub + tests, with NAME.solution.c and optional NAME.wrong-*.c
  practice/<track>/<module>/<NN-slug>.md      practice problems (code and questions): statement, examples, hints, explanation
  build.py                                    assembles templates into src/content/{notes,labs}
```

| Directive | Effect |
|---|---|
| `{{include c/dir/file.c}}` | embeds the file as a fenced code block (`{{include c/dir/file.c 10-30}}` for a line range) |
| `{{run c/dir :: command}}` | runs `command` in `authoring/examples/c/dir` and embeds `$ command` plus its output |
| `{{vm :: cmd}}`, `{{vm :: cmd1 ;; cmd2}}` | runs the commands in one session inside the in-browser Linux (programs of `examples/vm` are in `/labs`) and embeds the transcript; `{{vm mem=1024 :: cmd}}` boots it with 1 GiB. Needs `npm run fetch-linux` |

```bash
python3 authoring/build.py                  # all templates (needs gcc, g++, make, cmake)
python3 authoring/build.py strings dynamic  # only templates whose path contains one of the words
npm run validate                            # content gate
```

Rebuild only the note you edit: a full rebuild re-measures every timing and can make prose that quotes ratios stale. Compiled binaries are git-ignored (`examples/.gitignore`). Some outputs (timings, addresses, random tokens) differ between runs; the text describes the shape of the result, not exact numbers.

## Adding a code problem

1. Write `examples/<track>/starters/<topic>/NAME.c` (the function stub between `/* ===== Your code` and `/* ===== End of your code ===== */`, then tests printing `PASS …` / `FAIL label: expected X, got Y` and a `N of M checks passed` summary) and `NAME.solution.c` (the region's reference solution). Add `NAME.wrong-*.c` for plausible wrong answers the tests must reject.
2. Write `practice/<track>/<module>/NN-slug.md` with `kind: code`, `starter: <track>/starters/…/NAME.c`, `examples`, `constraints`, `hints`, then the statement and an `## Explanation`.
3. `npm run starters`, `npm run data`, `npm run practice:verify`.

Questions need only the Markdown file (`kind: question`, statement, `## Explanation`).
