# DeepNotes

Source-backed lessons, practice problems and labs on **C, data structures and algorithms, Linux and networking**, from a first program to the kernel. A static site (Astro) for GitHub Pages that is also an offline Android app (Capacitor). No accounts: progress and saved code stay on your device.

- **Learn**: four tracks (Getting started → Essentials → Core modules) and five goal-based learning paths.
- **Practice**: code problems run in the browser with a real C compiler (WebAssembly), questions with answers, guided labs and a playground. Your code is saved when you submit.
- **You**: progress, streak, settings, backup and restore.
- **Search**: instant and offline over lessons, labs, problems and the glossary.

## Run it

```bash
npm install
npm run fetch-runner   # downloads the in-browser C compiler (about 58 MB, pinned by SHA-256)
npm run dev            # http://localhost:4321
npm run build          # validates content, generates data, builds dist/
```

Deploy: push to `main`; `.github/workflows/deploy.yml` builds with the Pages `SITE`/`BASE` and publishes. Android: `npm run app:sync` then `cd android && ./gradlew assembleDebug` (JDK 17, Android SDK 34), or take the APK from the latest release.

Node 20 or newer; Astro 5 and Capacitor 6 are pinned for Node 20 compatibility (CI uses Node 22).

## Add content

Lessons, labs and problems are Markdown files with front matter; `npm run validate` explains what is wrong. The formats, the rules and the commands are in [`CLAUDE.md`](CLAUDE.md); the product design is in [`docs/design.md`](docs/design.md); the C and DSA authoring pipeline is in [`authoring/README.md`](authoring/README.md).

## Quality gates

`npm run check` (content, data, unit tests, UI lint) · `npm run build && npm run check:dist && npm run test:ui` (built pages and behaviour) · `npm run starters` and `npm run practice:verify` (every exercise compiles, fails as shipped, passes with its solution). CI runs all of them on pull requests.
