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

You need `gcc`, `make`, and systemd development headers installed (normally present on Ubuntu, but can be installed via):
```bash
sudo apt install build-essential libsystemd-dev
```

Additionally, ensure you have the updated `msi-ec` driver installed and loaded.

### Build and Install

1. Clone the repository and compile the daemon:
   ```bash
   gcc -O3 -Wall msi-power-sync.c -o msi-power-sync -lsystemd
   ```

2. Copy the binary and the systemd service file to the system locations:
   ```bash
   sudo cp msi-power-sync /usr/local/bin/msi-power-sync
   sudo cp msi-power-sync.service /etc/systemd/system/msi-power-sync.service
   ```

3. Reload systemd configurations, then enable and start the service:
   ```bash
   sudo systemctl daemon-reload
   sudo systemctl enable --now msi-power-sync
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

3. Open your Ubuntu Settings/Power panel or use the top bar to change the power mode (e.g., Performance, Balanced, Power Saver). You should see the log outputs:
   ```
   msi-power-sync: Ubuntu power profile changed to performance
   msi-power-sync: successfully set shift_mode to turbo
   ```

4. Verify that the hardware register was updated:
   ```bash
   cat /sys/devices/platform/msi-ec/shift_mode
   ```

---

## Removal

To completely remove the daemon from your system:

```bash
# Stop and disable the service
sudo systemctl disable --now msi-power-sync

# Remove the installed files
sudo rm -f /usr/local/bin/msi-power-sync
sudo rm -f /etc/systemd/system/msi-power-sync.service

# Reload systemd configuration
sudo systemctl daemon-reload
```

---

## Credits

- **[msi-ec](https://github.com/BeardOverflow/msi-ec)**: The underlying Linux kernel driver providing userspace control over the MSI Embedded Controller.
- **[power-profiles-daemon](https://gitlab.freedesktop.org/hadess/power-profiles-daemon)**: The standard Linux power profile daemon.
