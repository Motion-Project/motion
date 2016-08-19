#!/bin/sh
# postinst script for motion
# Made by Angel Carpintero

set -e

. /usr/share/debconf/confmodule

add_group_if_missing() {
    if ! getent group motion >/dev/null; then
        addgroup --system --force-badname motion || true
    fi
}

add_user_if_missing() {
    if ! id -u motion > /dev/null 2>&1; then
        mkdir -m 02750 -p /var/lib/motion
        adduser --system --home /var/lib/motion \
          --disabled-password \
          --force-badname motion \
          --ingroup motion
        adduser motion video
        chown motion:adm /var/lib/motion
    fi
}

add_group_if_missing
add_user_if_missing

db_stop

#DEBHELPER#

exit 0
