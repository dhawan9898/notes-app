---
title: systemd — Units, Dependencies, Targets, Hardening and the Journal
track: linux
module: systemd and Services
moduleOrder: 7
order: 1
level: intermediate
summary: How systemd models a system as units with dependency and ordering relations, writing correct service files (types, restart, kill behavior), socket activation, sandboxing directives and scoring, and using journald and journalctl effectively.
sources:
  - label: "systemd.unit(5), systemd.service(5), systemd.exec(5), systemd.kill(5), systemd.special(7)"
    url: https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html
  - label: "systemd.socket(5), systemd.target(5), systemd.resource-control(5), systemd.directives(7)"
    url: https://www.freedesktop.org/software/systemd/man/latest/systemd.socket.html
  - label: "journalctl(1), journald.conf(5), systemd.journal-fields(7)"
    url: https://www.freedesktop.org/software/systemd/man/latest/journalctl.html
  - label: "systemd-analyze(1) — verify, security, critical-chain"
    url: https://www.freedesktop.org/software/systemd/man/latest/systemd-analyze.html
  - label: "Poettering — Rethinking PID 1; systemd for Administrators (blog series)"
    url: http://0pointer.de/blog/projects/systemd.html
---

**systemd** is PID 1 on nearly every mainstream distribution. It starts the system, supervises services, tracks every process in a **cgroup**, activates services on demand, and bundles logging, timers, login sessions, networking, DNS and more. Its core idea: the system is a graph of **units** with explicit **dependencies**, started in parallel as far as the graph allows.

## Unit types

| Suffix | Purpose |
|---|---|
| `.service` | a daemon or a one-shot command |
| `.socket` | a listening socket that activates a service on first connection |
| `.timer` | schedule that activates another unit (cron replacement) |
| `.target` | a named synchronization point / group (like runlevels) |
| `.mount`, `.automount`, `.swap` | filesystems and swap (generated from `/etc/fstab` by `systemd-fstab-generator`) |
| `.path` | activate a unit when a file or directory changes |
| `.device` | kernel devices exposed by udev |
| `.slice` | node in the cgroup hierarchy for resource control |
| `.scope` | externally created processes (user sessions, containers) |

Unit files are searched in (highest precedence first): `/etc/systemd/system/` (admin), `/run/systemd/system/` (runtime), `/usr/lib/systemd/system/` (packages). Never edit packaged files: use **drop-ins**, `systemctl edit nginx` writes `/etc/systemd/system/nginx.service.d/override.conf`, and `systemctl edit --full` makes a complete copy.

```bash
systemctl status ssh              # state, cgroup tree, recent logs
systemctl cat ssh                 # the effective unit including drop-ins
systemctl show ssh -p Restart,TimeoutStopUSec,Type    # resolved properties
systemctl list-units --failed ; systemctl list-unit-files --state=enabled
systemctl daemon-reload           # after editing unit files
systemctl enable --now foo ; systemctl disable foo ; systemctl mask foo     # mask = cannot be started at all
systemctl is-active foo ; systemctl is-enabled foo
```

## Anatomy of a service unit

A real one (`systemctl cat ssh.service` on Debian):

```ini
[Unit]
Description=OpenBSD Secure Shell server
After=network.target nss-user-lookup.target auditd.service
ConditionPathExists=!/etc/ssh/sshd_not_to_be_run

[Service]
EnvironmentFile=-/etc/default/ssh
ExecStartPre=/usr/sbin/sshd -t
ExecStart=/usr/sbin/sshd -D $SSHD_OPTS
ExecReload=/usr/sbin/sshd -t
ExecReload=/bin/kill -HUP $MAINPID
KillMode=process
Restart=on-failure
RestartPreventExitStatus=255
Type=notify
RuntimeDirectory=sshd
RuntimeDirectoryMode=0755

[Install]
WantedBy=multi-user.target
Alias=sshd.service
```

### `[Service]` essentials

| Directive | Meaning |
|---|---|
| `Type=simple` (default) | the process started by `ExecStart` is the main process; the unit counts as started immediately (good for foreground programs) |
| `Type=exec` | like simple but waits until `execve` succeeds (catches missing binary) |
| `Type=forking` | the process daemonizes; systemd waits for the parent to exit and follows the child (set `PIDFile=`). Avoid: run daemons in the foreground instead |
| `Type=notify` | the service sends `READY=1` via `sd_notify` (socket `$NOTIFY_SOCKET`) when truly ready: the best option for correctness |
| `Type=oneshot` | run to completion; dependents wait for exit (use `RemainAfterExit=yes` to stay "active") |
| `Type=dbus`, `idle` | special cases |
| `ExecStart=`, `ExecStartPre=`, `ExecStartPost=`, `ExecReload=`, `ExecStop=`, `ExecStopPost=` | command lines: **absolute path required, no shell** (no pipes, `&&`, globbing, `~`); use `/bin/sh -c '…'` explicitly if needed; `$VAR`/`${VAR}` expand unit environment, `%n %i %h` are specifiers |
| `User=`, `Group=`, `WorkingDirectory=`, `Environment=`, `EnvironmentFile=` | execution context (a leading `-` makes the file optional) |
| `Restart=` | `no`, `on-failure`, `on-abnormal`, `on-watchdog`, `always`; plus `RestartSec=`; to avoid restart loops, `StartLimitIntervalSec=` / `StartLimitBurst=` (default 5 starts in 10 s, then the unit enters `failed`) |
| `TimeoutStartSec=`, `TimeoutStopSec=` | default 90 s each (stop: `SIGTERM`, then `SIGKILL` at timeout) |
| `KillMode=` | `control-group` (default: signal every process in the unit's cgroup), `mixed` (SIGTERM to main, SIGKILL to rest), `process` (only main: leaves children running, as sshd needs so existing sessions survive restart), `none` |
| `LimitNOFILE=`, `LimitCORE=`, `LimitMEMLOCK=` | rlimits (`ulimit` is not applied to services) |
| `StateDirectory=`, `RuntimeDirectory=`, `CacheDirectory=`, `LogsDirectory=`, `ConfigurationDirectory=` | systemd creates and chowns `/var/lib/NAME`, `/run/NAME`... for the service user, and cleans runtime dirs |
| `ExecStart=` + `Type=notify` + `WatchdogSec=` | service must ping `WATCHDOG=1` or be restarted |

A minimal robust service for your own application:

```ini
# /etc/systemd/system/myapp.service
[Unit]
Description=My application
After=network-online.target postgresql.service
Wants=network-online.target
Requires=postgresql.service

[Service]
Type=exec
User=myapp
DynamicUser=no
StateDirectory=myapp
EnvironmentFile=/etc/myapp/env
ExecStart=/opt/myapp/bin/myapp --config /etc/myapp/config.toml
Restart=on-failure
RestartSec=2s
LimitNOFILE=65536
NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
ReadWritePaths=/var/lib/myapp

[Install]
WantedBy=multi-user.target
```

## Dependencies and ordering: two separate axes

The most misunderstood part. **Requirement** (what must be started) and **ordering** (when) are independent:

| Directive | Meaning |
|---|---|
| `Wants=B` | start B too, but A runs even if B fails (soft) |
| `Requires=B` | if B fails to start, A isn't started; if B is stopped, A is stopped (hard) |
| `Requisite=B` | B must already be active, else A fails immediately |
| `BindsTo=B` | like Requires, plus A stops if B disappears unexpectedly (device-bound services) |
| `PartOf=B` | stop/restart A together with B (one direction only) |
| `Conflicts=B` | starting A stops B |
| `After=B` / `Before=B` | **ordering only**: if both are started in the same transaction, A starts after B. No requirement implied |

`Requires=B` without `After=B` starts A and B **in parallel**. The usual pairing is `Requires=B` + `After=B` (or `Wants=` + `After=`).

Install-time: `WantedBy=multi-user.target` in `[Install]` makes `systemctl enable` create a symlink in `multi-user.target.wants/`, so booting that target pulls the unit in. `systemctl list-dependencies multi-user.target` shows the tree; `systemd-analyze critical-chain` shows what delayed boot; `systemd-analyze verify myapp.service` lints a unit.

**Network pitfalls**: `network.target` only means the network *management stack* started; `network-online.target` (with `Wants=` + `After=`, and `systemd-networkd-wait-online` or `NetworkManager-wait-online`) means an address is configured. Real applications should still retry connections rather than depend on boot ordering.

### Targets

Targets replace SysV runlevels: `poweroff.target` (0), `rescue.target` (single-user, 1), `multi-user.target` (3), `graphical.target` (5), `reboot.target` (6), `emergency.target` (minimal shell, root filesystem read-only). `systemctl get-default`/`set-default multi-user.target`; `systemctl isolate rescue.target`. Synchronization targets: `sysinit.target`, `basic.target`, `network-online.target`, `local-fs.target`, `remote-fs.target`, `time-sync.target`, `sockets.target`, `timers.target`. Real boot on a desktop VM:

```text
$ systemd-analyze
Startup finished in 3.064s (kernel) + 39.663s (userspace) = 42.728s
graphical.target reached after 39.663s in userspace.
$ systemd-analyze blame | head   # slowest units;  systemd-analyze critical-chain  # the chain that gated boot
```

## Socket activation

systemd opens listening sockets itself and starts the service when the first connection arrives, passing the socket as file descriptor 3 (`LISTEN_FDS`, `sd_listen_fds`).

```ini
# myapp.socket
[Socket]
ListenStream=127.0.0.1:8080
Accept=no                       # one service instance handles all connections (yes = a new instance per connection, inetd-style)
[Install]
WantedBy=sockets.target
```

Benefits: faster boot (services start lazily, in parallel, sockets exist before consumers start so ordering problems vanish), **zero-downtime restarts** (the kernel queues connections while the service restarts, no `Connection refused`), privileged ports without root (the service gets the open fd), and privilege separation (`PrivateNetwork=yes` services still serve a passed-in socket). `.path` units trigger on file changes; `.automount` mounts on first access.

## Sandboxing and hardening

systemd can confine a service with namespaces, seccomp and capability bounding, declaratively. Typical directives:

| Directive | Effect |
|---|---|
| `NoNewPrivileges=yes` | `execve` can't gain privileges (setuid/file caps ignored) |
| `User=`, `DynamicUser=yes` | run as an unprivileged user (dynamic: allocated per start with `StateDirectory`) |
| `ProtectSystem=strict` (`full`, `yes`) | mount `/usr`, `/boot`, `/etc` (and with `strict`, the whole file system) read-only; whitelist with `ReadWritePaths=` |
| `ProtectHome=yes\|read-only\|tmpfs` | hide `/home`, `/root`, `/run/user` |
| `PrivateTmp=yes` | private `/tmp` and `/var/tmp` (also kills a class of symlink attacks) |
| `PrivateDevices=yes` | minimal `/dev` (no raw disks) |
| `PrivateNetwork=yes` | only loopback |
| `ProtectKernelTunables/Modules/Logs=yes`, `ProtectControlGroups`, `ProtectClock`, `ProtectProc=invisible` | read-only `/proc/sys`, no module loading, etc. |
| `CapabilityBoundingSet=`, `AmbientCapabilities=` | e.g. `CAP_NET_BIND_SERVICE` only |
| `RestrictAddressFamilies=AF_INET AF_INET6 AF_UNIX` | limit socket types |
| `RestrictNamespaces=yes`, `LockPersonality=yes`, `MemoryDenyWriteExecute=yes` | block risky features (W^X breaks JITs) |
| `SystemCallFilter=@system-service` `SystemCallArchitectures=native` | seccomp allow-list groups (see `systemd-analyze syscall-filter`) |
| `UMask=0027`, `IPAddressDeny=any` + `IPAddressAllow=…` | file modes; per-service IP firewall via cgroup BPF |

Check the exposure score:

```text
$ systemd-analyze security ssh.service
...
✗ UMask=      Files created by service are world-readable by default     0.1
→ Overall exposure level for ssh.service: 9.6 UNSAFE 😨
```

(0 = fully confined, 10 = unconfined.) A shipped `sshd` scores UNSAFE because it needs broad access by design; custom services should aim for < 3. Introduce directives incrementally and test (each restricts something that may break the app); `systemd-analyze security --offline` can score a unit file.

## Journald and journalctl

`systemd-journald` collects logs from the kernel (`/dev/kmsg`), services' **stdout/stderr** (the preferred way: just `print` to stdout), the native journal API, and syslog; it stores them in an indexed **binary** journal with structured fields: `_SYSTEMD_UNIT`, `_PID`, `_UID`, `_COMM`, `PRIORITY`, `MESSAGE`, `_HOSTNAME`, `_BOOT_ID`, `SYSLOG_IDENTIFIER`, `_SYSTEMD_CGROUP`, `_TRANSPORT`. Fields starting with `_` are added by journald and can't be forged by the client.

```bash
journalctl -u ssh -f                          # follow a unit
journalctl -u nginx --since "1 hour ago" -p warning     # time and priority filter (0 emerg … 7 debug)
journalctl -b ; journalctl -b -1              # this boot / previous boot (needs persistent storage)
journalctl -k                                 # kernel messages (dmesg)
journalctl _PID=1234 ; journalctl _SYSTEMD_UNIT=cron.service _COMM=cron
journalctl -o json-pretty -n 1 ; journalctl -o cat
journalctl --disk-usage ; journalctl --vacuum-size=500M ; journalctl --vacuum-time=2weeks
journalctl --verify
systemd-cat -t myscript echo "hello" ; logger -t myscript "hello"   # write from scripts
```

Configuration (`/etc/systemd/journald.conf`): `Storage=persistent` (keeps `/var/log/journal`; default `auto` persists only if that directory exists, otherwise logs are in volatile `/run/log/journal` and vanish at reboot, a common surprise), `SystemMaxUse=`, `MaxRetentionSec=`, `RateLimitIntervalSec=30s` / `RateLimitBurst=10000` per service (excess messages are dropped with a notice), `ForwardToSyslog=`, `Compress=`, `Seal=`. Non-root users see only their own logs unless in `systemd-journal`/`adm`; `sudo journalctl` for all.

Operational guidance: ship logs off-host (journald → `rsyslog`/`vector`/`fluent-bit`/`systemd-journal-remote`) for retention and tamper resistance; log structured JSON on stdout from your apps and let the collector parse; keep `Restart=` limits so a crash loop doesn't fill the journal; watch `journalctl --disk-usage`.

## Other systemd tools

`loginctl` (sessions, linger: `enable-linger user` keeps user services running after logout), `hostnamectl`, `timedatectl`, `localectl`, `resolvectl` (systemd-resolved DNS), `networkctl` (systemd-networkd), `machinectl`/`systemd-nspawn` (containers), `systemd-tmpfiles` (create/clean files per rules in `tmpfiles.d`), `systemd-sysusers`, `systemd-cryptenroll`, `bootctl`, `coredumpctl`, `systemd-run` (transient units, below), `systemctl --user` (per-user service manager).

## Key takeaways

- Everything is a unit; `systemctl cat/show/status` reveal the effective state; override via drop-ins, never edit package files.
- `Requires/Wants` (what) and `After/Before` (when) are independent; pair them. Prefer `Type=notify` or `exec`, avoid `forking`.
- ExecStart has no shell; set `Restart=`, rate limits, `KillMode`, and rlimits explicitly; know that `ulimit` doesn't apply to services.
- Socket activation gives lazy start and zero-downtime restarts; sandbox directives plus `systemd-analyze security` harden services cheaply.
- journald is structured and indexed; make it persistent, rate-limit-aware, and forward logs off-host.
