---
title: "Start Here: How to Use the Linux Track"
track: linux
module: Start Here
moduleOrder: 0
order: 1
level: beginner
summary: "What Linux is, what to install to practise safely, three reading paths from first commands to kernel internals, and how the notes are organised."
sources:
  - label: "The Linux Kernel documentation"
    url: https://www.kernel.org/doc/html/latest/
  - label: "The Linux Documentation Project and man-pages project"
    url: https://man7.org/linux/man-pages/
  - label: "Shotts — The Linux Command Line (free book)"
    url: https://linuxcommand.org/tlcl.php
---

**Linux** is an operating system **kernel**: the program that manages the hardware, memory, processes, files and networking so that other programs can run. A **distribution** (Debian, Ubuntu, Fedora, Red Hat...) bundles the kernel with the shell, system programs, package manager and applications. Servers, phones (Android), routers, containers and supercomputers run Linux, so understanding it is part of almost every engineering job.

## What you need

A Linux system you may break. Any of these works: a virtual machine (VirtualBox, VMware, KVM), WSL2 on Windows, a spare computer, or a cloud VM. A container works for most of the early notes, but the later ones (boot, kernel, containers) need a real or virtual machine. Use a distribution you can get help with: Debian, Ubuntu or Fedora. The examples in these notes were run on Debian 13 (kernel 6.12); commands differ slightly on other distributions and the notes say where.

## Reading paths

| You are… | Read in this order |
|---|---|
| **New to Linux** | *Linux Command Line Basics* → *Installing Software and Managing Services* → *Practice: Linux Exercises* → the seven **Linux Essentials** notes (*Users, Groups and Permissions in Practice*, *Processes, Monitoring and Scheduled Jobs*, *Disks, Mounts, Archives and Backups*, *Network Commands and SSH for Beginners*, *Your First Shell Scripts*, *Editing Files and Version Control with Git*, *Reading Logs and Finding What Went Wrong*) → *The Shell* → *Text Processing and Robust Shell Scripting* → *Filesystem Hierarchy, VFS, Inodes, Links and File Descriptors* → *Users, Permissions, ACLs and Capabilities* |
| **Developer deploying on Linux** | the beginner notes → *Process Lifecycle* → *Threads and Synchronization* → *systemd* → *Linux Troubleshooting Playbook* → *Performance Methodology* |
| **Systems / SRE / kernel-minded** | *Virtual Memory* → *The Block Layer* → *Event-Driven I/O* → *eBPF* → *Profiling and Tracing* → containers (module 8) → *The Linux Packet Path* |

## How the notes are organised

The track follows the system from the bottom up: boot and files, the shell, processes, memory, storage, kernel interfaces, services, containers, performance and observability, the network stack, then security. Each note explains a mechanism, shows how to observe it on a real system (`/proc`, `strace`, `perf`, `ss`), and lists the failure modes you will meet in practice. Terms you do not know are in the [Glossary](../../../glossary/).

## Practising safely

- Work in a VM or container with a snapshot you can revert.
- Experiment in a scratch directory (`mktemp -d`) rather than your home directory.
- Learn to read manual pages (`man ls`; press `q` to quit, `/` to search).
- Prefer an ordinary user and `sudo` for single commands; avoid working as root. Many destructive commands (`rm -r`, `dd`, `mkfs`) have no undo.
- Many advanced notes use **namespaces**, which let you create isolated network and mount environments as an ordinary user, so labs cannot damage your real system.

## Key takeaways

1. Linux is the kernel; a distribution adds everything else.
2. Practise in a VM, in a scratch directory, with an ordinary user.
3. Follow a path: commands and packages first, then files and processes, then memory and I/O, then observability and containers.
