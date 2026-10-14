---
title: "Users, Groups and Permissions in Practice"
track: linux
module: Linux Essentials
moduleOrder: 0.5
order: 1
level: beginner
summary: "Reading ls -l, what rwx means for files versus directories, changing access with chmod (symbolic and octal), umask, the executable bit, groups and shared directories, and using sudo safely, with real command output, before the deeper ACL and capabilities note."
prerequisites:
  - Linux Command Line Basics
sources:
  - label: "Shotts — The Linux Command Line, Ch. 9 (Permissions)"
    url: https://linuxcommand.org/tlcl.php
  - label: "man 1 chmod, man 1 chown, man 2 umask, man 5 sudoers"
    url: https://man7.org/linux/man-pages/man1/chmod.1.html
  - label: "POSIX.1-2017 — File Permissions (XBD §4.5 and chmod utility)"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/chmod.html
  - label: "GNU Coreutils manual — File permissions"
    url: https://www.gnu.org/software/coreutils/manual/html_node/File-permissions.html
---

Linux is a multi-user system: every file belongs to one user and one group, and every file carries a small set of **permission bits** that say who may read it, change it or run it. Almost every "Permission denied" you will ever see comes from this simple table. This note makes the table second nature. (The later note *Users, Permissions, ACLs and Capabilities* covers the kernel's full model, ACLs and capabilities.)

## Reading `ls -l`

```text
$ touch report.txt && ls -l report.txt
-rw-r--r-- 1 alice staff 0 Oct 14 10:44 report.txt
```

Break the line into fields: `-rw-r--r--` is the **mode**, `1` the link count, `alice` the **owner**, `staff` the **group**, `0` the size in bytes, then the modification time and the name. The ten-character mode is read as `-` `rw-` `r--` `r--`:

| Position | Meaning |
|---|---|
| 1st character | type: `-` regular file, `d` directory, `l` symbolic link |
| next three | permissions for the **owner** (user, `u`) |
| next three | permissions for the **group** (`g`) |
| last three | permissions for **everyone else** (others, `o`) |

Each triple is `r` (read), `w` (write), `x` (execute), or `-` for "not allowed". Linux checks, in order: are you the owner? then the owner bits decide. Otherwise, are you in the file's group? then the group bits decide. Otherwise the "others" bits decide. The first matching class wins, even if a later class would be more generous.

**What r, w and x mean depends on what the item is:**

| Bit | On a **file** | On a **directory** |
|---|---|---|
| `r` | read the contents | list the names inside (`ls`) |
| `w` | change the contents | create, delete and rename entries inside (even files you do not own!) |
| `x` | run it as a program | enter it (`cd`) and reach items inside by name |

The directory rules surprise beginners: to open `~/project/notes.txt` you need `x` on every directory along the path, and deleting a file needs `w` on the *directory*, not on the file.

## Changing permissions with `chmod`

`chmod` has two notations. **Symbolic**: who (`u`, `g`, `o`, `a`), an operator (`+`, `-`, `=`) and the bits.

```text
$ chmod 640 report.txt && ls -l report.txt
-rw-r----- 1 alice staff 0 Oct 14 10:44 report.txt
$ chmod g+w,o-r report.txt; ls -l report.txt
-rw-rw---- 1 alice staff 0 Oct 14 10:44 report.txt
$ stat -c "%A  octal %a  owner %U  group %G  %n" report.txt
-rw-rw----  octal 660  owner alice  group staff  report.txt
```

**Octal**: each triple is one digit made by adding `r`=4, `w`=2, `x`=1. So `rw-` is 6, `r--` is 4, `rwx` is 7, and `rw-rw----` is `660`. Memorise a few patterns:

| Octal | Symbolic | Typical use |
|---|---|---|
| `644` | `rw-r--r--` | ordinary file, others may read |
| `600` | `rw-------` | private file (SSH keys, secrets) |
| `755` | `rwxr-xr-x` | program or directory everyone may use |
| `700` | `rwx------` | private directory |
| `664` / `775` | `rw-rw-r--` / `rwxrwxr-x` | shared with a group |

Avoid `chmod 777` ("everyone may do everything"): it is almost never the real fix and is a security hole. Use `chmod -R` (recursive) carefully; it applies the same bits to files and directories, which usually want different ones (a handy trick is the capital `X`: `chmod -R u+rwX,go+rX dir` adds execute only to directories and to files that already have it).

## The executable bit

A script is not runnable until it has `x`. This is also why `./hello.sh` says "Permission denied" the first time:

```text
$ printf '#!/bin/bash\necho hello from a script\n' > hello.sh; ./hello.sh
bash: ./hello.sh: Permission denied
$ chmod +x hello.sh && ./hello.sh
hello from a script
```

The first line `#!/bin/bash` (the **shebang**) tells the kernel which interpreter runs the file. `./` is needed because the current directory is deliberately not on your `PATH`.

## Directories and the "no access" cases

```text
$ mkdir secret && chmod 700 secret && ls -ld secret
drwx------ 2 alice staff 40 Oct 14 10:44 secret
$ echo data > locked.txt && chmod 000 locked.txt && cat locked.txt
cat: locked.txt: Permission denied
$ cat /etc/shadow
cat: /etc/shadow: Permission denied
```

`ls -ld` shows the directory itself rather than its contents. `/etc/shadow` holds password hashes, so only root and the `shadow` group may read it. Note that being the **owner** does not override mode `000`: you must first `chmod` it back (the owner can always change the mode).

## umask: the default for new files

When a program creates a file it asks for mode `666` (files) or `777` (directories), and the **umask** removes bits from that request. The common umask `022` removes write for group and others, giving `644` files and `755` directories. A stricter `077` yields private files:

```text
$ umask
0022
$ (umask 077; touch private.txt; ls -l private.txt); (umask 022; touch normal.txt; ls -l normal.txt)
-rw------- 1 alice staff 0 Oct 14 10:44 private.txt
-rw-r--r-- 1 alice staff 0 Oct 14 10:44 normal.txt
```

Setting `umask` inside a subshell `( ... )` affects only that subshell. To change it for all your sessions, put the `umask` line in `~/.profile`.

## Who am I, and what groups am I in?

```text
$ id -un; id -gn
alice
staff
```

`id` shows your user name, numeric ID (UID) and every group (`id` alone prints them all). Linux works with **numbers** (UID, GID) internally; names are a lookup in `/etc/passwd` and `/etc/group`. Use `chown user:group file` to change a file's owner and group (only root can give a file to another user; an owner may `chgrp` a file to any group they belong to).

### Sharing with a group

To let a team edit files together, create a group, add people to it, and make a directory owned by that group with the **setgid** bit (`2` in the leading digit) so new files inherit the directory's group instead of each creator's:

```text
$ mkdir -m 2775 team && ls -ld team
drwxrwsr-x 2 alice staff 40 Oct 14 10:44 team
```

The `s` in the group triple is setgid. Typical administration commands (they need root, so they are shown without output):

```bash
sudo groupadd project                 # create a group
sudo usermod -aG project bob          # add bob to it (-a is essential: without it, bob LEAVES his other groups)
sudo chgrp project team && sudo chmod 2775 team
```

Group membership is read at login, so `bob` must log out and in (or run `newgrp project`) before it applies. The `/tmp` directory shows another special bit: `drwxrwxrwt` ends in `t`, the **sticky bit**, which means everyone may create files there but only a file's owner may delete it.

## sudo: borrowing root, carefully

`root` (UID 0) bypasses permission checks. Day-to-day you work as an ordinary user and prefix individual commands with `sudo` ("run this one command as root"), which asks for **your** password and logs the command:

```bash
sudo apt update              # administrative tasks: install software, edit /etc, manage services
sudo -l                      # list what you are allowed to run
sudoedit /etc/hosts          # edit a protected file safely
```

Rules of thumb: never run a whole session as root; never `sudo` a command you do not understand (especially anything copied from the web that pipes into a shell); and think about **paths** before `sudo rm -rf`: a typo there is not recoverable.

## Troubleshooting "Permission denied"

1. `ls -l file` and `ls -ld .` : read the owner, group and bits.
2. `id` : which user and groups are *you*?
3. Check `x` on **every directory in the path** (`namei -l /path/to/file` prints each component's mode).
4. Remember scripts need `x`, and files on some filesystems (USB sticks, network shares, `noexec` mounts) are governed by mount options instead of these bits.
5. If SELinux or AppArmor is enabled, they can deny access even when the mode allows it; see *SELinux, AppArmor, Landlock and System Hardening*.

## Key takeaways

1. Every file has an owner, a group and three permission triples (user, group, others); the first matching class decides.
2. `r`, `w`, `x` mean different things on directories: list, create/delete entries, and enter.
3. `chmod` accepts symbolic (`g+w`) and octal (`640`) forms (r=4, w=2, x=1); avoid `777`.
4. `umask` subtracts bits from the default modes of new files; setgid directories share a group; the sticky bit protects `/tmp`.
5. Use `sudo` for single commands rather than working as root, and debug denials with `ls -l`, `id` and `namei -l`.
