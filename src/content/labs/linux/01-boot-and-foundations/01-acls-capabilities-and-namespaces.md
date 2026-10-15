---
title: ACLs, Capabilities and Namespaces
track: linux
module: Boot and Foundations
moduleOrder: 2
order: 1
level: intermediate
minutes: 40
summary: "Six exercises on the foundations module that run without root: granting access to one extra user with ACLs and reading the mask, decoding capability bitmasks, seeing why root inside a user namespace has capabilities but no power over the host, and showing a private mount, plus reading the boot facts the kernel exposes."
prerequisites:
  - "Filesystem Hierarchy, VFS, Inodes, Links and File Descriptors"
  - "Users, Permissions, ACLs and Capabilities"
  - "The Boot Process — Firmware, Bootloader, Kernel, initramfs and init"
sources:
  - label: "man 5 acl, man 1 setfacl, man 1 getfacl"
    url: https://man7.org/linux/man-pages/man5/acl.5.html
  - label: "man 7 capabilities, man 1 capsh, man 7 user_namespaces, man 7 mount_namespaces"
    url: https://man7.org/linux/man-pages/man7/capabilities.7.html
  - label: "systemd-analyze(1) and bootup(7)"
    url: https://www.freedesktop.org/software/systemd/man/latest/bootup.html
---

The standard permission bits give you one owner, one group and everyone else. Access control lists, capabilities and namespaces are the three mechanisms that go beyond that, and all three can be explored by an ordinary user. Run each command and compare with the output (user and host names are replaced; capability values are from a real run).

## Exercises

**1. One extra reader.** Create `report.txt` with mode `640`. Give the existing user `nobody` read access *without* changing the owner, group or the other bits. How do you see it, and what changed in `ls -l`?

**2. The mask.** What is the ACL **mask**, why did it appear, and what happens to the effective permission of `nobody` when you set the mask to `---`? How do you remove one entry, and all extended entries?

**3. Default ACLs.** Make new files created inside a directory `proj` inherit group write access. Which kind of ACL entry does that?

**4. Decode capabilities.** Decode the bitmasks `000001ffffffffff` and `0000000000003000`. What are the effective capabilities of your own shell, and what is meant by the *bounding set*?

**5. Root that is not root.** Run `unshare -Ur` to become "root" in a new user namespace. Which capabilities do you hold there, and what can you still not do on the host (try `cat /etc/shadow`)? Why is this safe?

**6. A private mount, and boot facts.** In a new mount namespace, mount a `tmpfs` over a directory and write a file into it; what does the rest of the system see? Then show the default boot target, the kernel version and how long the last boot took in kernel versus user space.

## Answers

Try before opening.

<details>
<summary>Show answers and captured output</summary>

**1.**

```text
$ echo secret > report.txt; chmod 640 report.txt
$ getfacl -c report.txt
user::rw-
group::r--
other::---
$ setfacl -m u:nobody:r report.txt && getfacl -c report.txt
user::rw-
user:nobody:r--
group::r--
mask::r--
other::---
$ ls -l report.txt | cut -c1-11
-rw-r-----+
```

`setfacl -m u:nobody:r` **m**odifies the ACL by adding a named-user entry (`user:nobody:r--`); the owner, owning group and other entries stay as they were. `getfacl` shows the full ACL (`-c` hides the comment header). In `ls -l` the **`+`** after the mode bits is the only sign that an ACL exists, so a file that "looks" like `640` can grant access to people it does not list: always check `getfacl` when permissions seem inconsistent. ACLs need filesystem support (ext4, xfs and btrfs have it).

**2.**

```text
$ setfacl -m m::--- report.txt && getfacl -c report.txt | grep -E "nobody|mask"
user:nobody:r--	#effective:---
mask::---
$ setfacl -x u:nobody report.txt       # remove one entry
$ setfacl -b report.txt                # remove ALL extended entries
$ getfacl -c report.txt | head -4; ls -l report.txt | cut -c1-11
user::rw-
group::r--
other::---

-rw-r-----
```

The **mask** is the upper limit on the permissions of every entry except the owner and "other": the *effective* permission is the entry **ANDed with the mask**. It appeared automatically when the first named entry was added. Setting `m::---` leaves the entry written as `r--` but its `#effective:---` shows it grants nothing. The mask is also what `ls -l` shows in the **group** position of the mode when an ACL exists, which is why `chmod g-w` on such a file edits the mask and can silently remove access from named users. `-x` deletes one entry and `-b` returns the file to plain permission bits (the `+` disappears).

**3.**

```text
$ mkdir proj; setfacl -d -m g::rwx proj && getfacl -c proj | grep default
default:user::rwx
default:group::rwx
default:other::r-x
```

`-d` edits the **default ACL** of a directory: a template that new files and subdirectories **inherit** as their ACL (before the creating program's mode and `umask` are applied). It does not affect existing files. Use it for shared project directories, together with the setgid bit (see *Users, Groups and Permissions in Practice*), so that team members' files stay accessible to the team.

**4.**

```text
$ grep -E "^Cap(Eff|Bnd)" /proc/self/status
CapEff:	0000000000000000
CapBnd:	000001ffffffffff
$ capsh --decode=0000000000003000
0x0000000000003000=cap_net_admin,cap_net_raw
$ capsh --decode=000001ffffffffff
0x000001ffffffffff=cap_chown,cap_dac_override,cap_dac_read_search,cap_fowner,cap_fsetid,cap_kill,cap_setgid,cap_setuid,cap_setpcap,cap_linux_immutable,...
```

(The last line continues through all 41 capabilities.) Instead of one all-powerful "root", the kernel splits privilege into about 40 **capabilities** such as `cap_net_admin` (configure networking) or `cap_dac_override` (bypass file permission checks). Each is one bit in a mask, shown in hexadecimal: `0x3000` is bits 12 and 13, `cap_net_admin` and `cap_net_raw`; `0x1ffffffffff` is all 41 bits set. An ordinary shell has an **effective** set of zero (`CapEff` all zeros): it can use no privileged operation. The **bounding set** (`CapBnd`) is the *ceiling* of what this process tree could ever acquire (here everything, because it was inherited from init); container runtimes lower it to drop dangerous capabilities for good. A program can be given exactly one capability instead of full root with `setcap cap_net_bind_service=+ep binary` (to bind port 80, say), and `getcap -r /usr/bin` lists binaries that have such file capabilities.

**5.**

```text
$ unshare -Ur bash -c 'grep CapEff /proc/self/status; id -u'
CapEff:	000001ffffffffff
0
$ unshare -Ur cat /etc/shadow
cat: /etc/shadow: Permission denied
```

`unshare -Ur` creates a new **user namespace** and maps your own user ID to **0** inside it. Within the namespace you have every capability (`CapEff` is full) and `id -u` prints `0`. But capabilities are always relative to a namespace: they apply to the resources that *belong to that namespace* (its own network stack, mounts, hostname) and **not** to the host's files, processes or devices, which are still owned by the real UIDs. `cat /etc/shadow` is refused because the file belongs to the real root, and your real identity is still an unprivileged user. That is why unprivileged user namespaces are the foundation of rootless containers and are safe: they grant power only over things you created. (Some distributions restrict them for security reasons; if `unshare -Ur` fails with "Operation not permitted", an administrator has disabled it.)

**6.**

```text
$ echo "original file on disk" > hidden/note
$ unshare -Urm bash -c "mount -t tmpfs tmpfs hidden && echo inside > hidden/note && cat hidden/note"; cat hidden/note
inside
original file on disk
$ systemctl get-default
graphical.target
$ systemd-analyze | head -2
Startup finished in 3.064s (kernel) + 39.663s (userspace) = 42.728s
graphical.target reached after 39.663s in userspace.
$ cat /proc/sys/kernel/osrelease | cut -d- -f1
6.12.107+deb13
```

A **mount namespace** gives a process its own view of the mount table. Inside it, `mount -t tmpfs` covered `hidden/` with a RAM-backed file system containing `inside`; outside the namespace nothing changed (`cat` still shows the file on disk) and the mount vanishes when the last process in the namespace exits. This is how containers get their own root file system, and it is also how `ip netns` keeps its names. For the boot, `systemctl get-default` shows the **target** that boot aims for (`graphical.target`, or `multi-user.target` on servers), `systemd-analyze` splits the time into **kernel** (hardware, initramfs and mounting the root: about 3 s here) and **userspace** (starting services: about 40 s here), and `systemd-analyze blame` lists the slowest units and `systemd-analyze critical-chain` the chain that decided the total. `/proc/cmdline` shows the kernel parameters the bootloader passed, and `/sys/firmware/efi` exists only when the machine booted through UEFI.

</details>

## Key takeaways

1. ACLs add named users and groups beyond owner/group/other; the `+` in `ls -l` is the only hint, `getfacl` shows them, and the mask caps every entry except owner and other.
2. Default ACLs on a directory are inherited by new files; `setfacl -x` removes one entry and `-b` removes all.
3. Capabilities split root's power into about 40 bits; decode masks with `capsh --decode`, and note that an ordinary process has none effective.
4. Root in a user namespace holds capabilities only over resources of that namespace, which is what makes rootless containers safe.
5. A mount namespace gives a private mount table; `systemd-analyze` splits boot time into kernel and userspace.
