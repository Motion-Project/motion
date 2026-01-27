#!/bin/bash
# Motion Automated Installation Script
# PRODUCTION - Complete installation from source on Raspberry Pi
#
# This script automates the full installation workflow:
# 1. Install dependencies (pi-setup.sh)
# 2. Build Motion (pi-build.sh)
# 3. Install binaries (make install)
# 4. Configure authentication (motion-setup)
# 5. Set up systemd service (setup-motion-service.sh)
#
# Usage:
#   Interactive:  sudo scripts/install.sh
#   Unattended:   sudo scripts/install.sh --unattended --admin-pass <pass> --viewer-pass <pass>
#
# Requirements:
# - Raspberry Pi 4 or newer (64-bit)
# - Raspberry Pi OS Bookworm or newer
# - Internet connection (for package downloads)
# - Root privileges (sudo)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
UNATTENDED=false
ADMIN_PASS=""
VIEWER_PASS=""
VIEWER_USER="viewer"
SKIP_SERVICE=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --unattended)
            UNATTENDED=true
            shift
            ;;
        --admin-pass)
            ADMIN_PASS="$2"
            shift 2
            ;;
        --viewer-pass)
            VIEWER_PASS="$2"
            shift 2
            ;;
        --viewer-user)
            VIEWER_USER="$2"
            shift 2
            ;;
        --skip-service)
            SKIP_SERVICE=true
            shift
            ;;
        --help|-h)
            echo "Motion Installation Script"
            echo ""
            echo "Usage: sudo $0 [options]"
            echo ""
            echo "Options:"
            echo "  --unattended       Run without prompts (requires --admin-pass and --viewer-pass)"
            echo "  --admin-pass PASS  Set admin password (required for unattended)"
            echo "  --viewer-pass PASS Set viewer password (required for unattended)"
            echo "  --viewer-user USER Set viewer username (default: viewer)"
            echo "  --skip-service     Skip systemd service setup"
            echo "  --help, -h         Show this help message"
            echo ""
            echo "Examples:"
            echo "  Interactive:  sudo $0"
            echo "  Unattended:   sudo $0 --unattended --admin-pass secret123 --viewer-pass view123"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Check for root privileges
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}Error: This script must be run as root (use sudo)${NC}"
    exit 1
fi

# Validate unattended mode requirements (fail fast before any installation begins)
if [ "$UNATTENDED" = true ]; then
    if [ -z "$ADMIN_PASS" ] || [ -z "$VIEWER_PASS" ]; then
        echo -e "${RED}Error: --unattended requires --admin-pass and --viewer-pass${NC}"
        exit 1
    fi

    # Validate password length (minimum 8 characters recommended)
    if [ ${#ADMIN_PASS} -lt 8 ]; then
        echo -e "${RED}Error: Admin password must be at least 8 characters${NC}"
        exit 1
    fi

    if [ ${#VIEWER_PASS} -lt 8 ]; then
        echo -e "${RED}Error: Viewer password must be at least 8 characters${NC}"
        exit 1
    fi

    echo -e "${GREEN}✓ Password validation passed${NC}"
fi

# Detect Pi model
detect_pi_model() {
    if [ -f /proc/device-tree/model ]; then
        MODEL=$(tr -d '\0' < /proc/device-tree/model)
        if echo "$MODEL" | grep -q "Raspberry Pi 5"; then
            echo "5"
        elif echo "$MODEL" | grep -q "Raspberry Pi 4"; then
            echo "4"
        else
            echo "unsupported"
        fi
    else
        echo "unknown"
    fi
}

PI_MODEL=$(detect_pi_model)

echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║            Motion Automated Installation Script               ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# Check Pi model
case "$PI_MODEL" in
    5)
        echo -e "${GREEN}Detected: Raspberry Pi 5${NC}"
        ;;
    4)
        echo -e "${GREEN}Detected: Raspberry Pi 4${NC}"
        ;;
    unsupported)
        echo -e "${RED}Error: Unsupported Raspberry Pi model${NC}"
        echo "Motion 5.0 requires Raspberry Pi 4 or newer (64-bit)"
        exit 1
        ;;
    unknown)
        echo -e "${YELLOW}Warning: Could not detect Pi model${NC}"
        if [ "$UNATTENDED" = false ]; then
            read -p "Continue anyway? (y/N) " -n 1 -r
            echo
            if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                exit 1
            fi
        fi
        ;;
esac

echo ""
echo -e "${BLUE}Installation will perform the following steps:${NC}"
echo "  1. Install dependencies"
echo "  2. Build Motion from source"
echo "  3. Install binaries to /usr/local"
echo "  4. Configure authentication"
if [ "$SKIP_SERVICE" = false ]; then
    echo "  5. Set up systemd service"
fi
echo ""

if [ "$UNATTENDED" = false ]; then
    read -p "Continue with installation? (Y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Nn]$ ]]; then
        echo "Installation cancelled."
        exit 0
    fi
fi

# Step 1: Install dependencies
echo ""
echo -e "${BLUE}[1/5] Installing dependencies...${NC}"
echo "═══════════════════════════════════════════════════════════════"
cd "$PROJECT_DIR"
bash "$SCRIPT_DIR/pi-setup.sh"
echo -e "${GREEN}✅ Dependencies installed${NC}"

# Step 2: Build Motion
echo ""
echo -e "${BLUE}[2/5] Building Motion...${NC}"
echo "═══════════════════════════════════════════════════════════════"
cd "$PROJECT_DIR"
bash "$SCRIPT_DIR/pi-build.sh"
echo -e "${GREEN}✅ Build complete${NC}"

# Step 3: Install binaries and service
echo ""
echo -e "${BLUE}[3/5] Installing Motion...${NC}"
echo "═══════════════════════════════════════════════════════════════"
cd "$PROJECT_DIR"
make install
make install-service
echo -e "${GREEN}✅ Motion installed to /usr/local${NC}"
echo -e "${GREEN}✅ Systemd service file installed${NC}"

# Step 4: Configure authentication
echo ""
echo -e "${BLUE}[4/5] Configuring authentication...${NC}"
echo "═══════════════════════════════════════════════════════════════"

if [ "$UNATTENDED" = true ]; then
    # Unattended: use provided passwords
    echo "Setting up authentication with provided credentials..."

    # Use motion-setup with piped input
    printf '%s\n%s\n%s\n%s\n%s\n' \
        "$ADMIN_PASS" "$ADMIN_PASS" \
        "$VIEWER_USER" "$VIEWER_PASS" "$VIEWER_PASS" | \
        /usr/local/bin/motion-setup
else
    # Interactive: run motion-setup normally
    /usr/local/bin/motion-setup
fi
echo -e "${GREEN}✅ Authentication configured${NC}"

# Step 5: Set up systemd service
if [ "$SKIP_SERVICE" = false ]; then
    echo ""
    echo -e "${BLUE}[5/5] Setting up systemd service...${NC}"
    echo "═══════════════════════════════════════════════════════════════"
    bash "$SCRIPT_DIR/setup-motion-service.sh"

    # Enable and start service
    systemctl daemon-reload
    systemctl enable motion
    systemctl start motion

    sleep 3

    # Verify service is running
    if systemctl is-active --quiet motion; then
        echo -e "${GREEN}✅ Motion service is running${NC}"
    else
        echo -e "${YELLOW}⚠️  Motion service may not have started correctly${NC}"
        echo "Check logs with: sudo journalctl -u motion -n 50"
    fi
else
    echo ""
    echo -e "${YELLOW}[5/5] Skipping systemd service setup (--skip-service)${NC}"
fi

# Get IP address for display
IP_ADDR=$(hostname -I | awk '{print $1}')

echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║              ✅ INSTALLATION COMPLETE                         ║"
echo "╠═══════════════════════════════════════════════════════════════╣"
echo "║                                                               ║"
echo "║  Web UI: http://$IP_ADDR:8080/                          ║"
echo "║                                                               ║"
echo "║  Default credentials:                                         ║"
echo "║    Admin:  admin / (password you set)                         ║"
echo "║    Viewer: $VIEWER_USER / (password you set)                         ║"
echo "║                                                               ║"
echo "║  Useful commands:                                             ║"
echo "║    View logs:    sudo journalctl -u motion -f                 ║"
echo "║    Restart:      sudo systemctl restart motion                ║"
echo "║    Stop:         sudo systemctl stop motion                   ║"
echo "║    Reset pass:   sudo motion-setup --reset                    ║"
echo "║                                                               ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""
