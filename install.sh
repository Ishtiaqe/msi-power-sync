#!/usr/bin/env bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== MSI Power Sync Daemon Installer ===${NC}"

# Check for root privilege
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}Please run this installer with sudo:${NC}"
    echo "sudo ./install.sh"
    exit 1
fi

# Detect package manager and install dependencies if missing
echo -e "\n${BLUE}[1/4] Checking and installing dependencies...${NC}"
if ! dpkg -s build-essential libsystemd-dev >/dev/null 2>&1; then
    echo "Installing build-essential and libsystemd-dev..."
    apt-get update && apt-get install -y build-essential libsystemd-dev
else
    echo "Dependencies are already installed."
fi

# Compile the daemon
echo -e "\n${BLUE}[2/4] Compiling msi-power-sync daemon...${NC}"
gcc -O3 -Wall msi-power-sync.c -o msi-power-sync -lsystemd
echo -e "${GREEN}Compilation successful.${NC}"

# Install files
echo -e "\n${BLUE}[3/4] Installing daemon and service configurations...${NC}"
cp msi-power-sync /usr/local/bin/msi-power-sync
cp msi-power-sync.service /etc/systemd/system/msi-power-sync.service
echo "Files copied successfully."

# Start service
echo -e "\n${BLUE}[4/4] Starting msi-power-sync system service...${NC}"
systemctl daemon-reload
systemctl enable --now msi-power-sync

echo -e "\n${GREEN}=== Installation Completed Successfully! ===${NC}"
echo -e "To check daemon status:  ${BLUE}systemctl status msi-power-sync${NC}"
echo -e "To view active logs:    ${BLUE}journalctl -u msi-power-sync -f${NC}"
