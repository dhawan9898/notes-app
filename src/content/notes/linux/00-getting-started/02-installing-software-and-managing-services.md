---
title: Installing Software and Managing Services
track: linux
module: Getting Started
moduleOrder: 0
order: 2
level: beginner
summary: "How software is installed and tracked by package managers (apt, dnf), how to find which package owns a file, and how to start, stop, enable and read the logs of a service with systemd, with real read-only queries you can run on any Debian-family system."
prerequisites:
  - Linux Command Line Basics
sources:
  - label: "Debian Reference — Package management"
    url: https://www.debian.org/doc/manuals/debian-reference/ch02.en.html
  - label: "apt(8), dpkg(1), dnf(8) — manual pages"
    url: https://man7.org/linux/man-pages/
  - label: "systemctl(1), journalctl(1), systemd.service(5) — systemd manual pages"
    url: https://www.freedesktop.org/software/systemd/man/latest/systemctl.html
  - label: "Filesystem Hierarchy Standard 3.0"
    url: https://refspecs.linuxfoundation.org/FHS_3.0/fhs/index.html
---

On Linux you normally do **not** download an installer from a website. Software comes from **repositories** (online collections of vetted packages) and a **package manager** installs it, keeps track of every file it put on disk, resolves **dependencies** (other packages it needs) and can upgrade or remove it cleanly. Programs that run in the background are managed as **services** by **systemd**.

## Package managers

| Family | Distributions | Low-level tool | Front end |
|---|---|---|---|
| Debian | Debian, Ubuntu, Mint | `dpkg` | `apt` |
| Red Hat | Fedora, RHEL, Rocky, Alma | `rpm` | `dnf` |
| Arch | Arch, Manjaro | `pacman` | `pacman` |
| SUSE | openSUSE | `rpm` | `zypper` |

The everyday commands (Debian/Ubuntu on the left, Fedora/RHEL on the right; the first of each needs `sudo` because it changes the system):

| Task | `apt` | `dnf` |
|---|---|---|
| Refresh the list of available packages | `sudo apt update` | (automatic) |
| Install | `sudo apt install nginx` | `sudo dnf install nginx` |
| Upgrade everything | `sudo apt upgrade` | `sudo dnf upgrade` |
| Remove (keep config) | `sudo apt remove nginx` | `sudo dnf remove nginx` |
| Remove including config | `sudo apt purge nginx` | n/a |
| Search | `apt search word` | `dnf search word` |
| Show details | `apt show nginx` | `dnf info nginx` |
| Which package owns a file? | `dpkg -S /usr/bin/ls` | `rpm -qf /usr/bin/ls` |
| Files in a package | `dpkg -L coreutils` | `rpm -ql coreutils` |

Two facts that matter: `apt update` only refreshes the *catalogue* (it installs nothing), and the repository is signed so packages cannot be altered in transit.

### Read-only queries you can run safely

These commands only read the package database, so they need no `sudo`. They show the idea: the package manager knows, for every file, which package owns it.

```text
$ dpkg -S /usr/bin/ls
coreutils: /usr/bin/ls
```

`/usr/bin/ls` belongs to the package **coreutils**. And the files that package installs:

```text
$ dpkg -L coreutils | grep -E "/bin/(ls|cp|mv|rm)$"
/usr/bin/cp
/usr/bin/ls
/usr/bin/mv
/usr/bin/rm
```

The version installed and available:

```text
$ apt-cache policy bash | head -4
bash:
  Installed: 5.2.37-2+b9
  Candidate: 5.2.37-2+b10
  Version table:
```

*Installed* is what is on this machine; *Candidate* is the version `apt upgrade` would install; the lines below list the repositories offering each version and their priority. A package version looks like `5.2.37-2+b9`: upstream version, then the distribution's revision.

### Where did it go?

Package files land in standard places: programs in `/usr/bin` and `/usr/sbin`, libraries in `/usr/lib`, configuration in `/etc`, data in `/usr/share`, state and databases in `/var/lib`, logs in `/var/log`. Do not edit files under `/usr`; edit configuration under `/etc`, and keep your own scripts in `/usr/local` or your home directory so upgrades do not overwrite them.

### Safe habits

- Install only from your distribution's repositories unless you understand the extra source: third-party repositories and `curl ... | sudo bash` run someone else's code as root.
- Read the confirmation list: `apt` tells you what it will install and remove before you answer `y`.
- Keep the system updated (`apt update && apt upgrade`) for security fixes; reboot after kernel updates.
- To build from source, install into `/usr/local` or a prefix, or better, package it.
- Other formats exist (Flatpak, Snap, AppImage, container images) that bundle their dependencies.

## Services with systemd

A **service** (daemon) is a background program: a web server, SSH server, a database. On nearly all current distributions **systemd** starts, stops, supervises and logs them. Its unit of work is a **unit file** describing what to run. This one (shown from the cron scheduler on this system) is a typical service:

```text
$ cat /usr/lib/systemd/system/cron.service
[Unit]
Description=Regular background program processing daemon
Documentation=man:cron(8)
After=remote-fs.target nss-user-lookup.target

[Service]
EnvironmentFile=-/etc/default/cron
ExecStart=/usr/sbin/cron -f $EXTRA_OPTS
IgnoreSIGPIPE=false
KillMode=process
Restart=on-failure
SyslogFacility=cron

[Install]
WantedBy=multi-user.target
```

Reading it: `[Unit]` gives a description and ordering (`After=` means start later than those units); `[Service]` says what to run (`ExecStart`), and that systemd should restart it on failure; `[Install]` says which **target** (a milestone, like `multi-user.target` = normal text-mode boot) pulls it in when enabled. Distribution files live in `/usr/lib/systemd/system/`; your own overrides go in `/etc/systemd/system/` (see *systemd — Units, Dependencies, Targets, Hardening and the Journal*).

### The commands

| Task | Command |
|---|---|
| Is it running? | `systemctl status cron` |
| Start / stop now | `sudo systemctl start cron` / `stop` |
| Restart / reload config | `sudo systemctl restart cron` / `reload` |
| Start at every boot | `sudo systemctl enable cron` (add `--now` to also start it) |
| Stop starting at boot | `sudo systemctl disable cron` |
| Show a property | `systemctl show cron -p ActiveState` |
| List running services | `systemctl list-units --type=service` |
| Show failed units | `systemctl --failed` |
| Read its logs | `journalctl -u cron` (`-f` to follow, `-b` this boot, `--since "1 hour ago"`) |

**Enabled** and **active** are different: *active* means it is running right now; *enabled* means it will start at boot. Querying both for one service:

```text
$ systemctl show cron -p Id,LoadState,ActiveState,SubState,Restart,FragmentPath
Restart=on-failure
Id=cron.service
LoadState=loaded
ActiveState=active
SubState=running
FragmentPath=/usr/lib/systemd/system/cron.service
```

```text
$ systemctl is-enabled cron
enabled
```

`LoadState=loaded` means systemd read the unit file; `ActiveState=active` and `SubState=running` that the process is up; `FragmentPath` tells you which file defined it. After you edit a unit file run `sudo systemctl daemon-reload`.

### Logs

systemd collects the output of all services in the **journal**, read with `journalctl`. Ordinary users see only their own messages unless they are in the `adm` or `systemd-journal` group. Typical use: `journalctl -u nginx -b` (this service, this boot), `journalctl -p err` (errors only), `journalctl -f` (follow live). Traditional text logs also exist under `/var/log`.

### Scheduling: cron and timers

To run something regularly, use **cron** (`crontab -e`; five time fields: minute, hour, day of month, month, day of week, then the command; `*/5 * * * *` is every five minutes) or a **systemd timer** (see *systemd Timers, cgroups v2 and Resource Control*). Remember that cron has a minimal environment: use absolute paths in jobs.

## Troubleshooting a service that will not start

1. `systemctl status name` shows the last log lines and the exit code.
2. `journalctl -u name -b` shows the full log.
3. Run the `ExecStart` command by hand as the service's user to see the real error.
4. Typical causes: a typo in the configuration, a port already in use (`ss -ltnp`), wrong permissions on a file or directory, a missing dependency (another unit not started), or SELinux/AppArmor denial (see *SELinux, AppArmor, Landlock and System Hardening*).
5. After fixing, `sudo systemctl restart name` and re-check status.

## Pitfalls

- Forgetting `apt update` before installing (stale catalogue, "package not found").
- Mixing installation methods (a distribution package plus a manual `make install` of the same program).
- Editing files in `/usr/lib/systemd/system` directly (they are overwritten on upgrade; use `systemctl edit`).
- Starting a service but not enabling it (it vanishes at reboot), or enabling but not starting.
- Opening a service to the network without a firewall rule review.
- Removing a package that other packages depend on (apt lists them; read the list).

## Key takeaways

1. A package manager installs from signed repositories, tracks every file, and resolves dependencies; `apt`/`dpkg` on Debian, `dnf`/`rpm` on Red Hat.
2. `dpkg -S file` finds the owning package; configuration lives in `/etc`, your own programs in `/usr/local`.
3. systemd runs services from unit files; `systemctl` starts, stops, enables and inspects them, `journalctl` reads logs.
4. *Active* (running now) and *enabled* (starts at boot) are independent.
5. Read the logs first when a service fails.

**Next:** the *Linux Exercises* lab, then *Users, Permissions, ACLs and Capabilities*.
