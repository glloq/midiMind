#!/bin/bash
# ============================================================================
# MidiMind v3.0 - Uninstallation Script
# ============================================================================

set -e

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║                                                               ║"
echo "║                MidiMind v3.0 - Uninstaller                    ║"
echo "║                                                               ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "⚠️  Please run as root (sudo ./uninstall.sh)"
    exit 1
fi

# Confirm
read -p "Are you sure you want to uninstall MidiMind? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Uninstallation cancelled."
    exit 0
fi

# Stop service
echo "▶ Stopping service..."
systemctl stop midimind || true
systemctl disable midimind || true

# Remove service file
echo "▶ Removing service..."
rm -f /etc/systemd/system/midimind.service
systemctl daemon-reload

# Remove executable
echo "▶ Removing executable..."
rm -f /usr/local/bin/midimind

# Ask about data
read -p "Remove data directory (/home/pi/MidiMind)? (y/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "▶ Removing data..."
    rm -rf /home/pi/MidiMind
else
    echo "  Data directory preserved."
fi

echo ""
echo "✓ MidiMind uninstalled successfully!"
echo ""