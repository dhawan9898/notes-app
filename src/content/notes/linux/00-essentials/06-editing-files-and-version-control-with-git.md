---
title: "Editing Files and Version Control with Git"
track: linux
module: Linux Essentials
moduleOrder: 0.5
order: 6
level: beginner
summary: "Editing text in the terminal with nano and the minimum of vim, then Git from scratch: repositories, staging and commits, reading history, branches and merging, undoing mistakes, .gitignore, and sharing work through a remote, with a captured session."
prerequisites:
  - Linux Command Line Basics
sources:
  - label: "Chacon & Straub — Pro Git, 2nd ed. (free online), Ch. 1-3"
    url: https://git-scm.com/book/en/v2
  - label: "Git reference documentation"
    url: https://git-scm.com/docs
  - label: "GNU nano manual"
    url: https://www.nano-editor.org/dist/latest/nano.html
  - label: "Vim built-in tutorial (vimtutor) and user manual"
    url: https://vimhelp.org/usr_01.txt.html
---

Two skills separate someone who *uses* Linux from someone who *works* on it: editing a file without leaving the terminal, and recording changes with version control. Servers have no graphical editor, configuration files live in `/etc`, and almost every software project, including this site, is stored in Git. This note gives the minimum for each.

## Editing in the terminal

**nano** is the friendly choice: the shortcuts are listed at the bottom of the screen (`^` means Ctrl).

| Keys | Action |
|---|---|
| `nano file.txt` | open (or create) the file |
| type normally | edit |
| `Ctrl+O`, `Enter` | write out (save) |
| `Ctrl+X` | exit (asks to save if there are unsaved changes) |
| `Ctrl+W` | search |
| `Ctrl+K` / `Ctrl+U` | cut / paste a line |

For system files use `sudoedit /etc/hosts` (or `sudo nano /etc/hosts`).

**vim** is installed almost everywhere and is what you will land in when a tool opens "the editor" on a server, so learn the escape route. Vim has **modes**: in *normal* mode keys are commands; press `i` to enter *insert* mode and type text; press `Esc` to return to normal mode.

| Keys (normal mode) | Action |
|---|---|
| `i` | start typing before the cursor (insert mode); `Esc` to stop |
| `:w` then `Enter` | save |
| `:q` / `:wq` / `:q!` | quit / save and quit / quit discarding changes |
| `/word` then `Enter`, `n` | search, next match |
| `dd` / `yy` / `p` | delete (cut) a line / copy a line / paste |
| `u` / `Ctrl+R` | undo / redo |
| `gg` / `G` | first / last line |

If you are stuck in vim: press `Esc` a couple of times, type `:q!` and press `Enter`. Running `vimtutor` teaches the rest in about 30 minutes. Choose which editor programs open with `export EDITOR=nano` in `~/.bashrc`.

## Why version control?

Without it you end up with `report-final-v2-REAL.doc`. **Git** records snapshots of your project called **commits**. Each commit has an author, a message, a timestamp and a unique ID, so you can see what changed, why and when, return to any earlier state, work on a risky idea on a separate **branch**, and share work with others. Everything is stored locally in a hidden `.git` folder, so it works offline.

## First steps: a repository, staging and commits

Tell Git who you are once (use your own details):

```bash
git config --global user.name  "Alice Example"
git config --global user.email "alice@example.com"
```

Then a first session (outputs are real):

```text
$ git init -q -b main notes && cd notes && git status -sb
## No commits yet on main
$ printf "# My notes\n" > README.md && git status -s
?? README.md
$ git add README.md && git status -s
A  README.md
$ git commit -q -m "Add README" && git log --oneline
397a6c3 Add README
```

Git has three places a change can live: your **working directory** (files you edit), the **staging area** (the changes you have picked for the next commit, via `git add`) and the **repository** (committed history). `?? README.md` means "untracked"; `A` means "staged, new file". Staging lets you commit related changes together even when you have edited many files. In `git status -s`, the first column is the staging area and the second is the working directory.

Now change the file and look at the difference before committing:

```text
$ printf "second line\n" >> README.md && git diff
diff --git a/README.md b/README.md
index 96ac6d0..edd7575 100644
--- a/README.md
+++ b/README.md
@@ -1 +1,2 @@
 # My notes
+second line
$ git commit -qam "Expand README" && git log --oneline
e75344a Expand README
397a6c3 Add README
```

Lines starting with `+` were added, lines with `-` removed. `git commit -a` stages every change to *tracked* files (it does not add new files). **Write messages that say why**: "Fix crash when file is empty" beats "changes". A good habit is small, focused commits.

## Branches and merging

A **branch** is a movable label on a line of commits. Create one for each feature or experiment, so `main` always stays working:

```text
$ git switch -q -c feature/todo && echo "- buy milk" > todo.txt && git add todo.txt && git commit -qm "Add todo list" && git branch
* feature/todo
  main
$ git switch -q main && ls
README.md
$ git merge -q feature/todo && ls
README.md
todo.txt
$ git log --oneline --graph
* fe64442 Add todo list
* e75344a Expand README
* 397a6c3 Add README
```

Switching back to `main` made `todo.txt` disappear (it only exists on the other branch); merging brought it in. Here `main` had no new commits of its own, so Git simply moved the label forward (a **fast-forward** merge, which is why the graph is a straight line). When both branches changed things, Git creates a *merge commit*; if both changed the *same lines*, Git stops with a **conflict** and marks the file with `<<<<<<<`, `=======` and `>>>>>>>` around the two versions. Edit the file to the result you want, remove the markers, `git add` it and `git commit`.

## Undoing mistakes

| Situation | Command |
|---|---|
| Discard uncommitted edits to a file | `git restore file` (cannot be undone) |
| Unstage a file but keep the edits | `git restore --staged file` |
| Fix the last commit's message (not yet shared) | `git commit --amend` |
| Undo a commit that is already shared | `git revert <commit>` (makes a new commit that reverses it) |
| See where you have been | `git reflog` (a safety net: almost nothing committed is truly lost) |

Avoid `git reset --hard` and `git push --force` until you understand them: they discard work or rewrite shared history.

## Ignoring files

Build outputs, logs and secrets should not be committed. List patterns in a `.gitignore` file:

```text
$ printf '*.log\nbuild/\n' > .gitignore && touch debug.log && git status -s
?? .gitignore
```

`debug.log` is not reported because it matches `*.log`; the `.gitignore` itself is, so you can commit it. **Never commit passwords, API keys or private keys**; once they are in history they are very hard to remove, so rotate any secret that leaks.

## Sharing: remotes

A **remote** is another copy of the repository, usually on a hosting service. The commands you need:

```bash
git clone https://github.com/user/project.git   # copy a repository (creates the remote named "origin")
git remote -v                                   # list remotes
git pull                                        # fetch and merge others' commits
git push -u origin main                         # publish yours (-u remembers the branch pairing)
```

Authenticate with an SSH key (see *Network Commands and SSH for Beginners*) or the host's token mechanism. A healthy rhythm is: `git pull`, work on a branch, commit in small steps, `git push`, and open a pull request for review on the hosting service.

## Pitfalls

- **Committing large binaries or secrets**; use `.gitignore` and keep secrets out of the repository.
- **Working on `main` directly and pulling constantly** produces tangled history; use short-lived branches.
- **Vague commit messages** that make `git log` useless a month later.
- **Forgetting to `git add` new files**; `git commit -a` skips untracked files.
- **Force-pushing** a shared branch and erasing a colleague's commits.
- **Being stuck in vim** after Git opens it for a commit message: type your message, press `Esc`, then `:wq`.

## Key takeaways

1. `nano` is the easy editor; in vim, `i` inserts, `Esc` returns, `:wq` saves and quits and `:q!` escapes.
2. Git stores snapshots (commits) with messages; the cycle is edit, `git add`, `git commit`, with `git status` and `git diff` to look.
3. Branches isolate work; merging brings it back, and conflicts are resolved by editing the marked file.
4. `git restore`, `git revert` and `git reflog` undo most mistakes; avoid `reset --hard` and force-push until you understand them.
5. Use `.gitignore`, never commit secrets, and share through remotes with `clone`, `pull` and `push`.
