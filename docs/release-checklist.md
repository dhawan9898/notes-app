# Release checklist: what the build cannot check

The automated gates (`npm run check`, build, `check:dist`, `test:ui`, `starters`, `practice:verify`) cover content consistency, links, accessibility basics and behaviour in jsdom. These need a person with a browser or phone before a release.

## Browser (desktop, Chrome and Firefox, light and dark)
- [ ] Home, Learn, a track, a path, a lesson, a lab, Practice, a problem (code and question), Playground, You, Search, Glossary, About, 404: layout, spacing, contrast.
- [ ] Mermaid diagrams render in a few lessons (check Networking and Linux).
- [ ] Code editor: typed text and highlighting stay aligned while scrolling long code, with and without line wrapping off; Tab, Enter, brackets.
- [ ] Run a code problem: Wrong Answer, Compile Error, Accepted; refresh and see the saved code; Reset; submissions list.
- [ ] Width 320, 390, 768, 1024, 1440: no horizontal page scroll, tables scroll inside their block.
- [ ] Keyboard only: tab order, visible focus, `/` opens search, `M` toggles completion.
- [ ] Offline: load a few pages, go offline, reload them and run a problem.

## Phone and app (Android)
- [ ] Tab bar, safe areas (notch, gesture bar), back gesture returns to the previous page; home closes the app.
- [ ] Problem page: soft keyboard never covers the editor or the Run/Submit bar; the symbol row works; Description/Code/Result switching.
- [ ] Glass look on the top bar, tab bar, cards; scrolling stays smooth on a mid-range device.
- [ ] Backup export and import (file picker) work in the WebView.
- [ ] First Run of the compiler works offline; install over an older APK keeps progress.

## Content
- [ ] Spot-check standards claims in the lessons you touched against the cited documents (`docs/fact-check-log.md`).
- [ ] Source links alive (`python3 scripts/check-links.py`).
