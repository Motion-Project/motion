#!/bin/bash
# Motion Service Setup Script
# PRODUCTION - Sets up systemd service and initial configuration
#
# Usage: sudo scripts/setup-motion-service.sh
# This file is part of Motion.
#
# Motion is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
MOTION_USER="${MOTION_USER:-$USER}"
MOTION_CONF_DIR="${MOTION_CONF_DIR:-/usr/local/etc/motion}"
MOTION_DATA_DIR="${MOTION_DATA_DIR:-/usr/local/var/lib/motion}"
MOTION_MEDIA_DIR="${MOTION_MEDIA_DIR:-$MOTION_DATA_DIR/media}"
ADMIN_USER="${ADMIN_USER:-admin}"
ADMIN_PASS="${ADMIN_PASS:-}"
VIEWER_USER="${VIEWER_USER:-viewer}"
VIEWER_PASS="${VIEWER_PASS:-}"
NO_ENABLE="false"
NO_START="false"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --user USER          System user to run motion as (default: $USER)"
    echo "  --admin-pass PASS    Password for admin webcontrol user"
    echo "  --viewer-pass PASS   Password for viewer webcontrol user"
    echo "  --media-dir DIR      Directory for captured media (default: $MOTION_MEDIA_DIR)"
    echo "  --no-enable          Don't enable service to start on boot"
    echo "  --no-start           Don't start service after setup"
    echo "  -h, --help           Show this help message"
    echo ""
    echo "Environment variables:"
    echo "  MOTION_USER          System user to run motion as"
    echo "  ADMIN_USER           Admin username (default: admin)"
    echo "  ADMIN_PASS           Admin password (required for authentication)"
    echo "  VIEWER_USER          Viewer username (default: viewer)"
    echo "  VIEWER_PASS          Viewer password (required for authentication)"
    exit 1
}

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This script must be run as root (use sudo)"
        exit 1
    fi
}

check_motion_installed() {
    if ! command -v motion &> /dev/null; then
        log_error "Motion is not installed. Run 'make install' first."
        exit 1
    fi
    MOTION_BIN=$(which motion)
    log_info "Motion found at $MOTION_BIN"
}

setup_directories() {
    log_info "Creating directories..."

    mkdir -p "$MOTION_CONF_DIR"
    mkdir -p "$MOTION_DATA_DIR"
    mkdir -p "$MOTION_MEDIA_DIR"
    mkdir -p "$MOTION_DATA_DIR/webui"

    # Set ownership
    if [[ -n "$MOTION_USER" ]] && id "$MOTION_USER" &>/dev/null; then
        chown -R "$MOTION_USER:$MOTION_USER" "$MOTION_DATA_DIR"
    fi
    chmod 755 "$MOTION_DATA_DIR"
    chmod 755 "$MOTION_MEDIA_DIR"

    log_info "Directories created and permissions set"
}

setup_config() {
    log_info "Setting up configuration..."

    local conf_file="$MOTION_CONF_DIR/motion.conf"

    # Copy default config if not exists
    if [[ ! -f "$conf_file" ]]; then
        if [[ -f "$MOTION_DATA_DIR/motion-dist.conf" ]]; then
            cp "$MOTION_DATA_DIR/motion-dist.conf" "$conf_file"
            log_info "Created $conf_file from distribution config"
        else
            log_error "No distribution config found at $MOTION_DATA_DIR/motion-dist.conf"
            exit 1
        fi
    else
        log_warn "Config file already exists, not overwriting: $conf_file"
        log_warn "Will still update authentication settings if passwords provided"
    fi

    # Update key settings
    sed -i.bak "s|^target_dir.*|target_dir $MOTION_MEDIA_DIR|" "$conf_file"
    sed -i.bak "s|^webcontrol_localhost.*|webcontrol_localhost off|" "$conf_file"

    # Set authentication if passwords provided
    if [[ -n "$ADMIN_PASS" ]]; then
        # Enable digest auth method
        sed -i.bak "s|^webcontrol_auth_method.*|webcontrol_auth_method digest|" "$conf_file"
        sed -i.bak "s|^; webcontrol_auth_method.*|webcontrol_auth_method digest|" "$conf_file"

        # Set admin authentication
        sed -i.bak "s|^webcontrol_authentication.*|webcontrol_authentication $ADMIN_USER:$ADMIN_PASS|" "$conf_file"
        sed -i.bak "s|^; webcontrol_authentication.*|webcontrol_authentication $ADMIN_USER:$ADMIN_PASS|" "$conf_file"
        log_info "Admin authentication configured (user: $ADMIN_USER)"
    fi

    if [[ -n "$VIEWER_PASS" ]]; then
        # Set viewer authentication
        sed -i.bak "s|^webcontrol_user_authentication.*|webcontrol_user_authentication $VIEWER_USER:$VIEWER_PASS|" "$conf_file"
        sed -i.bak "s|^; webcontrol_user_authentication.*|webcontrol_user_authentication $VIEWER_USER:$VIEWER_PASS|" "$conf_file"
        log_info "Viewer authentication configured (user: $VIEWER_USER)"
    fi

    # Comment out camera config line (use single-file config for simplicity)
    sed -i.bak "s|^camera |#camera |" "$conf_file"

    # Disable camera-level SQL to prevent relative path database errors
    # Force network access (override any camera-level localhost setting)
    cat >> "$conf_file" << 'EOF_SQL'

;*************************************************
;*****   Per-Camera SQL (disabled - use main config only)
;*************************************************
sql_event_start
sql_event_end
sql_movie_start
sql_movie_end
sql_pic_save

;*************************************************
;*****   Force network access (override camera defaults)
;*************************************************
webcontrol_localhost off
EOF_SQL

    log_info "Single-camera config mode enabled (network access enabled)"

    # Clean up backup files
    rm -f "$conf_file.bak"

    # Set permissions
    chmod 640 "$conf_file"
    if [[ -n "$MOTION_USER" ]] && id "$MOTION_USER" &>/dev/null; then
        chown root:"$MOTION_USER" "$conf_file"
    fi
}

setup_service() {
    log_info "Configuring systemd service..."

    # Check if service file exists
    if [[ ! -f "/etc/systemd/system/motion.service" ]]; then
        log_error "Service file not found. Run 'sudo make install-service' first."
        exit 1
    fi

    # Create service override for user
    mkdir -p /etc/systemd/system/motion.service.d
    cat > /etc/systemd/system/motion.service.d/override.conf << EOF
[Service]
User=$MOTION_USER
Group=video
ExecStart=
ExecStart=$MOTION_BIN -c $MOTION_CONF_DIR/motion.conf -n
EOF

    log_info "Service override created for user: $MOTION_USER"

    # Reload systemd
    systemctl daemon-reload
}

add_user_to_video_group() {
    if [[ -n "$MOTION_USER" ]] && id "$MOTION_USER" &>/dev/null; then
        if ! groups "$MOTION_USER" | grep -q "\bvideo\b"; then
            usermod -a -G video "$MOTION_USER"
            log_info "Added $MOTION_USER to video group for camera access"
        else
            log_info "User $MOTION_USER already in video group"
        fi
    fi
}

enable_service() {
    if [[ "$NO_ENABLE" != "true" ]]; then
        systemctl enable motion
        log_info "Service enabled to start on boot"
    else
        log_warn "Service not enabled (--no-enable specified)"
    fi
}

start_service() {
    if [[ "$NO_START" != "true" ]]; then
        systemctl start motion
        sleep 2
        if systemctl is-active --quiet motion; then
            log_info "Motion service started successfully"
        else
            log_error "Motion service failed to start. Check: journalctl -u motion -n 50"
            exit 1
        fi
    else
        log_warn "Service not started (--no-start specified)"
    fi
}

show_summary() {
    echo ""
    echo "======================================"
    echo "Motion Service Setup Complete"
    echo "======================================"
    echo ""
    echo "Configuration: $MOTION_CONF_DIR/motion.conf"
    echo "Media storage: $MOTION_MEDIA_DIR"
    echo "Running as:    $MOTION_USER"
    echo ""
    echo "Useful commands:"
    echo "  sudo systemctl status motion    # Check status"
    echo "  sudo systemctl restart motion   # Restart service"
    echo "  sudo journalctl -u motion -f    # View logs"
    echo ""
    # Check if authentication is configured (either via this script or motion-setup)
    local auth_configured=false
    if [[ -n "$ADMIN_PASS" ]]; then
        auth_configured=true
    elif grep -q "^webcontrol_authentication" "$MOTION_CONF_DIR/motion.conf" 2>/dev/null; then
        auth_configured=true
    fi

    if [[ "$auth_configured" == "true" ]]; then
        local port=$(grep "^webcontrol_port" "$MOTION_CONF_DIR/motion.conf" 2>/dev/null | awk '{print $2}' || echo "8080")
        echo "Web interface: http://$(hostname -I | awk '{print $1}'):$port/"
        echo "  Authentication: configured"
    else
        log_warn "No authentication configured. Run 'sudo motion-setup' or set --admin-pass for security."
    fi
    echo ""
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --user)
            MOTION_USER="$2"
            shift 2
            ;;
        --admin-pass)
            ADMIN_PASS="$2"
            shift 2
            ;;
        --viewer-pass)
            VIEWER_PASS="$2"
            shift 2
            ;;
        --media-dir)
            MOTION_MEDIA_DIR="$2"
            shift 2
            ;;
        --no-enable)
            NO_ENABLE="true"
            shift
            ;;
        --no-start)
            NO_START="true"
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            ;;
    esac
done

# Main execution
log_info "Motion Service Setup Starting..."
check_root
check_motion_installed
setup_directories
setup_config
add_user_to_video_group
setup_service
enable_service
start_service
show_summary
