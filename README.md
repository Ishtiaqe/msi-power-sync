# msi-power-sync

A lightweight, high-performance system daemon that synchronizes Linux system power profiles (managed by `power-profiles-daemon`) with MSI Embedded Controller (EC) performance shift modes (managed by the `msi-ec` driver) bidirectionally in real-time.

Developed in **C** using systemd's native `sd-bus` library and `poll()`, this daemon has an extremely small footprint:
- **Memory usage**: < 1 MB RAM.
- **CPU usage**: 0.0% (event-driven, blocks until either D-Bus emits a change signal or the driver's sysfs file notifies of a hardware shift).

---

## How It Works

The daemon synchronizes power settings **bidirectionally**:

1. **Ubuntu to MSI EC**: It listens to the system D-Bus for `PropertiesChanged` events from `net.hadess.PowerProfiles` (or `org.freedesktop.power-profiles-daemon`). Changing the profile in Ubuntu Settings instantly maps:
   - `performance` ➔ `turbo` (Extreme Performance)
   - `balanced` ➔ `comfort` (Balanced)
   - `power-saver` ➔ `eco` (Super Battery / Silent)

2. **MSI EC to Ubuntu (e.g., via MControlCenter)**: It monitors `/sys/devices/platform/msi-ec/shift_mode` via `poll()` on the sysfs file. Changing the mode via MControlCenter or hardware shortcuts instantly maps:
   - `turbo` ➔ `performance`
   - `comfort` ➔ `balanced`
   - `eco` ➔ `power-saver`

Both synchronization paths feature strict loop-prevention checks to prevent circular triggers.

---

## Installation

### Prerequisites

Ensure you have the updated `msi-ec` driver installed and loaded.

### One-Click Installation

1. Clone this repository and navigate into it:
   ```bash
   git clone https://github.com/Ishtiaqe/msi-power-sync.git
   cd msi-power-sync
   ```

2. Make the script executable and run the installer:
   ```bash
   chmod +x install.sh
   sudo ./install.sh
   ```

---

## Testing

1. Check the status of the sync daemon service:
   ```bash
   systemctl status msi-power-sync
   ```

2. Monitor real-time logs of the daemon:
   ```bash
   journalctl -u msi-power-sync -f
   ```

3. Open your Ubuntu Settings/Power panel, use the top status bar, or use **MControlCenter** to change the power mode. You should see the logs update instantly in both directions.

4. Verify that the hardware register was updated:
   ```bash
   cat /sys/devices/platform/msi-ec/shift_mode
   ```

---

## Removal

To completely remove the daemon from your system, run the uninstaller script:

```bash
chmod +x uninstall.sh
sudo ./uninstall.sh
```

---

## Credits

- **[msi-ec](https://github.com/BeardOverflow/msi-ec)**: The underlying Linux kernel driver providing userspace control over the MSI Embedded Controller.
- **[power-profiles-daemon](https://gitlab.freedesktop.org/hadess/power-profiles-daemon)**: The standard Linux power profile daemon.
