---
title: Kernel Modules, Device Drivers and udev
track: linux
module: Kernel Interfaces
moduleOrder: 6
order: 2
level: advanced
summary: How loadable modules extend the kernel, how devices and drivers find each other (the driver model, modaliases), how /dev nodes and permissions are created by udev, and the practical commands for loading, tuning, signing and debugging.
sources:
  - label: "Linux kernel docs — The Linux driver implementer's API guide; Driver model (Documentation/driver-api/driver-model)"
    url: https://docs.kernel.org/driver-api/driver-model/overview.html
  - label: "man 8 modprobe, man 8 lsmod, man 8 modinfo, man 5 modprobe.d, man 8 depmod"
    url: https://man7.org/linux/man-pages/man8/modprobe.8.html
  - label: "man 7 udev, man 8 udevadm, man 5 udev.rules"
    url: https://man7.org/linux/man-pages/man7/udev.7.html
  - label: "Linux kernel docs — Kernel module signing facility; Tainted kernels"
    url: https://docs.kernel.org/admin-guide/module-signing.html
  - label: "Corbet, Rubini, Kroah-Hartman — Linux Device Drivers, 3rd ed."
    url: https://lwn.net/Kernel/LDD3/
  - label: "DKMS — Dynamic Kernel Module Support"
    url: https://github.com/dell/dkms
---

Linux is a **monolithic kernel** (all core code in one address space at ring 0), but not a static one: features can be compiled **built-in** (`=y`), as **loadable modules** (`=m`, `.ko` files), or left out. Modules keep the kernel image small and let hardware and filesystem support be loaded on demand. A module runs with full kernel privileges: a bug crashes the machine and a malicious module owns it.

## Modules in practice

Modules live in `/lib/modules/$(uname -r)/` (`kernel/drivers/…`, `kernel/fs/…`, `modules.dep`, `modules.alias`, built by `depmod`). They are tied to the exact kernel build (**vermagic** string, symbol CRCs).

```bash
lsmod                                   # loaded modules, size, "used by" count and dependents (reads /proc/modules)
modinfo ext4                            # filename, description, license, depends, parameters, signer
modinfo -p e1000e                       # list parameters
modprobe nf_conntrack                   # load with dependencies (uses modules.dep)
modprobe -r nf_conntrack                # unload (fails if in use)
modprobe -n -v foo                      # dry run
insmod ./my.ko / rmmod my               # low level: no dependency resolution
dmesg | tail ; journalctl -k -f         # module messages
```

### Parameters

Set at load time (`modprobe mod param=value`), persistently in `/etc/modprobe.d/*.conf`, on the kernel command line (`mod.param=value` for built-ins/early), or at runtime via `/sys/module/<mod>/parameters/<param>` when writable:

```text
# /etc/modprobe.d/local.conf
options nf_conntrack hashsize=262144
options kvm_intel nested=1
blacklist nouveau                       # prevents *alias-based* autoloading only (explicit modprobe still works)
install usb_storage /bin/false          # stronger: refuse any load
alias char-major-10-229 fuse
```

Changes to modules needed in early boot require rebuilding the initramfs (`update-initramfs -u`, `dracut -f`).

### Autoloading

Hardware announces identifiers (PCI vendor:device, USB class, ACPI ID, device-tree `compatible`). The kernel builds a **modalias** string for each new device (`cat /sys/bus/pci/devices/0000:00:1f.6/modalias` → `pci:v00008086d…`); `udev` receives the event and runs `modprobe $MODALIAS`; `modules.alias` maps patterns to modules (`MODULE_DEVICE_TABLE` in the driver). Filesystems, crypto algorithms, protocols and netfilter pieces load on first use through `request_module()` (e.g. mounting `xfs` loads `xfs.ko`).

### Out-of-tree modules, DKMS, signing and taint

- **DKMS** rebuilds registered third-party modules (NVIDIA, ZFS, VirtualBox, wireguard on old kernels) against each newly installed kernel; needs matching **kernel headers** (`linux-headers-$(uname -r)`).
- **Module signing** (`CONFIG_MODULE_SIG`): with Secure Boot / kernel lockdown, only modules signed by a trusted key load. Enroll your own key (`mokutil --import`), sign with `kmodsign`/`scripts/sign-file`; unsigned loading fails with `Key was rejected by service`. `kernel.modules_disabled=1` is a one-way switch to forbid any further loading (hardening after boot).
- **Taint** (`cat /proc/sys/kernel/tainted`, 0 = clean): flags like `P` (proprietary module), `O` (out-of-tree), `E` (unsigned), `F` (forced load) mark kernels that upstream developers may refuse to debug; they appear in oops messages.

### A minimal module (illustrative)

```c
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
static int __init hello_init(void) { pr_info("hello: loaded\n"); return 0; }
static void __exit hello_exit(void) { pr_info("hello: unloaded\n"); }
module_init(hello_init); module_exit(hello_exit);
MODULE_LICENSE("GPL"); MODULE_AUTHOR("you"); MODULE_DESCRIPTION("demo");
```

```make
obj-m += hello.o
all:  make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
```

(Illustrative: building requires the kernel headers/build tree for your running kernel.) Kernel code has strict rules: no libc, no floating point, tiny stack (8–16 KiB), must handle concurrency and preemption, allocate with `kmalloc(…, GFP_KERNEL)` only in sleepable context (`GFP_ATOMIC` in interrupts), free everything, and never trust user pointers.

## The driver model: buses, devices and drivers

The kernel represents hardware as **devices** attached to **buses** (PCI, USB, platform, I²C, SPI, virtio), served by **drivers** registered with the bus; the bus **matches** devices to drivers (ID tables, device-tree `compatible`, ACPI), calls the driver's `probe()` and later `remove()`. Classes (`net`, `block`, `input`, `tty`) group devices by function. All of it is mirrored in sysfs.

```bash
lspci -nnk                                # PCI devices, their IDs and "Kernel driver in use / modules"
lsusb -t ; lsblk ; ip -br link
ls -l /sys/bus/pci/devices/0000:00:1f.6/driver     # symlink to the bound driver
echo 0000:03:00.0 > /sys/bus/pci/drivers/ixgbe/unbind
echo 0000:03:00.0 > /sys/bus/pci/drivers/vfio-pci/bind     # hand a device to VFIO (VM passthrough)
```

Related topics: **interrupts** (`/proc/interrupts`, MSI-X vectors per queue, affinity in `/proc/irq/N/smp_affinity`, `irqbalance`), **DMA** and the **IOMMU** (protects memory from devices, needed for VFIO/SR-IOV; `intel_iommu=on iommu=pt`), **firmware** blobs (`/lib/firmware`, `request_firmware`: "firmware: failed to load" lines in dmesg), **ACPI/device tree** hardware description, **power management** (runtime PM, `powertop`), **hotplug** (PCIe, USB, memory, CPU).

## Character and block device nodes

User space talks to drivers through **device nodes** in `/dev`: a node is an inode with type (char `c` or block `b`) and a **major:minor** pair; the major selects the driver (see `/proc/devices`), the minor selects the instance.

| Node | Meaning |
|---|---|
| `/dev/null` (1:3) | discards writes, reads EOF |
| `/dev/zero` (1:5) | endless zeros |
| `/dev/full` (1:7) | writes fail `ENOSPC` |
| `/dev/random`, `/dev/urandom` (1:8, 1:9) | kernel CSPRNG; both are fine for cryptography on modern kernels (≥ 5.6: `random` blocks only until initialized); `getrandom(2)` is preferred |
| `/dev/tty`, `/dev/pts/N`, `/dev/ptmx` | terminals and pseudo-terminals |
| `/dev/sda`, `/dev/nvme0n1p1`, `/dev/loopN`, `/dev/mapper/*` | block devices |
| `/dev/shm` | tmpfs for POSIX shared memory |
| `/dev/fuse`, `/dev/kvm`, `/dev/dri/card0`, `/dev/net/tun` | special-purpose |
| `/dev/stdin|stdout|stderr`, `/dev/fd/N` | symlinks into `/proc/self/fd` |

`/dev` itself is a **devtmpfs** the kernel populates automatically, and **udev** then adjusts ownership, permissions, and symlinks.

## udev

`systemd-udevd` is the userspace device manager:

1. The kernel emits a **uevent** (netlink `NETLINK_KOBJECT_UEVENT`) when a device is added/removed/changed, with properties (`ACTION=add`, `SUBSYSTEM=block`, `DEVPATH=…`, `MODALIAS=…`).
2. udev matches the event against **rules** in `/usr/lib/udev/rules.d/` (vendor), `/run/udev/rules.d/`, `/etc/udev/rules.d/` (local overrides, same filename wins), processed in **lexical order** of filenames (hence the `NN-` prefixes), and for each rule that matches executes assignments: set `NAME`, `SYMLINK+=`, `OWNER/GROUP/MODE`, `TAG`, `ENV{}`, `RUN+=` programs, import properties from helpers (`blkid`, `usb_id`, `path_id`), load modules.
3. It maintains a database in `/run/udev/data` and the persistent symlinks: `/dev/disk/by-uuid|by-label|by-id|by-path`, `/dev/input/by-id`.

```text
# /etc/udev/rules.d/60-usb-serial.rules
SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", ATTRS{serial}=="A1B2C3", \
    SYMLINK+="ttyRelayBoard", GROUP="dialout", MODE="0660", TAG+="systemd", ENV{SYSTEMD_WANTS}="relay.service"

# io scheduler per device type
ACTION=="add|change", KERNEL=="sd[a-z]", ATTR{queue/rotational}=="0", ATTR{queue/scheduler}="none"
```

Syntax: `==`/`!=` match, `=`/`+=`/`:=` assign; keys like `KERNEL`, `SUBSYSTEM`, `ATTR{}` (this device), `ATTRS{}` (this or any parent), `ENV{}`, `ACTION`, `DRIVER`. Test and debug:

```bash
udevadm info -a -n /dev/sda               # attributes of the device and all parents (to write matches)
udevadm info -q property -n /dev/sda      # DEVPATH, ID_SERIAL, ID_FS_UUID...
udevadm monitor --udev --kernel --property   # live events (plug something in)
udevadm test /sys/class/block/sda         # simulate rule processing and show which rules applied
udevadm control --reload ; udevadm trigger --subsystem-match=block --action=change
```

Network interface names (`enp3s0`, `wlp2s0`) are assigned by udev's `net_setup_link` using `systemd.link` files (predictable naming based on firmware index/PCI slot/MAC); override with `/etc/systemd/network/10-lan0.link` (`[Match] MACAddress=…` `[Link] Name=lan0`) or `net.ifnames=0`.

Pitfalls: long-running `RUN+=` programs block event handling (use `systemd-run`/`SYSTEMD_WANTS`), race conditions at boot (a rule fires before dependencies exist), device renaming inside containers, and expecting device names (`sdb`) to stay stable.

## Kernel configuration and versions

`uname -r` (e.g. `6.12.107+deb13-amd64`), `/boot/config-$(uname -r)` or `/proc/config.gz` list `CONFIG_*` options. Building a custom kernel: `make olddefconfig` (reuse the current config), `make menuconfig`, `make -j$(nproc)`, `make modules_install install` (updates `/boot`, initramfs and GRUB). Long-term support (LTS) kernels receive years of fixes; distro kernels backport patches, so version numbers alone don't tell what's fixed. **Live patching** (`kpatch`, `livepatch`) applies security fixes without reboot; **kdump** captures a crash dump on panic for analysis with `crash`.

## Key takeaways

- Modules are code loaded into ring 0 on demand; `modprobe` resolves dependencies, `modinfo` shows parameters, `/etc/modprobe.d` persists them, and the initramfs must be rebuilt for early-boot changes.
- Devices and drivers meet through buses and modaliases; sysfs mirrors the model, and unbind/bind lets you reassign drivers.
- `/dev` nodes (major:minor) are created by devtmpfs; udev rules (lexical order, `/etc` overrides `/usr/lib`) set permissions, names, symlinks and trigger actions.
- Secure Boot and lockdown require signed modules; DKMS rebuilds third-party ones; the taint flags record deviations.
- Debug with `udevadm info/monitor/test`, `lspci -k`, `dmesg`, and keep rules fast and idempotent.
