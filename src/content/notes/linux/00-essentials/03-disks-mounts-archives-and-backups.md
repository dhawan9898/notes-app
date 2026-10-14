---
title: "Disks, Mounts, Archives and Backups"
track: linux
module: Linux Essentials
moduleOrder: 0.5
order: 3
level: beginner
summary: "How storage appears as one file tree: devices, partitions, filesystems and mount points; checking space with df and du (including inodes), mounting and fstab, creating and verifying tar archives, compression, and incremental backups with rsync, using captured command output."
prerequisites:
  - Linux Command Line Basics
sources:
  - label: "Shotts — The Linux Command Line, Ch. 15 (Storage Media) and Ch. 18 (Archiving and Backup)"
    url: https://linuxcommand.org/tlcl.php
  - label: "man 8 mount, man 5 fstab, man 1 df, man 1 du, man 1 tar"
    url: https://man7.org/linux/man-pages/man8/mount.8.html
  - label: "GNU tar manual"
    url: https://www.gnu.org/software/tar/manual/tar.html
  - label: "rsync(1) manual"
    url: https://download.samba.org/pub/rsync/rsync.1
---

Linux presents all storage as one tree starting at `/`. A USB stick, a second disk or a network share does not get a drive letter; it is **mounted**: attached to a directory, so its files appear inside the tree. This note explains how to see what storage you have, how full it is, how to attach and detach more, and how to make archives and backups that you can trust. (The kernel side is in *The Block Layer, I/O Schedulers, SSDs and NVMe* and *ext4, XFS, Btrfs, Journaling and Durability (fsync)*; this is the practical front end.)

## The layers: device, partition, filesystem, mount point

1. A **block device** is a disk as the kernel sees it: `/dev/sda`, `/dev/nvme0n1`, `/dev/vda` in a virtual machine.
2. It may be split into **partitions** (`/dev/sda1`, `/dev/nvme0n1p2`).
3. A partition holds a **filesystem** (ext4, xfs, btrfs, vfat, ntfs) which organises files.
4. The filesystem is attached with `mount` at a **mount point**, an ordinary (usually empty) directory.

`lsblk` shows devices and where they are mounted, and `findmnt` shows what is mounted where:

```text
$ lsblk -o NAME,SIZE,TYPE,MOUNTPOINTS
NAME     SIZE TYPE MOUNTPOINTS
loop0   66.8M loop /snap/core24/2124
loop1  615.3M loop /snap/gnome-46-2404/168
...
$ findmnt -n -o TARGET,SOURCE,FSTYPE,OPTIONS /
/ /dev/sda1 ext4 rw,relatime,errors=remount-ro
```

Here `/` is `/dev/sda1` formatted as ext4, mounted read-write. (`loop` devices are disk images such as snap packages; real disks show as `sda`, `nvme0n1` and so on.) Always identify the right device with `lsblk` *before* running anything destructive: `/dev/sdb` today may be your USB stick and tomorrow your backup disk.

## How full is it? `df` and `du`

- `df -h` ("disk free") reports space **per filesystem**.
- `du -sh path` ("disk usage") reports space used by **a directory tree**.

```text
$ df -h /tmp | head -2
Filesystem      Size  Used Avail Use% Mounted on
tmpfs           9.8G  993M  8.8G  10% /tmp
$ du -sh project; du -h --max-depth=1 project | sort -h
328K	project
4.0K	project/docs
324K	project/src
328K	project
```

The `sort -h` trick sorts human-readable sizes, so the biggest directory ends up last: the quickest way to find what is eating a disk is `du -h --max-depth=1 / 2>/dev/null | sort -h | tail`, repeated one level deeper each time.

A disk can also be "full" without running out of space: every file needs an **inode** (its metadata record), and a filesystem has a fixed number of them. `df -i` shows that budget:

```text
$ df -i / | head -2
Filesystem      Inodes  IUsed   IFree IUse% Mounted on
/dev/sda1      6537216 870353 5666863   14% /
```

If `Use%` is fine but you get "No space left on device", check `df -i`: millions of tiny files (mail queues, caches, session files) can exhaust inodes. A third cause is deleted-but-still-open files: space is only returned when the last process closes the file (`lsof +L1` finds them).

## Mounting and unmounting

Desktop systems auto-mount removable media. On servers or by hand:

```bash
lsblk                                   # find the device, e.g. /dev/sdb1
sudo mkdir -p /mnt/usb
sudo mount /dev/sdb1 /mnt/usb           # attach it
ls /mnt/usb                             # its files are now here
sudo umount /mnt/usb                    # detach (fails if something is using it)
```

`umount` says "target is busy" if a shell is `cd`'d into it or a program has a file open; `fuser -vm /mnt/usb` or `lsof +f -- /mnt/usb` shows who. Always unmount before pulling a drive, or buffered writes may be lost.

To mount at every boot, add a line to **`/etc/fstab`**:

```text
# device (prefer UUID)                     mount point  type  options          dump pass
UUID=3f5a1c2e-8d7b-4e11-a1b2-0123456789ab  /data        ext4  defaults,noatime  0    2
```

Use `blkid` to find a partition's UUID, because device names such as `/dev/sdb1` can change between boots. A mistake in `fstab` can stop the system booting, so test with `sudo mount -a` (mounts everything listed; any error appears now, not at the next reboot) and keep a spare console or snapshot when editing remote machines. Useful options: `ro` (read-only), `noexec` (forbid running programs from there), `nofail` (do not hang boot if the device is missing), `noatime` (skip access-time updates for speed).

Formatting (`mkfs.ext4 /dev/sdb1`) **destroys everything on the partition**. Triple-check the device name; there is no undo.

## Archives with `tar` and compression with `gzip`

`tar` bundles many files into one archive file (it does not compress by itself); adding `z` compresses with gzip. The three commands to remember are **c**reate, **t**est (list) and e**x**tract:

```text
$ tar -czf project.tar.gz project && ls -lh project.tar.gz
-rw-r--r-- 1 alice staff 133K Oct 14 10:47 project.tar.gz
$ tar -tzf project.tar.gz | head -8
project/
project/docs/
project/docs/readme.txt
project/src/
project/src/data3.txt
project/src/data2.txt
project/src/data1.txt
$ mkdir restore && tar -xzf project.tar.gz -C restore && ls restore/project
docs
src
$ diff -r project restore/project && echo identical
identical
```

Read the flags as sentences: `-c` create, `-z` gzip, `-f` file name (must come right before the file name), `-t` list, `-x` extract, `-C dir` change to a directory first. **A backup you have never restored is only a hope**: the last two commands (extract elsewhere, then `diff -r`) prove the archive is complete. Other points:

- Use relative paths (`tar -czf x.tar.gz project`, not `/home/alice/project`), so extraction does not overwrite absolute locations.
- `-J` uses xz (smaller, slower), `-j` bzip2, and `--zstd` gives a good speed/size balance on newer systems.
- Compress single files with `gzip file` (replaces the file; add `-k` to keep it) and `gunzip`:

```text
$ gzip -k project/src/data1.txt && ls -l project/src/data1.txt*
-rw-r--r-- 1 alice staff 108894 Oct 14 10:47 project/src/data1.txt
-rw-r--r-- 1 alice staff  45014 Oct 14 10:47 project/src/data1.txt.gz
```

Text compresses well (here to 41%); already-compressed data (JPEG, MP4, ZIP) barely shrinks.

## Backups that copy only what changed: `rsync`

`rsync -a source/ dest/` copies a directory tree, preserving permissions, owners and times (`-a`, "archive"), and after the first run transfers **only the differences**. The first copy and a later change:

```text
$ rsync -a --delete project/ backup/ && ls backup
docs
src
$ echo "changed" >> project/docs/readme.txt; rsync -a --itemize-changes project/ backup/
>f.s....... docs/readme.txt
$ rsync -a --dry-run --itemize-changes project/ backup/
(nothing to copy: already in sync)
```

`--itemize-changes` lists what was transferred (here only `readme.txt`, whose size changed: the `s` in the code). Three habits prevent painful mistakes:

- **The trailing slash matters.** `project/` means "the *contents* of project"; `project` (no slash) means "the directory itself", creating `backup/project/`.
- **`--delete` makes the destination an exact mirror**, removing files that no longer exist in the source. That is what a mirror needs and what a mistaken command ruins; run with `--dry-run` first (`-n`) to preview.
- rsync also works over SSH: `rsync -a project/ user@host:/backups/project/`.

A sensible small-scale policy is the **3-2-1 rule**: three copies of the data, on two different kinds of media, one of them off-site, and a regular test restore. Snapshots (LVM, Btrfs, ZFS) are fast point-in-time copies but are *not* backups if they live on the same disk.

## Quick problem solver

| Symptom | Check |
|---|---|
| "No space left on device" | `df -h` (blocks), `df -i` (inodes), `du -h --max-depth=1 \| sort -h`, deleted-but-open files (`lsof +L1`) |
| Cannot unmount: "target is busy" | `fuser -vm /mountpoint`; leave the directory in your shell |
| Drive not showing up | `lsblk`, `dmesg \| tail`, is it formatted (`blkid`)? |
| Boot stops after editing fstab | boot to recovery, fix `/etc/fstab`; next time `mount -a` first and use `nofail` for non-essential disks |
| Archive extracted into the wrong place | list first with `tar -tf`; extract with `-C` into a new directory |

## Key takeaways

1. Storage is a layer cake (device, partition, filesystem) attached to the tree at mount points; `lsblk` and `findmnt` show it.
2. `df` measures filesystems and `du` measures directories; "full" can mean out of space or out of inodes.
3. Mount by UUID in `/etc/fstab`, test with `mount -a`, and unmount before removing media; `mkfs` destroys data.
4. `tar -czf`, `-tzf` and `-xzf` create, list and extract; verify a backup by restoring it and comparing with `diff -r`.
5. `rsync -a` copies only changes; mind the trailing slash, preview `--delete` with `--dry-run`, and follow the 3-2-1 rule.
