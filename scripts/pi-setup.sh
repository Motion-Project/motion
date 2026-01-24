#!/bin/bash
# Motion Setup Script for Raspberry Pi
# PRODUCTION - Run on new Pi installations to install dependencies
#
# Auto-detects Pi model and configures accordingly
# Supports: Pi 4, Pi 5 (with appropriate camera stack)
# OS: Raspberry Pi OS Bookworm or newer
#
# Usage: sudo scripts/pi-setup.sh

set -e

# Detect Pi model
detect_pi_model() {
    if [ -f /proc/device-tree/model ]; then
        MODEL=$(tr -d '\0' < /proc/device-tree/model)
        if echo "$MODEL" | grep -q "Raspberry Pi 5"; then
            echo "5"
        elif echo "$MODEL" | grep -q "Raspberry Pi 4"; then
            echo "4"
        else
            echo "other"
        fi
    else
        echo "unknown"
    fi
}

PI_MODEL=$(detect_pi_model)

echo "========================================"
echo " Motion Setup Script"
case "$PI_MODEL" in
    5)
        echo " Detected: Raspberry Pi 5"
        echo " Camera: libcamera (required)"
        ;;
    4)
        echo " Detected: Raspberry Pi 4"
        echo " Camera: libcamera or V4L2"
        ;;
    other)
        echo " Detected: Raspberry Pi (older model)"
        echo " Camera: V4L2"
        ;;
    unknown)
        echo " Warning: Not running on Raspberry Pi"
        ;;
esac
echo "========================================"
echo ""

# Update system first
echo "[1/6] Updating system packages..."
sudo apt update && sudo apt upgrade -y

# Install build essentials
echo "[2/6] Installing build tools..."
sudo apt install -y \
    build-essential \
    autoconf \
    autoconf-archive \
    automake \
    libtool \
    pkgconf \
    gettext \
    git

# Install required dependencies
echo "[3/6] Installing required dependencies..."
sudo apt install -y \
    libjpeg-dev \
    libmicrohttpd-dev \
    libsystemd-dev \
    zlib1g-dev

# Install FFmpeg dependencies (required)
echo "[4/6] Installing FFmpeg dependencies..."
sudo apt install -y \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libavdevice-dev

# Install libcamera (requirements vary by Pi model)
case "$PI_MODEL" in
    5)
        echo "[5/6] Installing libcamera (required for Pi 5)..."
        sudo apt install -y \
            libcamera-dev \
            libcamera-tools \
            libcamera-v4l2
        ;;
    4)
        echo "[5/6] Installing libcamera (recommended for Pi 4)..."
        sudo apt install -y \
            libcamera-dev \
            libcamera-tools \
            libcamera-v4l2 || echo "  Warning: libcamera install failed, V4L2 will be used"
        ;;
    *)
        echo "[5/6] Skipping libcamera (not required for this model)..."
        echo "  Will use V4L2 for camera support"
        ;;
esac

# Install optional but recommended dependencies
echo "[6/7] Installing optional dependencies..."
sudo apt install -y \
    libsqlite3-dev \
    libwebp-dev \
    dos2unix

# Install Node.js for frontend build
echo "[7/7] Installing Node.js for frontend build..."
if ! command -v node &> /dev/null; then
    curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
    sudo apt install -y nodejs
else
    echo "  Node.js already installed: $(node --version)"
fi

echo ""
echo "========================================"
echo " Dependency Installation Complete!"
echo "========================================"
echo ""

# Verify installations
echo "Verifying installations..."

# Check libcamera (requirement depends on Pi model)
if pkgconf --exists libcamera; then
    LIBCAM_VER=$(pkgconf --modversion libcamera)
    echo "  libcamera: OK (version $LIBCAM_VER)"
else
    case "$PI_MODEL" in
        5)
            echo "  libcamera: MISSING - REQUIRED for Pi 5!"
            exit 1
            ;;
        4)
            echo "  libcamera: NOT INSTALLED - V4L2 will be used"
            ;;
        *)
            echo "  libcamera: NOT INSTALLED (V4L2 available)"
            ;;
    esac
fi

# Verify Node.js installation
if command -v node &> /dev/null; then
    echo "  Node.js: OK ($(node --version))"
    echo "  npm: OK ($(npm --version))"
else
    echo "  Node.js: MISSING - frontend build will fail"
fi

# Check for camera (rpicam-* tools on Trixie, libcamera-* on Bookworm)
echo ""
echo "Checking for connected cameras..."
if command -v rpicam-hello &> /dev/null; then
    echo "  Running rpicam-hello --list-cameras..."
    rpicam-hello --list-cameras 2>&1 || true
elif command -v libcamera-hello &> /dev/null; then
    echo "  Running libcamera-hello --list-cameras..."
    libcamera-hello --list-cameras 2>&1 || true
else
    echo "  No camera tools found (rpicam-hello or libcamera-hello)"
fi

echo ""
echo "========================================"
echo " Setup complete!"
echo ""
echo " Next steps:"
echo "   1. Transfer the motion source code to ~/motion"
echo "   2. Run: cd ~/motion && ./scripts/pi-build.sh"
echo "   3. Run: cd ~/motion && ./scripts/pi-configure.sh"
echo "========================================"
