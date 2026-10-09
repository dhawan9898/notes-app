---
title: The Boot Process — Firmware, Bootloader, Kernel, initramfs and init
track: linux
module: Boot and Foundations
moduleOrder: 1
order: 1
level: intermediate
summary: Follow a Linux machine from power-on to a login prompt, what each stage does, the kernel command-line options that matter, and how to recover a system that will not boot.
sources:
  - label: "UEFI Specification 2.10 — Boot Manager, GPT, Secure Boot"
    url: https://uefi.org/specifications
  - label: "Linux kernel docs — Documentation/admin-guide/kernel-parameters.txt; initrd and initramfs (Documentation/filesystems/ramfs-rootfs-initramfs.rst)"
    url: https://docs.kernel.org/admin-guide/kernel-parameters.html
  - label: "The Linux Boot Protocol (Documentation/arch/x86/boot.rst)"
    url: https://docs.kernel.org/arch/x86/boot.html
  - label: "GNU GRUB Manual"
    url: https://www.gnu.org/software/grub/manual/grub/grub.html
  - label: "systemd — bootup(7), systemd-analyze(1), kernel-command-line(7)"
    url: https://man7.org/linux/man-pages/man7/bootup.7.html
  - label: "The Boot Loader Specification and UKI (Unified Kernel Image)"
    url: https://uapi-group.org/specifications/specs/boot_loader_specification/
---

Booting is a chain of hand-offs, each stage small enough to load the next: **firmware → bootloader → kernel → initramfs → init (PID 1) → services**. When something fails, knowing which link broke tells you where to look.

```mermaid
flowchart LR
  A["Power on: firmware (UEFI or BIOS), POST"] --> B["Bootloader: GRUB or systemd-boot"]
  B --> C["Kernel image + initramfs loaded"]
  C --> D["Kernel init: hardware, drivers, mount initramfs"]
  D --> E["initramfs /init: find and mount real root"]
  E --> F["switch_root, exec /sbin/init (systemd, PID 1)"]
  F --> G["Targets and services, login"]
```

## 1. Firmware

On power-on, the CPU starts at a fixed reset vector in firmware flash. The firmware runs **POST** (power-on self-test), initializes memory controllers, PCIe and storage, then finds something to boot.

### BIOS (legacy)

Reads the first 512-byte sector of the boot disk (the **MBR**: 446 bytes of boot code, 64 bytes of partition table, 2-byte signature `0x55AA`), executes it in 16-bit real mode, which loads a larger loader. Limited to MBR partitioning (2 TiB, 4 primary partitions).

### UEFI (modern)

The firmware understands **GPT** disks and **FAT** filesystems, and loads an **EFI application** (PE/COFF executable) directly:

- The **EFI System Partition (ESP)**, a FAT32 partition (type GUID `C12A7328-...`), typically mounted at `/boot/efi` or `/efi`, holds bootloaders at paths like `\EFI\ubuntu\shimx64.efi`, `\EFI\systemd\systemd-bootx64.efi`, or the fallback `\EFI\BOOT\BOOTX64.EFI`.
- Boot entries and their order live in **NVRAM variables** (`BootOrder`, `Boot0001`...). Manage them from Linux: `efibootmgr -v`, and see `ls /sys/firmware/efi` (exists only when booted via UEFI).
- **Secure Boot**: the firmware only runs binaries signed by keys in its database (`db`). Distros ship a Microsoft-signed **shim**, which verifies GRUB and the kernel against the distro key or locally enrolled keys (**MOK**, managed with `mokutil`). Unsigned third-party kernel modules (e.g. out-of-tree drivers) then fail to load unless signed. Kernel *lockdown* mode limits root's ability to modify the running kernel.
- **Measured boot**: each stage's hash can be extended into TPM PCRs; disk encryption can bind keys to those measurements (systemd-cryptenroll).

## 2. Bootloader

Its job: locate the kernel and initramfs, pass the **command line**, and jump to the kernel.

| Bootloader | Notes |
|---|---|
| **GRUB 2** | the usual default; reads its config `/boot/grub/grub.cfg` (generated; do not edit by hand). Edit `/etc/default/grub` then run `update-grub` (Debian/Ubuntu) or `grub2-mkconfig -o /boot/grub2/grub.cfg` (RHEL). Understands many filesystems, LVM, LUKS |
| **systemd-boot** | minimal UEFI-only, reads Type #1 entries from `/loader/entries/*.conf` on the ESP (Boot Loader Specification) |
| **UKI** | a *Unified Kernel Image*: kernel + initramfs + cmdline + signature in one EFI binary, which makes Secure Boot signing simple |
| ARM/embedded | U-Boot, with a **device tree** (`.dtb`) describing the hardware because ARM has no ACPI/PCI-style discovery on many boards |

At the GRUB menu press `e` to edit the entry for one boot (add options, change `init=`) — the standard recovery lever.

## 3. The kernel image and command line

The compressed kernel (`vmlinuz-*`, a `bzImage` on x86) decompresses itself, sets up paging and the CPU, and starts `start_kernel()`: initializes memory management, scheduler, interrupts, timers, then probes devices and drivers (built-in ones; the rest are modules loaded later). The log is in the ring buffer: `dmesg`, `journalctl -k`.

The **kernel command line** (`cat /proc/cmdline`) controls much of this. Frequently useful parameters:

| Parameter | Effect |
|---|---|
| `root=UUID=... rootfstype=ext4 ro` | the root filesystem (mounted read-only first, then remounted rw by init) |
| `init=/path` | run this instead of `/sbin/init` (e.g. `init=/bin/bash` for emergency access) |
| `systemd.unit=rescue.target` / `emergency.target` / `multi-user.target` | boot into a different systemd target |
| `quiet`, `loglevel=7`, `debug`, `earlyprintk` | verbosity |
| `console=ttyS0,115200n8 console=tty0` | where the kernel/boot console goes (serial for servers/VMs) |
| `nomodeset` | disable kernel mode-setting for GPUs (fix blank screens) |
| `panic=10` | reboot 10 s after a kernel panic |
| `rd.break` / `rd.shell` (dracut), `break=premount` (initramfs-tools) | drop to a shell inside the initramfs |
| `selinux=0` / `enforcing=0`, `apparmor=0` | disable MAC (debugging only) |
| `mem=`, `hugepages=`, `isolcpus=`, `nohz_full=`, `intel_iommu=on` | tuning/virtualization |
| `net.ifnames=0` | legacy `eth0` naming |

## 4. initramfs: the bridge to the real root

The real root filesystem may be on LVM, software RAID, an encrypted LUKS volume, iSCSI or NFS, all needing drivers and userspace tools that the kernel itself cannot run before a root filesystem exists. The bootloader therefore loads an **initramfs**: a gzip/zstd-compressed **cpio** archive that the kernel unpacks into a RAM-based filesystem (`rootfs`, tmpfs-like) and runs `/init` from it.

`/init` (a script, or systemd in the initramfs) will:

1. mount `/proc`, `/sys`, `/dev`;
2. load needed kernel modules (storage controller, filesystem, dm-crypt);
3. assemble RAID/LVM, unlock LUKS (prompting for a passphrase), find the root device by UUID;
4. mount the root filesystem on a temporary mount point;
5. **`switch_root`** (or `pivot_root`): make the real root the new `/`, free the initramfs memory, and `exec /sbin/init`.

Tools: `lsinitramfs /boot/initrd.img-$(uname -r)` (Debian) or `lsinitrd` (dracut) list contents; regenerate with `update-initramfs -u`, `dracut -f`, `mkinitcpio -P`. **A missing driver in the initramfs is the classic cause of "cannot find root device" / `Kernel panic - not syncing: VFS: Unable to mount root fs`.** Always regenerate it after changing storage, crypto, or RAID configuration.

## 5. PID 1: init

The kernel finally starts the first userspace process with **PID 1**: `/sbin/init`, usually a symlink to **systemd**. If it can't be executed the kernel panics ("Attempted to kill init!"). PID 1 is special: it adopts orphans, reaps them, ignores signals without handlers, and its death panics the kernel.

systemd reads its **default target** (`systemctl get-default`, usually `graphical.target` or `multi-user.target`) and starts units in parallel respecting dependencies: mounts from `/etc/fstab`, udev, networking, logging (journald), then your services and finally login prompts (getty) or a display manager. Diagram of key targets: `sysinit.target` → `basic.target` → `multi-user.target` → `graphical.target`. See the systemd notes.

## Boot performance and logs

```bash
systemd-analyze                   # firmware + loader + kernel + userspace time
systemd-analyze blame             # slowest units
systemd-analyze critical-chain    # the chain that actually gated the boot
journalctl -b -1                  # logs from the previous boot (needs persistent journal)
journalctl -b -p err              # errors since this boot
dmesg --level=err,warn            # kernel messages
```

## Recovery cookbook

| Situation | Approach |
|---|---|
| Forgot root password | edit the GRUB entry, append `init=/bin/bash` (or `rd.break` on RHEL, then `mount -o remount,rw /sysroot; chroot /sysroot; passwd; touch /.autorelabel`); remount `/` read-write, `passwd`, reboot (not possible on locked-down/encrypted systems) |
| Boots to emergency mode (bad fstab entry) | fix `/etc/fstab` (use `nofail`/UUIDs), `systemctl daemon-reload`, reboot |
| GRUB broken / missing | boot a live ISO, mount root (+ESP), `chroot`, `grub-install` + `update-grub` (BIOS: install to the disk; UEFI: `--efi-directory`) |
| Kernel panic after upgrade | boot the older kernel from the GRUB "Advanced options"; rebuild initramfs; check disk space on `/boot` |
| Root device not found | missing initramfs driver, changed UUID, RAID/LVM not assembled: use `rd.break`, then `lsblk`, `blkid`, `modprobe` |
| Filesystem errors | `fsck` from rescue mode (never on a mounted filesystem); `xfs_repair` for XFS |
| Stuck "A start job is running" | a unit waiting on a device/network: `systemctl list-jobs`; shorten timeouts or add `nofail` |

Generic live-system chroot:

```bash
mount /dev/vg0/root /mnt && mount /dev/sda1 /mnt/boot/efi
for d in dev proc sys run; do mount --rbind /$d /mnt/$d; done
chroot /mnt
```

## Key takeaways

- Boot chain: firmware (POST; UEFI loads from the ESP, BIOS from the MBR) → bootloader → kernel + initramfs → `/init` → real root → systemd as PID 1.
- The initramfs exists to reach the root device; regenerate it after storage or encryption changes.
- The kernel command line (`/proc/cmdline`) is the main debugging lever: `init=`, `systemd.unit=`, `rd.break`, `console=`.
- Secure Boot (shim, MOK) affects unsigned kernel modules; UKIs simplify signing.
- For recovery, know your three tools: the GRUB editor, rescue targets, and a live-ISO chroot.
