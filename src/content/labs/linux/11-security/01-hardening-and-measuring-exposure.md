---
title: Hardening and Measuring Exposure
track: linux
module: Security
moduleOrder: 11
order: 1
level: expert
minutes: 40
env: machine
summary: "Four exercises on the security module that run without root: scoring a service's exposure with systemd-analyze before and after sandboxing directives, seeing why salted password hashes differ for the same password, validating sudoers syntax safely, and reading SSH client defaults and key types, with captured output."
prerequisites:
  - "SSH, PAM, sudo and auditd — Authentication, Authorization and Accountability"
  - "SELinux, AppArmor, Landlock and System Hardening"
sources:
  - label: "systemd-analyze(1) — security, and systemd.exec(5) sandboxing options"
    url: https://www.freedesktop.org/software/systemd/man/latest/systemd.exec.html
  - label: "man 5 sudoers, man 8 visudo, man 5 ssh_config, man 1 ssh-keygen"
    url: https://man7.org/linux/man-pages/man5/sudoers.5.html
  - label: "OpenSSH release notes and security recommendations"
    url: https://www.openssh.com/security.html
  - label: "OWASP Password Storage Cheat Sheet"
    url: https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html
---

Security work is mostly about *reducing what can go wrong* and *checking that you did*. These exercises give you measurements instead of opinions. They need no root: a user can sandbox and score their own services, hash passwords, and validate configuration files without installing them. Outputs are from a real run (key fingerprints and user names are masked).

## Exercises

**1. Score a service, harden it, score again.** Start `sleep 60` as a transient user service twice: once plain and once with a set of sandboxing options. Use `systemd-analyze --user security UNIT` to get each **exposure level**. How many checks does each fail? Which options had the largest effect?

```text
$ systemd-run --user --unit=pr-plain /bin/sleep 60
$ systemd-run --user --unit=pr-hard \
    -p NoNewPrivileges=yes -p PrivateTmp=yes -p ProtectSystem=strict -p ProtectHome=yes \
    -p PrivateDevices=yes -p ProtectKernelTunables=yes -p ProtectControlGroups=yes \
    -p RestrictAddressFamilies=AF_UNIX -p RestrictNamespaces=yes \
    -p SystemCallFilter=@system-service -p CapabilityBoundingSet= \
    -p MemoryDenyWriteExecute=yes /bin/sleep 60
$ systemd-analyze --user security pr-plain | tail -1
→ Overall exposure level for pr-plain.service: 9.8 UNSAFE 😨
$ systemd-analyze --user security pr-hard | tail -1
→ Overall exposure level for pr-hard.service: 2.8 OK 🙂
```

**2. Why salt?** Hash the password `correct horse` with SHA-512 crypt twice using the salt `abcdefgh` and once with the salt `zyxwvuts`. What do you learn, and why must a password store never use an unsalted fast hash?

```text
$ openssl passwd -6 -salt abcdefgh 'correct horse' | cut -c1-40
$6$abcdefgh$yIZAF3gQPvtKZO/9qOJKffAKKbtS
$ openssl passwd -6 -salt zyxwvuts 'correct horse' | cut -c1-40
$6$zyxwvuts$FMe3XcjnCh.2/9mYKMo9U3AZfT50
$ openssl passwd -6 -salt abcdefgh 'correct horse' | cut -c1-40
$6$abcdefgh$yIZAF3gQPvtKZO/9qOJKffAKKbtS
```

**3. Don't lock yourself out.** A syntax error in `/etc/sudoers` can remove everyone's administrative access. How do you check a sudoers file for errors *without* installing it?

```text
$ cat ok
alice ALL=(ALL) ALL
bob ALL=(root) NOPASSWD: /usr/bin/systemctl restart nginx
$ cat bad
alice ALL=(ALL) ALL
bob ALL=(root NOPASSWD /usr/bin/id
$ visudo -cf ok
ok: parsed OK
$ visudo -cf bad
bad:2:15: syntax error
bob ALL=(root NOPASSWD /usr/bin/id
              ^~~~~~~~
```

**4. SSH defaults and keys.** What do the following client defaults mean for security, and what would you change? Which key type would you generate today?

```text
$ ssh -G example.com | grep -iE "^(passwordauthentication|pubkeyauthentication|stricthostkeychecking|hashknownhosts|identitiesonly|forwardagent) "
hashknownhosts yes
identitiesonly no
passwordauthentication yes
pubkeyauthentication true
stricthostkeychecking ask
forwardagent no
$ ssh-keygen -lf ed25519.pub | awk '{print $1, $NF}'
256 (ED25519)
$ ssh-keygen -lf rsa.pub | awk '{print $1, $NF}'      # generated with -b 3072
3072 (RSA)
```

## Answers

Try before opening.

<details>
<summary>Show the explanations</summary>

**1.** The plain service scores **9.8, UNSAFE**; the sandboxed one **2.8, OK**. `systemd-analyze security` inspects the unit's *configuration* and gives each missing restriction a weight, then combines them into an exposure score from 0 (fully sandboxed) to 10. The plain unit failed 76 of the checks (it can see the whole file system, create sockets of any family, use any system call, keep every capability, and so on) while the hardened one failed 20. The options that pay most: **`CapabilityBoundingSet=`** (empty) drops every capability; **`SystemCallFilter=@system-service`** applies a seccomp allow-list of ordinary service calls so exploit payloads cannot call `mount` or `ptrace`; **`ProtectSystem=strict`** makes the file system read-only; **`PrivateTmp`**, **`PrivateDevices`** and **`ProtectHome`** hide shared and sensitive paths; **`RestrictAddressFamilies=AF_UNIX`** forbids network sockets; **`NoNewPrivileges`** stops `setuid` escalation; **`MemoryDenyWriteExecute`** blocks writable-and-executable memory used by many exploits. The score is a **guide, not a guarantee**: a service must still *work* after hardening (a network daemon needs `AF_INET`), so tighten step by step and run `systemctl status` and your tests each time. The remaining 2.8 comes from the checks that are still failing (20 of them), for example restrictions this simple service could also use but that were not requested. Make this routine for every unit you write (see *Practice: Units, Limits and Timers*).

**2.** The same password with the same salt gives the **identical** hash (lines 1 and 3), and with a different salt a **completely different** one (line 2). A **salt** is random data stored with the hash and mixed into it, so two users with the same password get different stored values, and an attacker cannot precompute one table of hashes (a *rainbow table*) for all users, nor tell from the file who shares a password. The format is `$6$salt$hash`: `$6$` names the algorithm (SHA-512 crypt), then the salt, then the hash (shortened here). Salting is necessary but not sufficient: the hash must also be **slow and memory-hard** so guessing is expensive. SHA-512 crypt runs thousands of rounds; modern systems prefer **yescrypt** (the default on recent Debian, Fedora and Ubuntu), **bcrypt** or **argon2**, which cost far more per guess than a fast hash such as plain SHA-256 or MD5. Rules: never store passwords reversibly or hashed with one fast round, never invent your own scheme, and use a vetted library (`crypt(3)`, the PAM stack, or your framework's password hasher).

**3.** **`visudo -c -f file`** parses a file and reports `parsed OK` or the **line and column** of the first error (`bad:2:15`, with a caret under the offending text: the closing parenthesis after `root` is missing) without touching `/etc/sudoers`. Always edit the real file with **`visudo`** (or `sudoedit /etc/sudoers.d/name`), which locks it, runs this check on save and refuses to install a broken file. Prefer small files in `/etc/sudoers.d/` over editing the main file, keep a root shell open in another terminal while changing sudo rules, and follow **least privilege**: the rule for `bob` above lets him run *one* command as root with no password, which is better than `ALL`. Beware rules that allow editors, shells or programs with shell escapes (`vi`, `less`, `find`, `tar`): a user can break out of them to a root shell.

**4.** `passwordauthentication yes` allows password logins, which are guessable: where possible set `PasswordAuthentication no` and use keys (on the **server**, in `sshd_config`; this output is the client's setting). `pubkeyauthentication true` is correct. `stricthostkeychecking ask` prompts you the first time you meet a server; **verify the fingerprint** out of band rather than answering yes by reflex, and never use `no` in scripts (it removes protection against impersonation). `hashknownhosts yes` stores host names in `~/.ssh/known_hosts` as hashes so a stolen file does not list your servers. `forwardagent no` is the safe default: agent forwarding lets the remote server's administrators use your keys while you are connected, so enable it only for hosts you trust (prefer `ProxyJump` for hopping through a bastion). `identitiesonly no` lets the agent offer every key it holds to every server, which can trip login-attempt limits and leaks which keys you have; set `IdentitiesOnly yes` with an explicit `IdentityFile` per host. For key types, **ED25519** (256-bit, fast, small, hard to implement wrongly) is the default choice today; **RSA** is fine at **3072 bits or more** if you need compatibility, and RSA 1024 or DSA keys should never be used. Protect private keys with a passphrase and mode `600`, keep them out of repositories, and rotate keys when people leave.

</details>

## Key takeaways

1. `systemd-analyze security` measures a service's sandboxing; dropping capabilities, filtering system calls, protecting the file system and restricting sockets move a unit from UNSAFE to OK, but always verify that the service still works.
2. Password hashes need a unique salt and a deliberately slow algorithm (yescrypt, bcrypt, argon2), never a fast unsalted hash.
3. Check sudoers files with `visudo -cf` and edit with `visudo`; grant the narrowest command that does the job.
4. Prefer ED25519 (or RSA of 3072 bits or more), disable password logins on servers, verify host fingerprints, and avoid agent forwarding to untrusted hosts.
