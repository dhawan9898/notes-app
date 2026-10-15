---
title: SELinux, AppArmor, Landlock and System Hardening
track: linux
module: Security
moduleOrder: 11
order: 2
level: expert
summary: Mandatory access control with Linux Security Modules — SELinux types and troubleshooting, AppArmor profiles, Landlock and Yama — plus a prioritized, evidence-based hardening checklist covering kernel sysctls, mounts, services, integrity, logging and boot.
prerequisites:
  - Users, Permissions, ACLs and Capabilities
  - SSH, PAM, sudo and auditd — Authentication, Authorization and Accountability
sources:
  - label: "Linux kernel docs — Linux Security Module Usage; SELinux; AppArmor; Yama; Landlock; Lockdown"
    url: https://docs.kernel.org/admin-guide/LSM/index.html
  - label: "Red Hat — Using SELinux (RHEL documentation); SELinux Project wiki"
    url: https://github.com/SELinuxProject/selinux-notebook
  - label: "AppArmor documentation (Ubuntu Server / Debian Wiki / GitLab apparmor wiki)"
    url: https://gitlab.com/apparmor/apparmor/-/wikis/Documentation
  - label: "CIS Benchmarks (Distribution Independent Linux, Ubuntu, RHEL); DISA STIG; NIST SP 800-123, 800-53"
    url: https://www.cisecurity.org/cis-benchmarks
  - label: "KSPP — Kernel Self Protection Project recommended settings"
    url: https://kernsec.org/wiki/index.php/Kernel_Self_Protection_Project/Recommended_Settings
  - label: "Lynis, OpenSCAP, AIDE documentation"
    url: https://cisofy.com/lynis/
---

## DAC vs. MAC

Standard Unix permissions (users, groups, mode bits, ACLs) are **discretionary access control (DAC)**: the owner of a file decides who can access it, and any process running as root bypasses it. If a web server running as a limited user is exploited, DAC still lets it read everything that user can read; if it runs as root, everything.

**Mandatory access control (MAC)** adds a *system-wide policy* that processes cannot override, even as root. A compromised `nginx` is confined to what the policy permits `nginx` to do (read web content, bind ports 80/443), not what its UID could.

Linux implements MAC through the **Linux Security Module (LSM)** framework: hooks at security-relevant kernel operations (file open, socket connect, exec, mount, ptrace...) that any enabled module can veto *after* DAC has allowed it. Multiple LSMs can be stacked (a "minor" and one "major" MAC historically; now several). The active list on a Debian system:

```text
$ cat /sys/kernel/security/lsm
lockdown,capability,landlock,yama,apparmor,tomoyo,bpf,ipe,ima,evm
```

Meaning: `capability` (always), `yama` (ptrace scope), `landlock` (self-sandboxing), `apparmor` (path-based MAC), `lockdown` (kernel integrity), `bpf` (BPF-LSM programs), `ima`/`evm` (integrity measurement and extended-attribute integrity), `tomoyo`, `ipe`. On RHEL/Fedora it reads `…,selinux,…`.

## SELinux

**Security-Enhanced Linux** (NSA, 2000) labels every subject (process) and object (file, socket, port, device) with a **security context** and enforces a rule policy over those labels.

```text
$ ls -Z /var/www/html/index.html
system_u:object_r:httpd_sys_content_t:s0  /var/www/html/index.html
$ ps -eZ | grep nginx
system_u:system_r:httpd_t:s0      1234 ?  nginx
        user  :  role :  type    : level
```

The context is `user:role:type:level`. The **type** matters most (**Type Enforcement**): policy rules like `allow httpd_t httpd_sys_content_t:file { read getattr open };` say processes of domain `httpd_t` may read files labeled `httpd_sys_content_t`, and *nothing else* unless allowed (default deny). `level` is the MLS/MCS sensitivity: **MCS categories** (`s0:c123,c456`) give each container/VM a unique label so that two containers of type `container_t` still cannot access each other's files.

### Modes

| Mode | Behavior |
|---|---|
| **Enforcing** | denials block and are logged |
| **Permissive** | denials are only logged (use for diagnosis, per-domain with `semanage permissive -a httpd_t`) |
| **Disabled** | off (changing requires reboot and relabeling): don't do this |

`getenforce`, `sestatus`, `setenforce 0|1` (runtime), `/etc/selinux/config` (`SELINUX=enforcing`, `SELINUXTYPE=targeted`). The default **targeted** policy confines network-facing services and leaves most user processes in `unconfined_t`.

### Working with labels

New files inherit the label of their directory; moved files **keep their old label** (the classic cause of "403 after mv"). Persistent label rules are in the policy's file-context database:

```bash
semanage fcontext -a -t httpd_sys_content_t '/srv/www(/.*)?'    # define the rule
restorecon -Rv /srv/www                                         # apply it (relabel)
chcon -t httpd_sys_content_t file                               # temporary, lost on relabel
ls -Zd /srv/www ; matchpathcon /srv/www                         # actual vs expected
semanage port -a -t http_port_t -p tcp 8081                     # let httpd bind another port
semanage port -l | grep http_port_t
getsebool -a | grep httpd ; setsebool -P httpd_can_network_connect on     # booleans toggle policy switches; -P persists
touch /.autorelabel && reboot                                   # full relabel after big changes
```

Booleans (`httpd_can_network_connect_db`, `httpd_enable_homedirs`, `container_manage_cgroup`, `virt_use_nfs`) are the intended knobs for common deviations: look there before writing policy.

### Troubleshooting denials

1. Symptom: "Permission denied" even though modes and ownership are right; works with `setenforce 0`.
2. Find the denial: `ausearch -m AVC,USER_AVC -ts recent`, `journalctl -t setroubleshoot`, `sealert -a /var/log/audit/audit.log` (human-readable explanation with suggested fixes).

```text
type=AVC msg=audit(1760000000.123:456): avc:  denied  { name_connect } for  pid=1234 comm="nginx" dest=5432
  scontext=system_u:system_r:httpd_t:s0 tcontext=system_u:object_r:postgresql_port_t:s0 tclass=tcp_socket permissive=0
```

3. Read it: *source type* `httpd_t` tried `name_connect` (outgoing TCP connection) to port type `postgresql_port_t`. The right fix is the boolean `httpd_can_network_connect_db`, not a new module.
4. Only if no boolean/label fix exists, generate a **local module** *after reviewing it*: `ausearch -m AVC -ts recent | audit2allow -M myapp` shows the `allow` rules; `semodule -i myapp.pp`. Never apply `audit2allow` output blindly (it allows whatever the attack did), and never leave `setenforce 0` as the "fix".
5. Use `semodule -l`, `sesearch -A -s httpd_t -t httpd_sys_content_t`, and `semanage permissive` for one domain while you develop a policy (`sepolicy generate`).

Containers on SELinux hosts: volume mounts need `:z` (shared) or `:Z` (private) so Docker/Podman relabel them; Kubernetes uses `seLinuxOptions`.

## AppArmor

**AppArmor** (Ubuntu, Debian, SUSE) is **path-based** MAC: a profile per program lists the files, capabilities, network access and executables it may use. Simpler to write and read than SELinux, with less granularity (labels attached to paths, not inodes: hard links and bind mounts can matter).

```bash
aa-status                                   # loaded profiles, enforce vs complain, confined processes
aa-genprof /usr/bin/myapp                   # interactive profile generation by running the app
aa-logprof                                  # update profiles from logged denials (reviews each)
aa-complain /etc/apparmor.d/usr.bin.myapp ; aa-enforce ...
apparmor_parser -r /etc/apparmor.d/usr.bin.myapp      # (re)load a profile
journalctl -k | grep 'apparmor="DENIED"'              # denials (also in audit log)
```

```text
# /etc/apparmor.d/usr.local.bin.myapp
#include <tunables/global>
/usr/local/bin/myapp {
  #include <abstractions/base>
  #include <abstractions/nameservice>
  capability net_bind_service,
  network inet stream,
  /etc/myapp/** r,
  /var/lib/myapp/** rw,
  /var/log/myapp.log w,
  deny /home/** rwx,
  /usr/bin/openssl ix,        # may execute openssl, inheriting this profile
}
```

Modes: **enforce** (block + log), **complain** (log only: learning). Unconfined programs have no profile. Docker applies `docker-default`; snaps and LXD use per-snap profiles. Recent Ubuntu versions use AppArmor to restrict **unprivileged user namespaces** for unprofiled programs.

## Other security modules

- **Landlock**: lets an *unprivileged process sandbox itself* (restrict filesystem and, in newer ABI versions, TCP bind/connect access) with three syscalls (`landlock_create_ruleset`, `landlock_add_rule`, `landlock_restrict_self`). Stackable with other LSMs, irreversible for the process and children. Used by browsers, `systemd` (`LandlockFS`), Rust/Go libraries to apply least privilege in code.
- **Yama**: `kernel.yama.ptrace_scope` — 0 classic (any process may ptrace a same-UID process), **1 restricted** (only descendants; stops a compromised process from reading other processes' memory such as ssh-agent or password managers), 2 admin-only, 3 none. Recommended: 1 or higher.
- **Lockdown**: `lockdown=integrity|confidentiality` (auto-enabled with Secure Boot) forbids actions that let root modify the running kernel or read its memory: unsigned modules, `/dev/mem`, kexec of unsigned images, hibernation, certain MSR/PCI access.
- **IMA/EVM**: measure and appraise file hashes/signatures (verified boot of userspace, remote attestation with TPM).
- **seccomp**, **capabilities**, **namespaces**, **cgroups**: covered in the container notes; **BPF-LSM** allows custom policies in eBPF.

## System hardening checklist

Hardening is risk reduction, prioritized by exposure. Measure first (`lynis audit system`, OpenSCAP scans against CIS/STIG profiles, `systemd-analyze security`) and then change the highest-impact items. Always test in staging and keep out-of-band access.

### 1. Reduce attack surface

- **Install minimal**; remove unneeded packages. `systemctl list-unit-files --state=enabled`, `ss -ltnup` (every listening port must be explained), `ps auxf`. Disable and mask what you don't use (`avahi`, `cups`, `rpcbind`, `telnet`).
- Bind services to the interface they need (`127.0.0.1` for local-only), put admin interfaces behind VPN/bastion.
- **Default-deny firewall** (nftables/firewalld/ufw) with egress filtering where practical; rate-limit SSH.

### 2. Patching and supply chain

- Automatic security updates (`unattended-upgrades`, `dnf-automatic`), kernel live-patching where reboots are costly; track CVEs for your package set; subscribe to distro advisories.
- Verify package signatures (`gpgcheck`), use trusted repos, pin and mirror internally; verify downloaded binaries (checksums, signatures); minimal container base images.

### 3. Accounts and authentication

- No shared accounts; individual SSH keys/certs; MFA for privileged access; remove or lock unused accounts (`passwd -l`, `usermod -s /usr/sbin/nologin`); service accounts with no login shell; `sudo` with least privilege; password aging and quality (`pam_pwquality`, `chage`); disable root SSH.
- Audit: `awk -F: '($3==0)' /etc/passwd` (only root has UID 0), `awk -F: '($2=="")' /etc/shadow` (no empty passwords), `lastlog`.

### 4. Filesystem and mounts

- Separate partitions/volumes for `/tmp`, `/var`, `/var/log`, `/home`, `/var/tmp` with options `nodev,nosuid,noexec` where feasible (`noexec` on `/tmp` breaks some installers; `/dev/shm` too); sticky bit on world-writable dirs.
- Find risk: `find / -xdev -type f \( -perm -4000 -o -perm -2000 \) -ls` (setuid/setgid inventory), `find / -xdev -type f -perm -0002 -ls` (world-writable), unowned files (`-nouser -o -nogroup`). Remove unnecessary setuid bits (`chmod u-s`).
- `umask 027` for services (`UMask=0027`), no secrets in world-readable files, `chmod 600` for keys; encrypt disks (LUKS) and backups.
- Prevent sneaky links/races: `fs.protected_symlinks=1`, `fs.protected_hardlinks=1`, `fs.protected_regular=2`, `fs.protected_fifos=2` (defaults on modern systems), `fs.suid_dumpable=0`.

### 5. Kernel and sysctl hardening

Example host values (from a Debian system) and the usual recommendations:

| sysctl | Example value | Recommended | Why |
|---|---|---|---|
| `kernel.randomize_va_space` | 2 | 2 | full ASLR |
| `kernel.yama.ptrace_scope` | **0** | 1 (or 2) | block cross-process memory reads |
| `kernel.kptr_restrict` | **0** | 1 or 2 | hide kernel addresses from KASLR-bypass |
| `kernel.dmesg_restrict` | 1 | 1 | kernel log only for root |
| `kernel.unprivileged_bpf_disabled` | 2 | 1 or 2 | reduce eBPF attack surface |
| `kernel.perf_event_paranoid` | 3 (Debian) | ≥ 2 | restrict perf |
| `kernel.kexec_load_disabled` | – | 1 | no replacing the running kernel |
| `kernel.modules_disabled` | 0 | 1 after boot (if feasible) | no more module loading |
| `kernel.sysrq` | – | 0 or limited bitmask | no keyboard magic |
| `net.ipv4.conf.all.rp_filter` | **0** | 1 (2 for asymmetric routing) | anti-spoofing |
| `net.ipv4.tcp_syncookies` | 1 | 1 | SYN flood resilience |
| `net.ipv4.conf.all.accept_redirects`, `accept_source_route` | – | 0 | no routing manipulation |
| `net.ipv4.conf.all.log_martians` | – | 1 | log impossible sources |
| `net.ipv4.icmp_echo_ignore_broadcasts` | – | 1 | no smurf amplification |
| `net.ipv6.conf.all.accept_ra` | – | 0 on servers with static config | no rogue RA |
| `user.max_user_namespaces` | – | 0 where not needed | reduce kernel attack surface (breaks rootless containers) |
| `vm.mmap_min_addr` | 65536 | 65536 | NULL-pointer deref exploits |

Kernel boot parameters from the KSPP: `init_on_alloc=1 init_on_free=1` (zero memory), `slab_nomerge`, `page_alloc.shuffle=1`, `randomize_kstack_offset=on`, `vsyscall=none`, `module.sig_enforce=1`, `lockdown=integrity`, `pti=on`, `spectre_v2=on`, `mitigations=auto` (don't disable CPU mitigations on multi-tenant systems). Check `grep . /sys/devices/system/cpu/vulnerabilities/*`. Persist sysctls in `/etc/sysctl.d/99-hardening.conf`.

### 6. Services and runtime confinement

- Run services as dedicated users; apply systemd sandboxing (`NoNewPrivileges`, `ProtectSystem=strict`, `PrivateTmp`, `CapabilityBoundingSet`, `SystemCallFilter`, `RestrictAddressFamilies`; score with `systemd-analyze security`).
- Keep SELinux enforcing or AppArmor profiles loaded; add profiles for your own network-facing apps.
- Containers: non-root, read-only rootfs, dropped capabilities, seccomp, no host mounts (see container notes).
- Don't put secrets in environment variables, command lines (visible in `/proc/PID/cmdline` and `ps`) or images: use credentials files with `0400`, systemd `LoadCredential=`/`SetCredentialEncrypted=`, Vault or cloud secret managers.

### 7. Logging, detection and integrity

- Central log shipping (journald → rsyslog/Vector/Fluent Bit → SIEM), time-synchronized (`chrony`), retention and alerting.
- **auditd** rules for identity, sudoers, sshd config, module loads, `execve` by users; alert on new SUID files and listeners.
- **File integrity monitoring**: AIDE/Tripwire/Wazuh; package verification (`rpm -Va`, `debsums -c`); IMA for stronger guarantees.
- Host intrusion detection: Wazuh/OSSEC, Falco/Tetragon (eBPF runtime rules), `rkhunter`/`chkrootkit` (weak), `fail2ban`/CrowdSec.
- Network visibility: flow logs, Zeek/Suricata at chokepoints.

### 8. Boot, firmware and physical

- UEFI Secure Boot with signed bootloader/kernel, **GRUB password** or locked bootloader config, kernel lockdown, TPM-bound disk encryption (`systemd-cryptenroll --tpm2-device`), BIOS/BMC passwords and firmware updates, restrict console access and USB (`usbguard`), IPMI/BMC on isolated management networks.

### 9. Process and resilience

- Backups (3-2-1, offline/immutable copy, tested restores): the real ransomware defense.
- Configuration management (Ansible, Puppet) and immutable infrastructure so hardening is **declarative and drift-checked**; golden images; benchmark scans in CI.
- Incident response plan; break-glass access; tabletop exercises. Threat-model: what are you protecting, from whom, and what is the blast radius if this host falls?

## A pragmatic order of operations

1. Patch; remove unused services; firewall default-deny.
2. SSH keys only, no root login, MFA/bastion; sudo least privilege.
3. Keep MAC enforcing (SELinux/AppArmor); sandbox services with systemd.
4. Central logs + auditd + alerting; backups tested.
5. Kernel/sysctl/mount hardening, integrity monitoring, Secure Boot/TPM.
6. Continuous scanning (Lynis/OpenSCAP/CIS) and drift detection.

## Key takeaways

- MAC (SELinux, AppArmor via LSM) confines processes by policy even if they run as root; SELinux uses type enforcement on labels (fix labels and booleans before writing policy, never disable it), AppArmor uses path-based profiles (complain → enforce).
- Landlock, Yama, lockdown and IMA complement them; check `/sys/kernel/security/lsm` to see what's active.
- Hardening = smaller attack surface + patching + strong authentication + confinement + logging + tested backups, applied in priority order and verified with scanners.
- Review, not just apply: `audit2allow` output, CIS settings, and sysctl lists copied from the internet can open holes or break workloads.
