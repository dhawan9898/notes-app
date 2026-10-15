#!/usr/bin/env python3
"""Assemble notes from templates, embedding tested source files and real command output.

Directives (each on its own line in authoring/templates/<track>/<module>/<note>.md):
  {{include c/dir/file.c}}              -> fenced code block (language from extension)
  {{include path/to/file.c 10-30}}        -> only lines 10-30
  {{run c/dir :: command}}       -> "$ command" + its stdout/stderr (run in that dir, 300 s timeout)
Output goes to src/content/notes/<track>/<module>/<note>.md, or to src/content/labs/<track>/<module>/<note>.md for
templates under authoring/templates/<track>/labs/.
"""
import re, shutil, subprocess, sys, pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
TPL = ROOT / "authoring" / "templates"
OUT = ROOT / "src" / "content" / "notes"
LABS = ROOT / "src" / "content" / "labs"
LANG = {".c": "c", ".h": "c", ".py": "python", ".sh": "bash", ".mk": "make", ".txt": "text", ".S": "asm", ".s": "asm", ".ld": "text"}

def include(arg):
    parts = arg.split()
    p = ROOT / "authoring" / "examples" / parts[0]
    text = p.read_text().rstrip("\n").split("\n")
    if len(parts) > 1:
        a, b = parts[1].split("-")
        text = text[int(a) - 1:int(b)]
    return "```" + LANG.get(p.suffix, "text") + "\n" + "\n".join(text) + "\n```"

def run(arg):
    where, cmd = [x.strip() for x in arg.split("::", 1)]
    r = subprocess.run(cmd, shell=True, cwd=ROOT / "authoring" / "examples" / where, capture_output=True, text=True, timeout=300, executable=shutil.which("bash") or "/bin/bash")
    out = (r.stdout + r.stderr).rstrip("\n")
    return "```text\n$ " + cmd + "\n" + out + "\n```"

def render(src):
    def sub(m):
        kind, arg = m.group(1), m.group(2).strip()
        return include(arg) if kind == "include" else run(arg)
    return re.sub(r"^\{\{(include|run) (.+?)\}\}$", sub, src, flags=re.M)

def main():
    only = sys.argv[1:]
    n = 0
    for tpl in sorted(TPL.rglob("*.md")):
        rel = tpl.relative_to(TPL)
        if only and not any(o in str(rel) for o in only):
            continue
        parts = rel.parts
        dst = LABS / parts[0] / pathlib.Path(*parts[2:]) if len(parts) > 2 and parts[1] == "labs" else OUT / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_text(render(tpl.read_text()))
        n += 1
        print("built", rel)
    print(f"{n} note(s) built")

if __name__ == "__main__":
    main()
