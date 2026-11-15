#!/bin/bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}=== xHCI IMOD Disabler for Linux Installer ===${NC}"

# 1. Build userspace tool
echo -e "\n${CYAN}[1/3] Building userspace tool...${NC}"
make -C "$(dirname "$0")"

# 2. Install binary
echo -e "\n${CYAN}[2/3] Installing binary to /usr/local/bin...${NC}"
sudo make -C "$(dirname "$0")" install

# 3. Prompt for systemd service
echo -e "\n${CYAN}[3/3] Systemd autostart setup:${NC}"
read -p "Do you want to enable automatic IMOD disabling on boot via systemd? [y/N]: " choice
case "$choice" in 
  y|Y )
    sudo systemctl daemon-reload
    sudo systemctl enable --now xhci-imod.service
    echo -e "${GREEN}✔ Systemd service enabled!${NC}"
    ;;
  * )
    echo "Skipped systemd service enable."
    ;;
esac

echo -e "\n${GREEN}✔ Installation completed!${NC}"
echo "Run 'sudo xhci-imod-disabler' to open the interactive menu, or 'xhci-imod-disabler --status' to view controller info."
