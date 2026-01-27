#!/bin/bash
# Motion Build Script for Raspberry Pi
# PRODUCTION - Build Motion from source on the Pi
#
# Auto-detects Pi model and configures accordingly
# Run this after pi-setup.sh has installed dependencies
#
# Usage: scripts/pi-build.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

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
echo " Motion Build Script"
echo " Project: $PROJECT_DIR"
case "$PI_MODEL" in
    5)
        echo " Target: Raspberry Pi 5 (libcamera)"
        ;;
    4)
        echo " Target: Raspberry Pi 4 (auto-detect camera)"
        ;;
    other)
        echo " Target: Raspberry Pi (V4L2)"
        ;;
    unknown)
        echo " Target: Generic (V4L2)"
        ;;
esac
echo "========================================"
echo ""

cd "$PROJECT_DIR"

# Fix Windows line endings if transferred from Windows
echo "[1/6] Fixing line endings (if from Windows)..."
if command -v dos2unix &> /dev/null; then
    find . -type f \( -name "*.sh" -o -name "configure*" -o -name "Makefile*" -o -name "*.ac" -o -name "*.am" -o -name "*.in" \) -exec dos2unix {} \; 2>/dev/null || true
    dos2unix scripts/* 2>/dev/null || true
fi

# Ensure scripts are executable
echo "[2/6] Setting executable permissions..."
chmod +x scripts/* 2>/dev/null || true
chmod +x configure 2>/dev/null || true

# Run autoreconf
echo "[3/6] Running autoreconf..."
autoreconf -fiv

# Configure with appropriate camera support
echo "[4/6] Configuring build..."

# Common configure options
COMMON_OPTS="--with-sqlite3 --with-webp --without-mysql --without-mariadb --without-pgsql --without-opencv --without-alsa --without-pulse --without-fftw3"

case "$PI_MODEL" in
    5)
        # Pi 5: Must use libcamera (legacy camera stack removed)
        echo "  Camera: libcamera (required)"
        ./configure --with-libcam --without-v4l2 $COMMON_OPTS
        ;;
    4)
        # Pi 4: Auto-detect - prefer libcamera if available, fallback to V4L2
        if pkgconf --exists libcamera 2>/dev/null; then
            echo "  Camera: libcamera (detected)"
            ./configure --with-libcam $COMMON_OPTS
        else
            echo "  Camera: V4L2 (libcamera not found)"
            ./configure --without-libcam $COMMON_OPTS
        fi
        ;;
    *)
        # Other/Unknown: Use V4L2
        echo "  Camera: V4L2"
        ./configure --without-libcam $COMMON_OPTS
        ;;
esac

# Build backend
echo "[5/6] Building backend (this may take a few minutes)..."
make -j4

# Build frontend
echo "[6/6] Building frontend..."
if command -v npm &> /dev/null; then
    cd "$PROJECT_DIR/frontend"
    npm install --silent
    npm run build
    cd "$PROJECT_DIR"
else
    echo "  WARNING: npm not found, skipping frontend build"
    echo "  Run scripts/pi-setup.sh to install Node.js"
fi

echo ""
echo "========================================"
echo " Build Complete!"
echo "========================================"
echo ""
echo " Backend: $PROJECT_DIR/src/motion"
echo " WebUI:   $PROJECT_DIR/data/webui/"
echo ""
echo " Next steps:"
echo ""
echo " 1. Configure Motion (interactive):"
echo "      ./scripts/pi-configure.sh"
echo ""
echo " 2. Or test with defaults (foreground, debug):"
echo "      ./src/motion -n -d"
echo ""
echo " 3. Or with a config file:"
echo "      ./src/motion -c motion.conf -n -d"
echo ""
echo " 4. Install system-wide (includes webui):"
echo "      sudo make install"
echo "========================================"
