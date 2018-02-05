#!/bin/bash
# Tested with a network camera VIVOTEK PZ81X1

# Just for debug
echo "---" ; env | grep TRACK | sort
exit 0

LOCKFILE=/tmp/on_camera_move_netcam.lock

if [ -e "$LOCKFILE" ]; then	# Trick to avoid flooding
  exit 0			# the netcam of multiple
fi				# moving commands.


function movecam() {
  # echo $1
  touch $LOCKFILE
  curl "http://10.42.0.96/cgi-bin/camctrl/camctrl.cgi?move=$1"
  LOCKED=true
}


case "$TRACK_ACTION" in
  "center")
    movecam home
    ;;
  "move")
    if [ "$TRACK_CENT_X" -lt "$((TRACK_IMGS_WIDTH*3/8))" ]; then
      movecam left
    fi
    if [ "$TRACK_CENT_X" -gt "$((TRACK_IMGS_WIDTH*5/8))" ]; then
      movecam right
    fi
    if [ "$TRACK_CENT_Y" -lt "$((TRACK_IMGS_HEIGHT*3/8))" ]; then
      movecam up
    fi
    if [ "$TRACK_CENT_Y" -gt "$((TRACK_IMGS_HEIGHT*5/8))" ]; then
      movecam down
    fi
    ;;
esac


if [ "$LOCKED" = "true" ]; then
  sleep 2
  rm -f "$LOCKFILE"
fi
