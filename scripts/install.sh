#!/bin/bash
# ============================================================================
# MidiMind v3.0 - Installation Script
# ============================================================================

set -e

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║                                                               ║"
echo "║                  MidiMind v3.0 - Installer                    ║"
echo "║                                                               ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "⚠️  Please run as root (sudo ./install.sh)"
    exit 1
fi

# Detect system
echo "▶ Detecting system..."
ARCH=$(uname -m)
OS=$(uname -s)

echo "  Architecture: $ARCH"
echo "  OS: $OS"

# Check for Raspberry Pi
if [ -f /proc/device-tree/model ]; then
    MODEL=$(cat /proc/device-tree/model)
    echo "  Device: $MODEL"
fi

# Install dependencies
echo ""
echo "▶ Installing dependencies..."

if [ "$OS" = "Linux" ]; then
    # Update package list
    apt-get update
    
    # Install required packages
    apt-get install -y \
        build-essential \
        cmake \
        git \
        libasound2-dev \
        libsqlite3-dev \
        nlohmann-json3-dev \
        pkg-config
    
    echo "  ✓ Dependencies installed"
else
    echo "  ⚠️  Unsupported OS: $OS"
    exit 1
fi

# Create directories
echo ""
echo "▶ Creating directories..."

INSTALL_DIR="/home/pi/MidiMind"
mkdir -p $INSTALL_DIR/{config,data,midi/files,midi/recordings,logs,backups}

echo "  ✓ Directories created"

# Copy files
echo ""
echo "▶ Installing MidiMind..."

# Copy executable
cp build/midimind /usr/local/bin/
chmod +x /usr/local/bin/midimind

# Copy config files
cp config/config.json $INSTALL_DIR/config/
cp -r config/presets $INSTALL_DIR/config/
cp -r config/routes $INSTALL_DIR/config/

# Set permissions
chown -R pi:pi $INSTALL_DIR

echo "  ✓ Files installed"

# Install systemd service
echo ""
echo "▶ Installing systemd service..."

cp scripts/midimind.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable midimind

echo "  ✓ Service installed"

# Add user to audio group
echo ""
echo "▶ Configuring user permissions..."

usermod -aG audio pi

echo "  ✓ User configured"

# Done
echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║                                                               ║"
echo "║          ✓ MidiMind v3.0 installed successfully!             ║"
echo "║                                                               ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""
echo "Installation directory: $INSTALL_DIR"
echo "Executable: /usr/local/bin/midimind"
echo "Config: $INSTALL_DIR/config/config.json"
echo ""
echo "To start MidiMind:"
echo "  sudo systemctl start midimind"
echo ""
echo "To check status:"
echo "  sudo systemctl status midimind"
echo ""
echo "To view logs:"
echo "  sudo journalctl -u midimind -f"
echo ""
echo "To run manually:"
echo "  midimind"
echo ""