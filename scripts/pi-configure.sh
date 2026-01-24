#!/bin/bash
# Motion Configuration Script for Raspberry Pi
# PRODUCTION - Interactive configuration tool for Motion settings
#
# Can be run multiple times to update configuration
#
# Usage: scripts/pi-configure.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

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

# Detect if running headless (no display)
is_headless() {
    if [ -z "$DISPLAY" ] && ! systemctl is-active --quiet graphical.target; then
        return 0  # headless
    else
        return 1  # has display
    fi
}

# Prompt for yes/no with default
prompt_yesno() {
    local prompt="$1"
    local default="$2"
    local response

    if [ "$default" = "y" ]; then
        prompt="$prompt [Y/n]: "
    else
        prompt="$prompt [y/N]: "
    fi

    read -p "$prompt" response
    response=${response:-$default}

    case "$response" in
        [Yy]|[Yy][Ee][Ss]) return 0 ;;
        *) return 1 ;;
    esac
}

# Prompt for input with default
prompt_input() {
    local prompt="$1"
    local default="$2"
    local response

    if [ -n "$default" ]; then
        prompt="$prompt [$default]: "
    else
        prompt="$prompt: "
    fi

    read -p "$prompt" response
    echo "${response:-$default}"
}

# Prompt for password (hidden input)
prompt_password() {
    local prompt="$1"
    local password
    local password_confirm

    while true; do
        read -s -p "$prompt: " password
        echo ""
        read -s -p "Confirm password: " password_confirm
        echo ""

        if [ "$password" = "$password_confirm" ]; then
            if [ -z "$password" ]; then
                echo -e "${RED}Password cannot be empty${NC}"
                continue
            fi
            echo "$password"
            return 0
        else
            echo -e "${RED}Passwords do not match. Try again.${NC}"
        fi
    done
}

# Detect available cameras
detect_cameras() {
    if command -v rpicam-hello &> /dev/null; then
        rpicam-hello --list-cameras 2>&1 | grep -q "Available cameras" && return 0
    elif command -v libcamera-hello &> /dev/null; then
        libcamera-hello --list-cameras 2>&1 | grep -q "Available cameras" && return 0
    elif [ -e /dev/video0 ]; then
        return 0
    fi
    return 1
}

PI_MODEL=$(detect_pi_model)

echo -e "${BLUE}========================================"
echo " Motion Configuration Wizard"
echo "========================================${NC}"
echo ""

case "$PI_MODEL" in
    5)
        echo -e "${GREEN}Detected: Raspberry Pi 5${NC}"
        ;;
    4)
        echo -e "${GREEN}Detected: Raspberry Pi 4${NC}"
        ;;
    other)
        echo -e "${YELLOW}Detected: Raspberry Pi (older model)${NC}"
        ;;
    unknown)
        echo -e "${YELLOW}Warning: Not running on Raspberry Pi${NC}"
        ;;
esac

if is_headless; then
    echo -e "${BLUE}System: Headless (no display)${NC}"
    DEFAULT_NETWORK="y"
else
    echo -e "${BLUE}System: Desktop environment detected${NC}"
    DEFAULT_NETWORK="n"
fi

echo ""

# Check if Motion is installed
MOTION_BINARY=""
if [ -x "$PROJECT_DIR/src/motion" ]; then
    MOTION_BINARY="$PROJECT_DIR/src/motion"
    echo -e "${GREEN}Found Motion binary: $MOTION_BINARY${NC}"
elif command -v motion &> /dev/null; then
    MOTION_BINARY=$(command -v motion)
    echo -e "${GREEN}Found installed Motion: $MOTION_BINARY${NC}"
else
    echo -e "${RED}Error: Motion binary not found${NC}"
    echo "Please build Motion first:"
    echo "  cd $PROJECT_DIR && ./scripts/pi-build.sh"
    exit 1
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Network Access Configuration${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

if is_headless; then
    echo "Headless system detected - network access recommended"
    echo "This allows you to access Motion's web interface from other devices."
fi

if prompt_yesno "Enable network access to web interface?" "$DEFAULT_NETWORK"; then
    NETWORK_ACCESS="off"  # webcontrol_localhost off = network access enabled

    echo ""
    echo -e "${YELLOW}Security Notice:${NC}"
    echo "Network access requires authentication to prevent unauthorized access."
    echo ""

    WEB_USER=$(prompt_input "Web interface username" "admin")
    WEB_PASS=$(prompt_password "Web interface password")
    AUTH_METHOD="digest"

else
    NETWORK_ACCESS="on"  # webcontrol_localhost on = localhost only
    WEB_USER=""
    WEB_PASS=""
    AUTH_METHOD="none"
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Web Interface Configuration${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

WEB_PORT=$(prompt_input "Web interface port" "7999")

# Check if React UI is available
if [ -d "$PROJECT_DIR/data/webui" ] || [ -d "/usr/local/var/lib/motion/webui" ] || [ -d "/var/lib/motion/webui" ]; then
    if prompt_yesno "Use modern React UI?" "y"; then
        USE_REACT="y"
    else
        USE_REACT="n"
    fi
else
    echo -e "${YELLOW}React UI not found, will use legacy interface${NC}"
    USE_REACT="n"
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Camera Configuration${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

if detect_cameras; then
    echo -e "${GREEN}Camera detected${NC}"

    case "$PI_MODEL" in
        5)
            CAMERA_TYPE="libcam"
            echo "Pi 5 will use libcamera (required)"
            ;;
        4)
            if pkgconf --exists libcamera 2>/dev/null; then
                CAMERA_TYPE="libcam"
                echo "Will use libcamera"
            else
                CAMERA_TYPE="v4l2"
                echo "Will use V4L2"
            fi
            ;;
        *)
            CAMERA_TYPE="v4l2"
            echo "Will use V4L2"
            ;;
    esac
else
    echo -e "${YELLOW}Warning: No camera detected${NC}"
    CAMERA_TYPE="none"
fi

CAMERA_WIDTH=$(prompt_input "Camera width" "640")
CAMERA_HEIGHT=$(prompt_input "Camera height" "480")
CAMERA_FPS=$(prompt_input "Camera framerate" "15")

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Storage Configuration${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

TARGET_DIR=$(prompt_input "Media storage directory" "/var/lib/motion/media")

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Configuration Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "Network Access:   $([ "$NETWORK_ACCESS" = "off" ] && echo -e "${GREEN}Enabled${NC}" || echo -e "${YELLOW}Localhost only${NC}")"
if [ "$NETWORK_ACCESS" = "off" ]; then
    echo "  Username:       $WEB_USER"
    echo "  Auth Method:    $AUTH_METHOD"
fi
echo "Web Port:         $WEB_PORT"
echo "Interface:        $([ "$USE_REACT" = "y" ] && echo "React UI (modern)" || echo "Legacy HTML")"
echo "Camera:           $CAMERA_TYPE"
echo "  Resolution:     ${CAMERA_WIDTH}x${CAMERA_HEIGHT}"
echo "  Framerate:      ${CAMERA_FPS} fps"
echo "Storage:          $TARGET_DIR"
echo ""

if ! prompt_yesno "Create configuration file with these settings?" "y"; then
    echo "Configuration cancelled."
    exit 0
fi

# Determine config file location
CONFIG_FILE=""
if [ -w "/etc/motion" ]; then
    CONFIG_FILE="/etc/motion/motion.conf"
elif [ -w "/etc/motioneye" ]; then
    CONFIG_FILE="/etc/motioneye/motion.conf"
else
    CONFIG_FILE="$HOME/motion.conf"
fi

if [ -f "$CONFIG_FILE" ]; then
    if ! prompt_yesno "Config file exists at $CONFIG_FILE. Overwrite?" "n"; then
        CONFIG_FILE="${CONFIG_FILE}.new"
        echo "Will create new config at: $CONFIG_FILE"
    else
        # Backup existing config
        BACKUP="${CONFIG_FILE}.backup.$(date +%Y%m%d-%H%M%S)"
        cp "$CONFIG_FILE" "$BACKUP"
        echo "Backed up existing config to: $BACKUP"
    fi
fi

echo ""
echo "Creating configuration file: $CONFIG_FILE"

# Create target directory if it doesn't exist
sudo mkdir -p "$TARGET_DIR" 2>/dev/null || mkdir -p "$TARGET_DIR" 2>/dev/null || true

# Generate configuration file
cat > "$CONFIG_FILE" << EOF
# Motion Configuration File
# Generated by pi-configure.sh on $(date)
# Run scripts/pi-configure.sh to reconfigure

#*************************************************
#*****   System
#*************************************************
daemon off
log_level 6
log_type ALL

#*************************************************
#*****   Camera
#*************************************************
device_name Camera
target_dir $TARGET_DIR

#*************************************************
#*****   Source
#*************************************************
EOF

case "$CAMERA_TYPE" in
    libcam)
        echo "# Using libcamera (Raspberry Pi Camera Module)" >> "$CONFIG_FILE"
        echo "libcam_device 0" >> "$CONFIG_FILE"
        ;;
    v4l2)
        echo "# Using V4L2 (USB webcam or legacy Pi camera)" >> "$CONFIG_FILE"
        echo "v4l2_device /dev/video0" >> "$CONFIG_FILE"
        ;;
    none)
        echo "# No camera configured - set libcam_device or v4l2_device" >> "$CONFIG_FILE"
        echo ";libcam_device 0" >> "$CONFIG_FILE"
        echo ";v4l2_device /dev/video0" >> "$CONFIG_FILE"
        ;;
esac

cat >> "$CONFIG_FILE" << EOF

#*************************************************
#*****   Image
#*************************************************
width $CAMERA_WIDTH
height $CAMERA_HEIGHT
framerate $CAMERA_FPS

#*************************************************
#*****   Overlays
#*************************************************
text_right %Y-%m-%d\\n%T

#*************************************************
#*****   Method
#*************************************************
emulate_motion off
threshold 1500

#*************************************************
#*****   Picture
#*************************************************
picture_output off

#*************************************************
#*****   Movie
#*************************************************
movie_output off
movie_codec mp4
movie_quality 75

#*************************************************
#*****   Web Control
#*************************************************
webcontrol_port $WEB_PORT
webcontrol_localhost $NETWORK_ACCESS
webcontrol_parms 1
EOF

if [ "$NETWORK_ACCESS" = "off" ]; then
    cat >> "$CONFIG_FILE" << EOF
webcontrol_auth_method $AUTH_METHOD
webcontrol_authentication $WEB_USER:$WEB_PASS
webcontrol_lock_attempts 5
webcontrol_lock_minutes 10
EOF
else
    cat >> "$CONFIG_FILE" << EOF
webcontrol_auth_method none
EOF
fi

if [ "$USE_REACT" = "y" ]; then
    # Try to find webui directory
    if [ -d "$PROJECT_DIR/data/webui" ]; then
        WEBUI_PATH="$PROJECT_DIR/data/webui"
    elif [ -d "/usr/local/var/lib/motion/webui" ]; then
        WEBUI_PATH="/usr/local/var/lib/motion/webui"
    elif [ -d "/var/lib/motion/webui" ]; then
        WEBUI_PATH="/var/lib/motion/webui"
    else
        WEBUI_PATH=""
    fi

    if [ -n "$WEBUI_PATH" ]; then
        echo "webcontrol_html_path $WEBUI_PATH" >> "$CONFIG_FILE"
    fi
fi

cat >> "$CONFIG_FILE" << EOF

#*************************************************
#*****   Web Stream
#*************************************************
stream_port 0
stream_localhost on
EOF

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Configuration Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Config file: $CONFIG_FILE"
echo ""
echo "To start Motion:"
echo "  $MOTION_BINARY -c $CONFIG_FILE -n"
echo ""

if [ "$NETWORK_ACCESS" = "off" ]; then
    # Get primary IP address
    PRIMARY_IP=$(hostname -I | awk '{print $1}')
    echo "Access web interface at:"
    echo "  http://${PRIMARY_IP}:${WEB_PORT}"
    echo "  Username: $WEB_USER"
    echo ""
else
    echo "Access web interface at:"
    echo "  http://localhost:${WEB_PORT}"
    echo ""
fi

# Check if motion service exists
if systemctl list-unit-files | grep -q "^motion.service\|^motioneye.service"; then
    echo ""
    if prompt_yesno "Restart Motion service now?" "n"; then
        if systemctl is-active --quiet motion.service; then
            sudo systemctl restart motion
            echo -e "${GREEN}Motion service restarted${NC}"
        elif systemctl is-active --quiet motioneye.service; then
            sudo systemctl restart motioneye
            echo -e "${GREEN}MotionEye service restarted${NC}"
        fi
    fi
fi

echo ""
echo -e "${BLUE}To reconfigure Motion later, run:${NC}"
echo "  $SCRIPT_DIR/pi-configure.sh"
echo ""
