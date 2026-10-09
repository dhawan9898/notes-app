---
title: Users, Permissions, ACLs and Capabilities
track: linux
module: Boot and Foundations
moduleOrder: 1
order: 3
level: intermediate
summary: The Linux access-control model from UIDs and mode bits through setuid, ACLs and extended attributes to capabilities — precisely how a permission check is evaluated and how to grant least privilege.
sources:
  - label: "POSIX.1-2017 — File access permissions, access(), chmod(), umask"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/
  - label: "man 7 credentials, man 7 capabilities, man 5 acl, man 7 xattr"
    url: https://man7.org/linux/man-pages/man7/capabilities.7.html
  - label: "man 5 passwd, man 5 shadow, man 5 nsswitch.conf, man 8 pam"
    url: https://man7.org/linux/man-pages/man5/passwd.5.html
  - label: "Linux kernel docs — Credentials in Linux"
    url: https://docs.kernel.org/security/credentials.html
  - label: "POSIX.1e draft (withdrawn) — ACL and capability model basis"
---

## Identity: users and groups

A process runs with numeric identities; names are only labels resolved from databases:

- `/etc/passwd`: `name:x:UID:GID:comment:home:shell`
- `/etc/shadow`: password hashes, aging (root-readable only)
- `/etc/group`, `/etc/gshadow`: groups and members
- **NSS** (`/etc/nsswitch.conf`) decides *where* names come from: files, `sss` (SSSD/LDAP/AD), `systemd`... **PAM** (`/etc/pam.d/`) decides how users *authenticate* and session setup.

UID 0 is root; UIDs < 1000 (varies: 500 on old RHEL) are conventionally system accounts; ≥ 1000 regular users. `id` prints UID, primary GID, supplementary groups.

### Process credentials

Each process carries (see `/proc/PID/status`):

| Credential | Purpose |
|---|---|
| **Real** UID/GID | who started the process |
| **Effective** UID/GID | used for **permission checks** |
| **Saved** set-UID/GID | lets a privileged process drop and later regain privilege |
| **Filesystem** UID | used for file access (mostly equals effective) |
| **Supplementary** groups | additional group memberships |

`setuid` programs (e.g. `passwd`, `sudo`) start with effective UID = file owner while real UID stays the caller; well-written ones do the privileged step and drop privileges immediately.

## The permission mode

Each inode stores 12 mode bits: `rwx` for **owner**, **group**, **other**, plus **setuid**, **setgid**, **sticky**.

| | File | Directory |
|---|---|---|
| `r` | read contents | list names (`ls`) |
| `w` | modify contents | create, delete, rename entries (needs `x` too) |
| `x` | execute (also needs `r` for scripts) | **traverse**: access entries by name |

Directory semantics matter: a user can `cat /d/file` with `x` but no `r` on `/d` (if they know the name); can't `ls` it. Deleting a file needs `w`+`x` on its **directory**, not write permission on the file itself.

Octal: `r=4 w=2 x=1`; `chmod 750 f` = `rwxr-x---`. Special bits as a fourth leading digit: `4` setuid, `2` setgid, `1` sticky (`chmod 4755`, `chmod 1777 /tmp`).

| Bit | On a file | On a directory |
|---|---|---|
| **setuid** (`s` in owner x) | run as the file owner | ignored on Linux |
| **setgid** (`s` in group x) | run with the file's group | new files **inherit the directory's group** (shared project dirs); new subdirs inherit setgid |
| **sticky** (`t`) | (ignored) | only the file's owner (or dir owner / root) can delete or rename entries: `/tmp` |

Setuid on **scripts** is ignored by Linux (security), and `nosuid` mount option disables setuid/setgid and file capabilities on a filesystem.

### The check algorithm

For a process with effective UID `u` and groups `G` accessing an inode with owner `o`, group `g`:

1. If `u == 0` (or it holds the right capability, e.g. `CAP_DAC_OVERRIDE`): allowed (except execute needs at least one `x` bit).
2. Else if `u == o`: use the **owner** bits — and *stop*, even if group/other would be more permissive.
3. Else if `g ∈ G` (and ACL group entries as below): use **group** bits.
4. Else use **other** bits.

There is no fall-through: the owner can be denied access that "others" have. On every path component, `x` on each directory is required. Also checked: read-only filesystem, `immutable`/`append-only` flags (`chattr +i`), LSM policy (SELinux/AppArmor), and mount options.

### umask

The `umask` removes bits from newly created files' requested mode: files request `666`, directories `777`; umask `022` yields `644` / `755`; `077` yields `600` / `700`. It's inherited from the parent process: set per user/service (`UMask=` in systemd units).

## POSIX ACLs

When owner/group/other isn't expressive enough, **ACLs** grant specific users or groups additional rights (needs a filesystem mounted with ACL support; default on ext4/XFS/Btrfs).

```bash
setfacl -m u:alice:rw,g:dev:rx report.txt     # modify entries
setfacl -d -m g:dev:rwx /srv/project          # DEFAULT ACL: inherited by new files in the directory
setfacl -x u:alice report.txt                 # remove an entry
getfacl report.txt
# file: report.txt
# owner: bob
# group: staff
user::rw-
user:alice:rw-
group::r--
group:dev:r-x
mask::rwx
other::r--
```

Rules: entries for named users/groups plus the **mask**, the maximum permission for named users, named groups and the owning group (`chmod g+..` on a file with an ACL changes the *mask*). A trailing `+` in `ls -l` indicates an ACL. Default ACLs only apply on directories. Tools that copy files may drop ACLs (use `cp -a`, `rsync -A`, `tar --acls`).

## Extended attributes and file flags

- **xattrs**: namespaced name/value pairs on inodes: `user.*` (user data), `security.*` (SELinux labels, file capabilities `security.capability`), `trusted.*`, `system.posix_acl_access`. Tools: `getfattr -d`, `setfattr`.
- **Attributes (ext4/XFS flags)**: `chattr +i file` (immutable: cannot be modified, deleted, renamed, linked, even by root until cleared), `+a` (append-only; good for logs), `lsattr` to view.

## Capabilities: splitting root

Historically, UID 0 meant "all powers". **Capabilities** divide root's privileges into about 40 units (e.g. `CAP_NET_BIND_SERVICE` bind ports < 1024; `CAP_NET_ADMIN` configure networking; `CAP_NET_RAW` raw sockets; `CAP_SYS_TIME` set clock; `CAP_CHOWN`; `CAP_DAC_OVERRIDE` bypass file permission checks; `CAP_SETUID`; `CAP_KILL`; `CAP_SYS_PTRACE`; `CAP_SYS_ADMIN` — the notorious catch-all, avoid granting it).

A process's capabilities are held in sets (see `grep Cap /proc/self/status`):

| Set | Meaning |
|---|---|
| **Permitted** | the upper limit of what the process may have |
| **Effective** | the ones currently in force for checks |
| **Inheritable** | may be passed across `execve` (with file inheritable caps) |
| **Bounding** | hard limit for the process tree; can only shrink |
| **Ambient** | preserved across `execve` for non-privileged programs (since 4.3) |

Root's effective set on a current kernel: `CapEff: 000001ffffffffff` (all 41 capabilities as of kernel 6.x). Decode with `capsh --decode=...`, `getpcaps PID`.

**File capabilities** let a normal binary run with specific privileges instead of setuid-root:

```bash
setcap cap_net_bind_service=+ep /usr/local/bin/myserver   # can bind :80 without root
getcap -r /usr/bin 2>/dev/null                            # audit what has file caps
```

Systemd units express the same concept declaratively: `AmbientCapabilities=CAP_NET_BIND_SERVICE`, `CapabilityBoundingSet=`, `NoNewPrivileges=yes`.

Security notes: any setuid-root or capability-bearing binary is an attack surface — audit with `find / -xdev -perm -4000 -type f`. `NoNewPrivileges` (a process flag) stops `execve` from gaining privilege (disables setuid/file caps effect), a strong default for services and containers.

## sudo, su and privilege delegation

- `su` switches user (needs the target's password). `sudo` runs a command as another user (default root) after authenticating **the caller**, governed by `/etc/sudoers` (edit with `visudo`; drop-ins in `/etc/sudoers.d/`): `alice ALL=(root) NOPASSWD: /usr/bin/systemctl restart nginx`.
- Principles: restrict by command, avoid wildcards and shell-escapes (editors, `less`, `find -exec` can spawn shells), log via sudo I/O logging and auditd, prefer capabilities or polkit rules over general root.
- `sudoedit` edits files with the caller's editor safely.

## Practical recipes

```bash
# Shared project directory: group collaboration with correct inheritance
mkdir /srv/project && chgrp devs /srv/project && chmod 2775 /srv/project
setfacl -d -m g:devs:rwx /srv/project

# Who can touch this path? (check every directory in the path)
namei -l /srv/project/data/report.txt

# Find risky permissions
find / -xdev -perm -0002 -type f ! -path '/proc/*'          # world-writable files
find / -xdev -perm -4000 -o -perm -2000 2>/dev/null          # setuid/setgid
```

## Key takeaways

- Permission checks use the *effective* IDs and take the first matching class (owner → group → other) without fall-through; directories need `x` to traverse and `w`+`x` to modify entries.
- setgid directories, the sticky bit, and `umask` shape collaboration and safety; ACLs add per-user/group rules with a mask.
- Capabilities split root into ~40 privileges; give services exactly what they need (`setcap`, `AmbientCapabilities`) and set `NoNewPrivileges`.
- Use `namei -l`, `getfacl`, `getcap`, `/proc/PID/status` and `find -perm` to diagnose and audit access.
