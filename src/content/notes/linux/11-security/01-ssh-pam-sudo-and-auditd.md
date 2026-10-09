---
title: SSH, PAM, sudo and auditd — Authentication, Authorization and Accountability
track: linux
module: Security
moduleOrder: 11
order: 1
level: advanced
summary: Securing remote access and privilege on Linux — how SSH authenticates hosts and users (keys, certificates, FIDO2), a hardened sshd configuration, the PAM stack model, sudoers design, and kernel auditing with auditd, with verified examples.
sources:
  - label: "RFC 4251 / 4252 / 4253 / 4254 — The Secure Shell (SSH) Protocol Architecture, Authentication, Transport, Connection"
    url: https://www.rfc-editor.org/rfc/rfc4251
  - label: "OpenSSH manual pages — ssh(1), sshd(8), ssh_config(5), sshd_config(5), ssh-keygen(1) (CERTIFICATES), ssh-agent(1)"
    url: https://man.openbsd.org/sshd_config
  - label: "Linux-PAM System Administrators' Guide; pam.conf(5), pam.d(5)"
    url: https://www.chiark.greenend.org.uk/doc/linux-pam-doc/Linux-PAM_SAG.html
  - label: "sudoers(5), sudo.conf(5); visudo(8)"
    url: https://www.sudo.ws/docs/man/sudoers.man/
  - label: "Linux Audit documentation — auditd(8), auditctl(8), audit.rules(7), ausearch(8)"
    url: https://github.com/linux-audit/audit-documentation
  - label: "CIS Benchmarks (Distribution Independent Linux) and DISA STIG; Mozilla OpenSSH guidelines"
    url: https://infosec.mozilla.org/guidelines/openssh
---

Remote administration and privilege escalation are the two most attacked paths on a Linux server. This note covers the tools that protect them — and that you will touch daily — SSH, PAM, sudo — plus the audit trail that proves what happened.

## SSH

The Secure Shell protocol (RFC 4251–4254) has three layers: a **transport** layer (key exchange, server authentication, encryption and integrity), a **user authentication** layer and a **connection** layer (multiplexed channels: shells, exec, forwarding, SFTP).

### Server authentication: host keys

On connection the server proves its identity with a **host key** (`/etc/ssh/ssh_host_*_key`). The client checks it against `~/.ssh/known_hosts` (or `/etc/ssh/ssh_known_hosts`). First contact is **TOFU** (trust on first use): you are shown the fingerprint (`SHA256:…`) and should verify it out-of-band. A later mismatch triggers the loud `REMOTE HOST IDENTIFICATION HAS CHANGED` warning: either the host was rebuilt or you are being intercepted. Better: **SSH host certificates** signed by a CA (clients trust the CA once with `@cert-authority * ssh-ed25519 AAAA…` in `known_hosts`), or `SSHFP` DNS records with DNSSEC. Keep `StrictHostKeyChecking=yes` (or `accept-new` for automation, never `no`).

### User authentication

| Method | Notes |
|---|---|
| **Public key** | the recommended method; client proves possession of a private key |
| Password | brute-forced constantly on the Internet; disable |
| Keyboard-interactive | PAM-driven prompts: OTP, second factor |
| GSSAPI/Kerberos | enterprise SSO |
| **Certificates** | CA-signed user keys with identity, principals, validity window, restrictions |
| **FIDO2 / security keys** (`sk-ssh-ed25519@openssh.com`) | private key lives in hardware; touch required; `ssh-keygen -t ed25519-sk -O resident -O verify-required` |

Key types: prefer **Ed25519** (`ssh-keygen -t ed25519 -a 100 -C "alice@laptop"`; `-a` = KDF rounds protecting the passphrase); RSA only at ≥ 3072 bits (`rsa-sha2-512`); avoid DSA (removed), ECDSA is acceptable. Always protect private keys with a **passphrase** (or hardware) and use `ssh-agent` (`ssh-add -t 8h`) to avoid retyping. Permissions: `~/.ssh` `700`, private keys and `authorized_keys` `600` (sshd's `StrictModes yes` refuses otherwise).

`~/.ssh/authorized_keys` options restrict what a key can do:

```text
restrict,from="203.0.113.0/24",command="/usr/local/bin/backup-only",no-pty ssh-ed25519 AAAA… backup@host
```

(`restrict` turns off forwarding, pty, agent, X11 and user-rc in one word; add back only what's needed.)

### SSH certificates (real example)

A user CA removes per-host `authorized_keys` management: servers trust the CA once (`TrustedUserCAKeys /etc/ssh/user_ca.pub`) and map principals to accounts (`AuthorizedPrincipalsFile`). Short-lived certificates make revocation largely unnecessary:

```bash
ssh-keygen -t ed25519 -N '' -f ca -C "demo-ca"                 # the CA key (guard it, ideally an HSM/YubiKey or Vault)
ssh-keygen -t ed25519 -N '' -f user -C "alice@laptop"
ssh-keygen -s ca -I alice-2026 -n alice,deploy -V +1h \
           -O no-agent-forwarding -O force-command=/usr/bin/uptime user.pub    # sign: identity, principals, validity, options
ssh-keygen -L -f user-cert.pub
```

```text
user-cert.pub:
        Type: ssh-ed25519-cert-v01@openssh.com user certificate
        Signing CA: ED25519 SHA256:kFPj…WN5U (using ssh-ed25519)
        Key ID: "alice-2026"
        Valid: from 2026-10-09T08:36:00 to 2026-10-09T09:37:01
        Principals: alice, deploy
        Critical Options: force-command /usr/bin/uptime
        Extensions: permit-X11-forwarding permit-port-forwarding permit-pty
```

The certificate is valid for one hour, usable only as `alice` or `deploy`, and forces a command. Tooling such as Smallstep, HashiCorp Vault, Teleport, or Netflix BLESS automate issuance after SSO.

### sshd hardening

Edit `/etc/ssh/sshd_config` (or drop-ins under `/etc/ssh/sshd_config.d/*.conf`; the first value wins for most options, so ordering of drop-ins matters), validate with `sshd -t`, inspect the **effective** configuration with `sudo sshd -T` (and `sshd -T -C user=alice,addr=10.0.0.5` for `Match` blocks), then `systemctl reload ssh` (keep an existing session open when testing!).

```text
PermitRootLogin no                      # or prohibit-password; use named accounts + sudo for accountability
PasswordAuthentication no
KbdInteractiveAuthentication no         # unless used for 2FA
PubkeyAuthentication yes
AuthenticationMethods publickey         # or "publickey,keyboard-interactive" for key + OTP
AllowGroups ssh-users                   # explicit allow-list (AllowUsers user@192.0.2.0/24 also possible)
MaxAuthTries 3
LoginGraceTime 20
MaxStartups 10:30:60                    # throttle unauthenticated connections
ClientAliveInterval 300
ClientAliveCountMax 2                   # drop dead sessions
X11Forwarding no
AllowTcpForwarding no                   # allow only where needed (Match blocks)
AllowAgentForwarding no
PermitTunnel no
PermitUserEnvironment no
LogLevel VERBOSE                        # logs key fingerprints used to log in
Banner /etc/issue.net
```

Algorithms: modern OpenSSH negotiates strong defaults. OpenSSH 10 prefers the post-quantum hybrid key exchange `mlkem768x25519-sha256` (then `sntrup761x25519-sha512`, `curve25519-sha256`), ciphers `chacha20-poly1305@openssh.com` and `aes256-gcm@openssh.com`. `ssh -Q kex|cipher|mac|key` lists what the *binary supports*, which still includes legacy items (`3des-cbc`, `diffie-hellman-group1-sha1`) kept for compatibility; restrict with `KexAlgorithms`, `Ciphers`, `MACs`, `HostKeyAlgorithms` only if you control all clients, and audit with `ssh-audit`.

Defense against brute force: key-only auth makes password guessing moot; add **fail2ban** / **CrowdSec** to ban noisy sources, rate-limit new connections in the firewall, restrict the source networks or put SSH behind a VPN/bastion/zero-trust proxy. Changing the port reduces log noise but is not security.

### Client features you should know

```bash
ssh -J bastion.example.com app1           # ProxyJump: hop through a bastion (preferred over agent forwarding)
ssh -L 5432:db.internal:5432 bastion      # local forward: localhost:5432 -> db.internal:5432 via bastion
ssh -R 9000:localhost:3000 server         # remote forward: server:9000 -> your localhost:3000
ssh -D 1080 server                        # SOCKS proxy
ssh -N -f -L ...                          # tunnel only, background
ssh -G host                               # print the fully resolved client configuration (great for debugging ~/.ssh/config)
ssh -vvv host                             # verbose protocol debugging; server side: sshd -ddd -p 2222
ssh-copy-id -i ~/.ssh/id_ed25519.pub user@host
```

```text
# ~/.ssh/config
Host *
    ServerAliveInterval 30
    IdentitiesOnly yes
    HashKnownHosts yes
Host bastion
    HostName bastion.example.com
    User alice
    ControlMaster auto
    ControlPath ~/.ssh/cm-%C
    ControlPersist 10m          # connection multiplexing: first login authenticates, others are instant
Host app*
    ProxyJump bastion
```

**Agent forwarding** (`-A`) exposes your agent socket on the remote host: anyone with root there can use your keys while the session lasts. Prefer `ProxyJump`, or restrict the agent with `ssh-add -c` (confirm each use) and FIDO2 keys that need a touch. `scp` uses a legacy protocol; modern OpenSSH implements `scp` over SFTP; use `sftp` or `rsync -e ssh`.

### Logs and investigation

`journalctl -u ssh` (Debian: `ssh`; RHEL: `sshd`), `/var/log/auth.log` or `/var/log/secure`. `last`, `lastb` (failed logins; `/var/log/btmp`), `who`, `w`, `ss -tnp state established '( sport = :22 )'`. With `LogLevel VERBOSE` successful key logins include the fingerprint, which lets you map a login to a key.

## PAM: Pluggable Authentication Modules

PAM separates applications (sshd, login, sudo, su, cron, xrdp, screensavers) from authentication policy. Each application has a stack of modules in `/etc/pam.d/<service>`; changing policy (add 2FA, lockouts, password quality) means editing configuration, not code.

Four **management groups**, each a stack:

| Type | Question | Example modules |
|---|---|---|
| **auth** | who are you? (verify credentials, set credentials) | `pam_unix`, `pam_sss`, `pam_krb5`, `pam_faillock`, `pam_google_authenticator`, `pam_u2f` |
| **account** | may you log in now? (expiry, time/host restrictions, locked) | `pam_unix`, `pam_nologin`, `pam_access`, `pam_time` |
| **password** | change authentication tokens | `pam_pwquality`, `pam_unix` |
| **session** | set up/tear down the session | `pam_limits`, `pam_env`, `pam_systemd` (logind), `pam_loginuid` (audit UID), `pam_motd`, `pam_mail`, `pam_selinux`, `pam_mkhomedir` |

Line format: `type control module-path [arguments]`. **Control flags** decide how a result affects the stack:

| Flag | Meaning |
|---|---|
| `required` | must succeed, but the rest of the stack still runs (so an attacker can't tell which module failed) |
| `requisite` | must succeed; failure returns immediately |
| `sufficient` | success (if no earlier `required` failed) ends the stack successfully; failure is ignored |
| `optional` | only matters if it's the only module |
| `[value=action …]` | fine-grained: e.g. `[success=2 default=ignore]` skip the next two lines on success |
| `include` / `substack` / `@include` | reuse another file (Debian `common-auth`) |

Real excerpt (Debian `/etc/pam.d/sshd`):

```text
@include common-auth
account    required     pam_nologin.so
@include common-account
session [success=ok ignore=ignore module_unknown=ignore default=bad]  pam_selinux.so close
session    required     pam_loginuid.so
session    optional     pam_keyinit.so force revoke
@include common-session
session    optional     pam_motd.so  motd=/run/motd.dynamic
session    required     pam_limits.so
session    required     pam_env.so
```

Things to know:

- `sshd` consults PAM only if `UsePAM yes` (needed for account/session handling and keyboard-interactive); public-key authentication is done by sshd itself, but **account and session** modules still run.
- **Account lockout**: `pam_faillock` (RHEL 8+/Debian 12+; replaces `pam_tally2`): `deny=5 unlock_time=900` in `/etc/security/faillock.conf`, view with `faillock --user alice`, reset with `--reset`. Mind denial-of-service via lockout of admin accounts.
- **Password policy**: `pam_pwquality` (`minlen`, `dcredit` …) in `/etc/security/pwquality.conf`; hashes in `/etc/shadow` use `$y$` (yescrypt) or `$6$` (SHA-512 crypt); `chage -l user`, `/etc/login.defs`.
- **Limits**: `pam_limits` applies `/etc/security/limits.conf` / `limits.d/*` to *login sessions*, **not to systemd services** (use `LimitNOFILE=` etc.).
- **Tools**: Debian `pam-auth-update`, RHEL `authselect`. Do not hand-edit generated `common-*` files.
- **Lockout hazard**: a syntax error or a `required pam_deny` in `sudo`/`sshd`/`login` can lock everyone out. Always keep a root shell open in another session while testing, and test with a second login before closing. Console recovery: rescue boot.
- **Debugging**: add `debug` to module args, watch `/var/log/auth.log`/journal; `pamtester service user authenticate`.

## sudo

`sudo` runs commands as another user (default root) after authenticating **the caller**, according to `/etc/sudoers` and `/etc/sudoers.d/*` (edit only with `visudo -f file`, which checks syntax; a broken sudoers locks you out of sudo).

```text
# who  where = (as_whom) tag: commands
alice  ALL = (root) /usr/bin/systemctl restart nginx, /usr/bin/journalctl -u nginx
%ops   ALL = (ALL) ALL
deploy web1 = (www-data) NOPASSWD: /opt/app/bin/deploy ""

Defaults  secure_path="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
Defaults  env_reset, use_pty, timestamp_timeout=5
Defaults  log_output, logfile=/var/log/sudo.log, iolog_dir=/var/log/sudo-io
Cmnd_Alias SERVICES = /usr/bin/systemctl start *, /usr/bin/systemctl stop *
```

Design rules:

- **Least privilege**: grant specific commands with exact arguments, not `ALL`. Wildcards in arguments (`systemctl * nginx`) are exploitable (`systemctl edit`, spawn pager shells). A trailing `""` forbids extra arguments.
- **Shell escapes**: editors (`vi`), pagers (`less`, `systemctl`'s pager), `find -exec`, `awk`, `tar --checkpoint-action`, and many tools spawn shells: use `NOEXEC:` tag, `sudoedit`, `SYSTEMD_PAGER=`, or avoid granting them. Check any proposed binary on GTFOBins.
- `NOPASSWD` is appropriate for narrow automation commands, risky for broad access. Use `Defaults env_reset` (default), keep `secure_path`, and be careful with `env_keep` (`LD_PRELOAD`, `PYTHONPATH`).
- **Accountability**: sudo logs via syslog/journal (`journalctl _COMM=sudo`), and can record full terminal sessions (`log_input,log_output`; replay with `sudoreplay`). Avoid `sudo su -`/`sudo -i` for routine work because it hides who did what in audit trails (the audit UID survives, but shell history does not).
- Related: `sudoedit`, polkit rules for desktop/service actions (`pkexec`), `doas` (small alternative), `su` (needs target's password), per-service users with systemd `User=`; avoid long-lived root shells. Keep sudo updated: it has had serious CVEs (Baron Samedit CVE-2021-3156 heap overflow, the 2025 `--chroot` local privilege escalation).

## auditd: the kernel audit trail

The **Linux Audit** subsystem records security-relevant events in the kernel (syscalls, file access, logins, SELinux denials) with tamper-resistant sequencing; `auditd` writes them to `/var/log/audit/audit.log`. It answers *who did what, when, and from where*, and is required by PCI-DSS, HIPAA, CIS and STIG.

Rules (`/etc/audit/rules.d/*.rules`, compiled by `augenrules --load`; ad hoc with `auditctl`):

```text
-w /etc/passwd -p wa -k identity                    # watch writes/attribute changes to a file
-w /etc/sudoers.d/ -p wa -k scope
-a always,exit -F arch=b64 -S execve -F auid>=1000 -F auid!=-1 -k user_exec       # log every program a logged-in user runs
-a always,exit -F arch=b64 -S mount,umount2 -k mounts
-a always,exit -F arch=b64 -S init_module,finit_module,delete_module -k modules
-a always,exit -F arch=b64 -S openat -F exit=-EACCES -F auid>=1000 -k access_denied
-e 2                                                # lock the rules until reboot (immutable)
```

```bash
ausearch -k identity -ts today -i              # search by key, interpret IDs/names
ausearch -m USER_LOGIN,USER_AUTH -sv no -ts yesterday      # failed logins
aureport -au --summary ; aureport -x --summary             # authentication / executables reports
auditctl -l ; auditctl -s                      # loaded rules, status (backlog, lost events)
```

Key concepts: the **audit UID (`auid`/loginuid)** is set at login (by `pam_loginuid`) and **never changes**, even across `su`/`sudo`: you can always trace actions to the original login. Records for one event share an ID (`msg=audit(1759999999.123:4567)`): `SYSCALL`, `EXECVE` (arguments), `CWD`, `PATH` (file, inode, perms), `PROCTITLE`, `USER_CMD` (sudo), `AVC` (SELinux). Performance: each audited syscall costs; avoid auditing hot paths (`read`, `write`, all `openat`), filter by `auid` and key, size the backlog (`-b 8192`) and watch `lost` counts. Forward logs off-host (`audisp-remote`, rsyslog, Wazuh/Elastic agent) so an attacker can't erase them; set `admin_space_left_action`/`disk_full_action` deliberately (halting a server on a full audit disk is a DoS risk but required in some regimes).

## Putting it together: remote access baseline

1. SSH keys or certificates (preferably hardware-backed), no passwords, no root login, allow-list group, bastion/VPN in front.
2. PAM: lockout policy and MFA for privileged paths; account expiry for contractors.
3. sudo: command-scoped rules, I/O logging for admins, no shared accounts.
4. auditd/journald shipping off-host; alerts on `identity`, sudoers and sshd_config changes, new SUID files, and logins from new places.
5. Test the break-glass path (console access, rescue boot) before you need it.

## Key takeaways

- SSH: verify host keys (or use host certificates), use Ed25519/FIDO2 keys or short-lived CA certificates, harden `sshd_config` (no passwords/root, allow-lists, limited forwarding), use `ProxyJump` instead of agent forwarding.
- PAM stacks (`auth`, `account`, `password`, `session`) with control flags define authentication policy; edit carefully and keep a recovery session.
- sudoers should grant narrowly with exact commands, guard against shell escapes, and log (including I/O) for accountability.
- auditd records syscalls and file changes with a persistent `auid`; write focused rules, ship logs off-host, and lock the ruleset.
