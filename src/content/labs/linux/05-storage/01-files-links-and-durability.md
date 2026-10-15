---
title: Files, Links and Durability
track: linux
module: Storage and Filesystems
moduleOrder: 5
order: 1
level: advanced
minutes: 35
summary: "Five exercises on the storage module: sparse files (apparent versus allocated size), hard links versus symbolic links and what deleting the original does, preallocation with fallocate, writing durably with fsync, and reading filesystem and inode information, with captured output."
prerequisites:
  - "The Block Layer, I/O Schedulers, SSDs and NVMe"
  - "ext4, XFS, Btrfs, Journaling and Durability (fsync)"
  - "Filesystem Hierarchy, VFS, Inodes, Links and File Descriptors"
sources:
  - label: "man 2 fsync, man 1 ln, man 1 truncate, man 1 fallocate, man 2 stat, man 1 stat"
    url: https://man7.org/linux/man-pages/man2/fsync.2.html
  - label: "Linux kernel documentation — ext4 filesystem"
    url: https://docs.kernel.org/filesystems/ext4/index.html
  - label: "Pillai et al. — All File Systems Are Not Created Equal: On the Complexity of Crafting Crash-Consistent Applications (OSDI 2014)"
---

Filesystems are easiest to understand by poking them. All of these exercises run as an ordinary user in a scratch directory (not `/tmp` if that is a RAM-backed tmpfs on your system; check with `df .`). Predict the result of each command before you run it.

## Exercises

**1. Two sizes.** Create a file with `truncate -s 1G sparse.img`. Compare `ls -lh`, `du -h` and `du -h --apparent-size`. Explain the difference. Name one real use of sparse files and one danger.

**2. Hard link or soft link?** Create `a.txt`, a hard link `hard.txt` and a symbolic link `soft.txt` to it. Show the inode numbers and link counts. Then delete `a.txt`: what happens to `hard.txt` and `soft.txt`, and why?

**3. Preallocate or sparse?** What is the difference between `truncate -s 16M f` and `fallocate -l 16M f`, as seen by `du`? Why would a database or a VM disk image prefer one over the other?

**4. Durable writes.** Write a 64 MiB file with `dd ... conv=fsync`. What does `fsync` guarantee that the plain `write` calls did not? What additional step does a program need after creating a *new* file to be sure the file's *name* survives a crash?

**5. Read the filesystem.** Use `stat -f` to show the filesystem type, block size and free blocks of the current directory, and `stat -c` to show a file's inode and link count. Which tool shows how many inodes remain?

## Answers

Try before opening.

<details>
<summary>Show answers and captured output</summary>

**1.**

```text
$ truncate -s 1G sparse.img && ls -lh sparse.img && du -h sparse.img && du -h --apparent-size sparse.img
-rw-r--r-- 1 alice staff 1.0G Oct 14 11:46 sparse.img
0	sparse.img
1.0G	sparse.img
```

`ls` reports the file's **logical size** (1 GiB: the highest offset that can be read); `du` reports the **disk blocks actually allocated** (zero). `truncate` just sets the length; the filesystem records a *hole* and allocates blocks only when data is written, and reads from a hole return zeros. `du --apparent-size` reports the logical size again. Uses: VM disk images, database files and swap that start large and fill gradually, and fast creation of big test files. Dangers: copying with a tool that does not understand holes (`cp` without `--sparse`, many archivers) can allocate the full 1 GiB; and the file system can run **out of space unexpectedly** when a sparse file is later filled, because the space was never reserved (thin provisioning has the same risk).

**2.**

```text
$ echo hello > a.txt && ln a.txt hard.txt && ln -s a.txt soft.txt
$ stat -c "%n: inode %i, links %h, size %s" a.txt hard.txt
a.txt: inode 570175, links 2, size 6
hard.txt: inode 570175, links 2, size 6
$ ls -li a.txt hard.txt soft.txt
570175 -rw-r--r-- 2 ... a.txt
570175 -rw-r--r-- 2 ... hard.txt
570177 lrwxrwxrwx 1 ... soft.txt -> a.txt
$ rm a.txt && cat hard.txt && cat soft.txt
hello
cat: soft.txt: No such file or directory
```

(The long `ls` columns are abbreviated.) A directory entry is just a **name pointing at an inode**; the inode holds the data and a link count. A **hard link** is a second name for the *same inode* (same number 570175, link count 2), so the data lives until the *last* name is removed: `hard.txt` still works. A **symbolic link** is a separate small file (its own inode, 570177) that stores a *path*; once `a.txt` no longer exists, the path leads nowhere and the link is **dangling**. Hard links cannot cross filesystems or point at directories; symlinks can do both. Deleting a file that a process still has open also keeps the inode alive until the descriptor closes, which is how "deleted" files can still occupy disk space.

**3.**

```text
$ fallocate -l 16M prealloc.bin && du -h prealloc.bin
16M	prealloc.bin
```

Unlike `truncate` (a hole: `du` shows 0), `fallocate` **reserves real blocks** immediately (`du` shows 16M), so later writes into the range cannot fail with "no space left on device" and are less likely to fragment the file. Databases use preallocation for write-ahead logs (predictable latency, no allocation on the write path) and VM images may choose preallocated format for the same reason; sparse images save space and creation time at the cost of surprises when the host fills. On filesystems that do not support the call, `fallocate` fails.

**4.**

```text
$ dd if=/dev/zero of=data.bin bs=1M count=64 conv=fsync status=none && ls -lh data.bin
-rw-r--r-- 1 alice staff 64M Oct 14 11:46 data.bin
```

Plain `write` calls only copy data into the **page cache** in RAM; the kernel writes it to the device later, so a power cut can lose it even though `write` returned success. `conv=fsync` makes `dd` call `fsync(fd)` before exiting: the call returns only after the file's data (and the metadata needed to read it) have been sent to stable storage and the device has acknowledged it, so the output is durable *when `dd` finishes*. For a newly created file there is one more step: the **directory entry** that names it is part of the *directory's* data, so call `fsync` on the **directory** too (open it with `open(dir, O_RDONLY)` and `fsync` that descriptor); otherwise a crash may leave data without a name. The safe pattern for replacing a file is: write a temporary file, `fsync` it, `rename` it over the original, then `fsync` the directory. (`oflag=direct` is a different thing: it bypasses the page cache, but does not by itself guarantee the device flushed its own cache.)

**5.**

```text
$ stat -f -c "filesystem type: %T, block size %S, free blocks %f of %b" .
filesystem type: ext2/ext3, block size 4096, free blocks 12385810 of 25631618
```

`stat -f` reports on the **filesystem** holding the path. The type shows as `ext2/ext3` because ext2, ext3 and ext4 share one on-disk magic number, so `stat` cannot tell them apart; `findmnt -no FSTYPE .` shows the real type (`ext4`). The block size is the allocation unit (4 KiB), so a 1-byte file occupies one block. For a file, `stat -c "%i %h"` gives the inode number and link count, as in exercise 2. To see how many **inodes** are left, use `df -i` (an inode shortage looks like "No space left on device" even when `df -h` shows free space, see *Disks, Mounts, Archives and Backups*).

</details>

## Key takeaways

1. `ls` shows logical size, `du` shows allocated blocks; sparse files make them differ and can run a filesystem out of space later.
2. A name is a directory entry pointing at an inode: hard links share the inode and survive deleting the original; symlinks store a path and dangle.
3. `fallocate` reserves blocks; `truncate` creates a hole. Preallocate where write latency and space guarantees matter.
4. `write` only reaches the page cache; durability needs `fsync` on the file, and for new files on the directory as well; replace files with write-temp, `fsync`, `rename`, `fsync` directory.
5. `stat -f` and `df -i` show filesystem type, block size, free blocks and free inodes.
