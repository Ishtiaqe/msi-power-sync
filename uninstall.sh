#!/usr/bin/env bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== MSI Power Sync Daemon Uninstaller ===${NC}"

# Check for root privilege
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}Please run this uninstaller with sudo:${NC}"
    echo "sudo ./uninstall.sh"
    exit 1
fi

# Stop and disable service if running
echo -e "\n${BLUE}[1/3] Stopping and disabling msi-power-sync service...${NC}"
if systemctl is-active --quiet msi-power-sync 2>/dev/null || systemctl is-enabled --quiet msi-power-sync 2>/dev/null; then
    systemctl disable --now msi-power-sync || true
    echo "Service disabled and stopped."
else
    echo "Service is not active or enabled."
fi

# Remove files
echo -e "\n${BLUE}[2/3] Removing installed files...${NC}"
rm -f /usr/local/bin/msi-power-sync
rm -f /etc/systemd/system/msi-power-sync.service
echo "Files removed."

# Reload systemd
echo -e "\n${BLUE}[3/3] Reloading systemd configurations...${NC}"
systemctl daemon-reload

echo -e "\n${GREEN}=== Uninstallation Completed Successfully! ===${NC}"
