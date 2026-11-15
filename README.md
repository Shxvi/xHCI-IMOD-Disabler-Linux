# xHCI IMOD Disabler for Linux

**English** | [Русский](README.ru.md)

Linux port and adaptation of **xHCI IMOD Disabler**.

Disables xHCI Interrupt Moderation (**IMOD**) on selected USB 3.x Host Controllers by setting the IMOD register interval to **`0`** (**`0 µs`** delay instead of the kernel/hardware default 40–50 µs).

Eliminating interrupt throttling allows high-polling-rate devices (1000 Hz, 4000 Hz, 8000 Hz mice and keyboards) and low-latency audio interfaces to deliver events to the CPU immediately without batching delays.

---

## ⚡ Features

- **Full Feature Parity with Windows Original:**
  - Modern interactive terminal TUI with arrow keys and number shortcuts.
  - Controller routing discovery: identifies **CPU-direct**, **Chipset**, **Thunderbolt**, and **Add-in cards**, along with hop count to the CPU.
  - Complete USB device tree: identifies gaming mice, keyboards, audio devices, hubs, speeds, and physical ports.
  - **Latency Optimization Tips:** highlights your gaming mouse, detects external USB hub routing, and recommends CPU-direct controller ports for minimum jitter.
  - Safe JSON backup and restore (`~/.config/xhci-imod/backup.json`).
  - Moderation testing mode (`16 ms / 62.5 Hz` to feel the latency difference).
  - Controller selection menu to include or exclude specific host controllers.
  - Headless `--silent` mode and ready-to-use **systemd service** for automatic application on system boot.
- **Dual Register Access Backends:**
  1. **Userspace MMIO (sysfs):** Direct access through `/sys/bus/pci/devices/<bdf>/resource0` when booted with `iomem=relaxed` (no custom drivers or compilation needed).
  2. **Companion Kernel Module (`xhci_imod`):** Clean kernel module using `ioremap` for systems where boot parameters cannot be changed, providing a `/sys/kernel/xhci_imod/` sysfs control interface.

---

## 🚀 Quick Start

### 1. Build the utility
```bash
make
```
The compiled binary `xhci-imod-disabler` will be built in the root directory.

### 2. View controller topology (No root required)
```bash
./xhci-imod-disabler --status
```
Displays all xHCI controllers, connected USB devices, your mouse position, and latency optimization tips.

---

## ⚙️ Register Access Setup (How to Disable IMOD)

In Linux, hardware MMIO access requires `sudo` privileges. Additionally, modern kernels protect active driver address ranges via `CONFIG_IO_STRICT_DEVMEM`. Choose one of the two solutions below:

### Method 1 (Recommended for Gaming): Kernel Parameter `iomem=relaxed`
Most Linux gaming distributions and latency tuners use this parameter. It unlocks direct userspace PCI MMIO access for root without compiling kernel modules.

#### 1. Add `iomem=relaxed` based on your bootloader:

- **CachyOS / Arch Linux (systemd-boot with UKI — Unified Kernel Images):**
  1. Open `/etc/kernel/cmdline` as root:
     ```bash
     sudo nano /etc/kernel/cmdline
     ```
  2. Append `iomem=relaxed` to the end of the line (separated by a space).
  3. **IMPORTANT: Rebuild your UKI boot image:**
     ```bash
     sudo mkinitcpio -P
     ```
     *(Without this command, the parameter is not embedded into the `.efi` binary and will not take effect upon reboot!)*

- **GRUB (Ubuntu / Debian / Linux Mint / Fedora / Arch with GRUB):**
  1. Open `/etc/default/grub`:
     ```bash
     sudo nano /etc/default/grub
     ```
  2. Append `iomem=relaxed` inside `GRUB_CMDLINE_LINUX_DEFAULT="..."`.
  3. Update GRUB:
     ```bash
     # Ubuntu / Debian / Linux Mint:
     sudo update-grub

     # Arch / Fedora / openSUSE:
     sudo grub-mkconfig -o /boot/grub/grub.cfg
     ```

- **systemd-boot (Standard entries, without UKI):**
  1. Open your boot entry file in `/boot/loader/entries/*.conf`.
  2. Append `iomem=relaxed` to the `options` line.

#### 2. Reboot and verify:
1. Reboot:
   ```bash
   sudo reboot
   ```
2. Verify that the parameter is active in the running kernel:
   ```bash
   cat /proc/cmdline | grep iomem
   ```
   > [!IMPORTANT]
   > You must see `iomem=relaxed` in the output. If it is missing, your bootloader or UKI image was not regenerated!

#### 3. Run with sudo:
```bash
sudo ./xhci-imod-disabler
```
Everything operates directly from userspace with zero external kernel drivers!

---

### Method 2: Kernel Module `xhci_imod` (No boot parameter changes needed)
If you prefer not to touch kernel command-line arguments:

1. Install kernel headers for your active kernel:
   - **Arch / CachyOS:** `sudo pacman -S linux-headers` (or `linux-cachyos-headers`)
   - **Ubuntu / Debian:** `sudo apt install linux-headers-$(uname -r)`
   - **Fedora:** `sudo dnf install kernel-devel`
2. Build and load the module:
   ```bash
   cd kernel-module
   make
   sudo make load
   ```
   The module immediately zeros out IMOD on all xHCI controllers upon loading.
3. Now `sudo ./xhci-imod-disabler` can read and write settings through `/sys/kernel/xhci_imod/`.

---

## 🖥 Usage

### Interactive Menu (TUI)
```bash
sudo ./xhci-imod-disabler
```
- **1. Controller Status & USB Topology:** Detailed view of controllers and connected devices.
- **2. IMOD Options:**
  - `1. Backup Current IMOD Values`: Save current values to JSON.
  - `2. Restore IMOD Values from Backup`: Restore previous intervals.
  - `3. Test IMOD (62.5 Hz / 16 ms)`: Apply high latency to feel the difference.
  - `4. Disable IMOD (0 µs)`: Set IMOD to 0 (Zero moderation).
  - `5. Set Custom IMOD Value`: Set arbitrary interval in 250 ns units.
- **3. Controller Selection:** Toggle individual controllers to include/exclude.
- **4. Startup Options:** Enable/disable the systemd autostart service.

### Command Line Interface (CLI)
- `sudo ./xhci-imod-disabler --status` — Display status and topology.
- `sudo ./xhci-imod-disabler --disable` — Disable IMOD on selected controllers.
- `sudo ./xhci-imod-disabler --restore` — Restore original IMOD values from backup.
- `sudo ./xhci-imod-disabler --test` — Apply test interval (16 ms).
- `sudo ./xhci-imod-disabler --set 0x00A0` — Set custom value (e.g., Linux default 40 µs).
- `sudo ./xhci-imod-disabler --silent` — Headless mode with exit code 0 on success.

---

## 🔄 Autostart on Boot (Systemd)

To automatically disable IMOD on every boot:

```bash
# 1. Install binary and service file
sudo make install

# 2. Enable and start systemd service
sudo systemctl enable --now xhci-imod.service
```

---

## 📈 Verification

1. Check controller status:
   ```bash
   sudo ./xhci-imod-disabler --status
   ```
   The `Current IMOD` line will show: **`Disabled (0 µs / 0x0000)`**.
2. If using the kernel module:
   ```bash
   cat /sys/kernel/xhci_imod/*/imod
   ```
3. Test your mouse polling rate and jitter using tools like `evhz` or `mouse-probe`.

---

## 📜 License

Licensed under the [GNU General Public License v3.0](LICENSE).
Based on the original [xHCI-IMOD-Disabler](https://github.com/QuakedK/xHCI-IMOD-Disabler) by QuakedK.
