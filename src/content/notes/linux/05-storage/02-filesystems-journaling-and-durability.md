---
title: ext4, XFS, Btrfs, Journaling and Durability (fsync)
track: linux
module: Storage and Filesystems
moduleOrder: 5
order: 2
level: expert
summary: How the major Linux filesystems differ, what journaling and copy-on-write actually guarantee, why fsync is slow and necessary (with measurements), safe file-update patterns, and mount options and maintenance that matter.
prerequisites:
  - Filesystem Hierarchy, VFS, Inodes, Links and File Descriptors
  - The Block Layer, I/O Schedulers, SSDs and NVMe
  - Virtual Memory, Paging and the Page Cache
sources:
  - label: "Linux kernel docs — ext4 (Documentation/filesystems/ext4), XFS, Btrfs"
    url: https://docs.kernel.org/filesystems/ext4/index.html
  - label: "man 2 fsync, man 2 rename, man 2 open (O_SYNC, O_DSYNC, O_DIRECT), man 5 ext4, man 8 mkfs.xfs, man 8 xfs_repair, btrfs(8)"
    url: https://man7.org/linux/man-pages/man2/fsync.2.html
  - label: "Pillai et al. — All File Systems Are Not Created Equal - On the Complexity of Crafting Crash-Consistent Applications (OSDI 2014)"
  - label: "Rebello et al. — Can Applications Recover from fsync Failures? (USENIX ATC 2020); PostgreSQL \"fsyncgate\" 2018"
  - label: "Mathur et al. — The new ext4 filesystem - current status and future plans (Linux Symposium 2007)"
  - label: "Rodeh, Bacik, Mason — BTRFS - The Linux B-Tree Filesystem (ACM TOS 2013)"
---

## What a filesystem does

A filesystem turns the flat array of blocks of a block device into files, directories, permissions and names: **allocation** (which blocks hold which file), **metadata** (inodes, directories, timestamps, extended attributes), **caching and write-back**, and **crash consistency** (survive a power loss without corruption). Everything in the VFS note (inodes, dentries, links) is implemented here.

## The main filesystems

| | **ext4** | **XFS** | **Btrfs** |
|---|---|---|---|
| Origin | evolved from ext2/3 (1990s) | SGI IRIX, Linux since 2001 | Linux-native CoW (2009) |
| Allocation | extents, delayed allocation, block groups | extents in B+trees, **allocation groups** (AGs) for parallelism | CoW B-trees of extents |
| Crash consistency | metadata journal (`data=ordered`) | metadata write-ahead log (delayed logging) | copy-on-write + checksummed trees (no journal) |
| Data checksums | no (metadata checksums yes) | no (metadata CRCs yes) | **yes** (data and metadata) |
| Snapshots/clones | no | reflinks (`cp --reflink`) | **yes**: subvolumes, snapshots, send/receive |
| Compression/dedup | no | no | zstd/lzo compression, offline dedup |
| Multi-device | no (use md/LVM) | no | built-in RAID0/1/10 (RAID5/6 still flawed) |
| Shrink online/offline | offline shrink; online grow | **grow only** | both |
| Max file / fs | 16 TiB / 1 EiB (4 KiB blocks) | 8 EiB | 16 EiB |
| Strengths | ubiquitous, robust, low overhead, fast fsck | huge files/volumes, parallel I/O, scalability, quotas (project) | integrity, snapshots, flexible volumes |
| Weak spots | inode count fixed at mkfs, no checksums | cannot shrink; metadata-heavy small-file performance average | fragmentation on random overwrite (VMs/DBs), ENOSPC "balance" issues, complexity |

Others: **ZFS** (out-of-tree, CoW + volume manager + checksums; OpenZFS), **F2FS** (flash-optimized), **tmpfs**, **overlayfs**, **NFS/CIFS**, **FAT/exFAT/NTFS** for interchange, **ext3/ext2** legacy.

Rules of thumb: **ext4** for general purpose and simplicity; **XFS** for large files, high parallelism and RHEL defaults; **Btrfs** when you value snapshots/integrity (Fedora/openSUSE default); **ZFS** for storage servers. Whatever you choose, test with your workload.

### ext4 internals worth knowing

- Disk divided into **block groups** (bitmaps, inode table, data blocks) to keep related data close.
- **Extents** (start, length) replace block maps; **delayed allocation** postpones block assignment until writeback, reducing fragmentation; **multiblock allocator**.
- **htree** directories (hashed B-tree) for huge directories.
- Inode count set at `mkfs` (`-i bytes-per-inode`); reserved blocks default 5 % for root (`tune2fs -m 1`); `e2fsck`, `resize2fs`, `dumpe2fs -h`, `debugfs`, `filefrag -v`.

### XFS internals

- Divided into **allocation groups** with independent B+tree free-space and inode structures, enabling many parallel allocators. Inodes allocated dynamically (no "out of inodes"), `xfs_info /mnt`, `xfs_repair` (offline, unmounted), `xfs_growfs`, `xfs_fsr` (defrag), project quotas (per-directory), `reflink=1` (default now).
- Align to RAID geometry at mkfs (`su`/`sw`, auto-detected from md/LVM).

### Btrfs essentials

```bash
btrfs subvolume create /data/@home ; btrfs subvolume snapshot -r /data/@home /data/@home-snap
btrfs send /data/@home-snap | ssh backup btrfs receive /backup          # incremental with -p
btrfs scrub start /data ; btrfs fi usage /data ; btrfs balance start -dusage=50 /data
chattr +C /var/lib/db        # nodatacow for databases/VM images (also disables checksums & compression)
```

Pitfalls: metadata ENOSPC when chunks are fully allocated (run a filtered balance), degraded multi-device recovery, and RAID5/6 write hole.

## Journaling: what it does and doesn't promise

Without protection a crash mid-update leaves metadata inconsistent (a file in the directory but its inode not initialized), forcing a lengthy `fsck`. A **journal** (write-ahead log) records intended metadata changes first, commits them, then applies them in place; after a crash the log is **replayed** in seconds.

ext4 journal modes (`data=` mount option):

| Mode | Journals | Behavior |
|---|---|---|
| `ordered` (default) | metadata only; data blocks written **before** their metadata commits | no stale data exposed; metadata consistent |
| `writeback` | metadata only, no data ordering | fastest; after crash, files can contain stale/garbage blocks |
| `journal` | data + metadata | safest, up to 2× write cost |

Crucially, **journaling gives filesystem consistency, not application durability**. After a crash the filesystem is mountable and structurally sound, but recent writes may be *missing* or *partially applied* unless the application used the correct sync calls. XFS journals metadata only; **Btrfs/ZFS** achieve consistency by never overwriting live data (CoW) and atomically switching the root of the tree.

## The durability stack and why `fsync` is slow

A `write()` copies data into the **page cache** and returns: the data is in RAM only, readable by all, lost on power failure. To make it durable it must be pushed through: page cache → filesystem (allocate blocks, update journal) → block layer → device **volatile cache** → **flash/platters**. `fsync` forces all of this and finally sends a **cache flush (FLUSH/FUA)** to the device, then waits.

Measured on a virtual ext4 disk (4 KiB appends):

```text
write() only (page cache)      164,642 ops/s  (      6.1 us/op)
write()+fdatasync()                183 ops/s  (   5453.1 us/op)
write()+fsync()                    181 ops/s  (   5534.8 us/op)
```

That is ≈ **900× slower**: ~5.5 ms per durable commit (typical: consumer NVMe 0.5–3 ms for flush-heavy, enterprise NVMe with power-loss protection ~50–100 µs, HDD ~10 ms, cloud block storage 1–3 ms). It is also why **group commit** (batching many transactions into one `fsync`) is the core trick in databases and message queues, and why benchmarks that skip fsync are meaningless for durability claims.

| Call / flag | Guarantee |
|---|---|
| `fsync(fd)` | file data **and** metadata (size, timestamps) durable. POSIX does **not** promise that the *directory entry* of a newly created file is durable: `fsync` the parent directory too |
| `fdatasync(fd)` | data and the metadata needed to read it (e.g. size), skipping pure timestamps: cheaper |
| `O_SYNC` / `O_DSYNC` | each `write()` behaves like `fsync`/`fdatasync` |
| `O_DIRECT` | bypass page cache (aligned I/O); **not** a durability guarantee by itself: still needs a flush (`fdatasync`) for the device cache |
| `sync_file_range` | starts/waits writeback of a range; **no** metadata or flush guarantee: not a substitute for fsync |
| `msync(MS_SYNC)` | for mmap'ed regions |
| `sync` / `syncfs` | whole system / filesystem |
| `fsync` on the **parent directory** | makes a new/renamed/deleted entry durable |

### Safe ways to update a file

**Atomic replace** (config files, small state):

```c
int fd = open("config.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
write(fd, data, len);
fsync(fd);                               /* data durable BEFORE it becomes visible under the real name */
close(fd);
rename("config.tmp", "config");          /* atomic: readers see old or new, never partial */
int dfd = open(".", O_RDONLY | O_DIRECTORY);
fsync(dfd);                              /* make the rename itself durable */
close(dfd);
```

Without the first `fsync`, a crash can leave `config` pointing at a zero-length or partial file (ext4 added the `auto_da_alloc` heuristic to paper over the common truncate/rename pattern; do not rely on it). **Appending logs/WAL**: `write` + `fdatasync`; preallocate (`fallocate`) so size-changing metadata doesn't need syncing each time, and checksum records so torn tail writes are detectable at recovery.

### Error handling traps

- **`fsync` failure is serious.** If writeback fails (`EIO`), Linux marks the pages clean/error and reports the error *once*; a retry of `fsync` may return success although the data was lost ("fsyncgate", PostgreSQL 2018, driving a change to panic on fsync failure). Treat a failed `fsync` as a failed durable write; do not retry blindly, fail over or crash and recover from the log.
- `close()` can also return deferred write errors (NFS especially): check it.
- **Disk caches that lie**: consumer SSDs/RAID controllers without battery/capacitor and with write cache enabled can lose acknowledged data. Use `hdparm -W`/`nvme get-feature` to inspect the cache, enterprise parts with PLP, or disable write cache where integrity matters more than speed. Virtualization: ensure the hypervisor honors flushes (`cache=none`/`writeback` semantics).
- Network filesystems (NFS) relax semantics (close-to-open consistency); locking and `fsync` semantics differ.

## Mount options and maintenance

| Option | Effect |
|---|---|
| `noatime` / `relatime` (default) | avoid or throttle access-time updates (each read would otherwise cause a metadata write) |
| `nodev,nosuid,noexec` | hardening for `/tmp`, `/home`, data volumes |
| `discard` / periodic `fstrim` | TRIM policy |
| `data=ordered/journal/writeback`, `commit=5` | ext4 journal behavior; flush interval in seconds |
| `errors=remount-ro` / `panic` / `continue` | reaction to corruption; use `remount-ro` or `panic` + auto-reboot for HA clusters |
| `barrier` | modern kernels always issue flushes (barrier option obsolete) |
| `nofail`, `x-systemd.device-timeout=` | don't block boot if a data disk is missing |
| `usrquota,grpquota`, `prjquota` | quotas |
| `compress=zstd`, `subvol=@home` (btrfs) | compression, subvolume mounts |

Maintenance: `fsck` only on unmounted (or read-only) filesystems (journaling reduces the need; a clean journal replay is automatic); periodic `btrfs scrub` / `xfs_scrub` for latent errors; monitor `df -h` **and** `df -i`; keep ≥ 10–15 % free space for allocators (fragmentation, CoW); backups are the answer to corruption, not RAID or snapshots on the same device.

Diagnostics: `findmnt`, `stat -f /path`, `tune2fs -l /dev/X`, `dumpe2fs -h`, `xfs_info /mnt`, `filefrag -v file`, `lsattr`, `dmesg | grep -iE 'EXT4-fs|XFS|BTRFS'`, `iostat -x`, `strace -f -T -e trace=fsync,fdatasync,rename -p PID` to see where an application's time goes.

## Key takeaways

- ext4 = general-purpose journal; XFS = parallel/large-scale (cannot shrink); Btrfs = CoW with checksums and snapshots (watch fragmentation and balance).
- Journaling keeps *the filesystem* consistent; only `fsync`/`fdatasync` (plus a directory `fsync` for new names) make *your data* durable.
- `fsync` is ~1000× slower than a cached write (5.5 ms vs 6 µs measured): batch commits (group commit), preallocate, and measure with the real hardware.
- Update files atomically: write temp → `fsync` → `rename` → `fsync` directory. Treat `fsync` errors as fatal for that data.
- Check the device write cache and power-loss protection before trusting acknowledged writes; monitor space, inodes and mount errors.
