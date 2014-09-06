#!/bin/sh -e
#
# /etc/init.d/motion: Start the motion detection
#
### BEGIN INIT INFO
# Provides:	  motion
# Required-Start: $local_fs $syslog $remote_fs
# Required-Stop: $remote_fs
# Default-Start:  2 3 4 5
# Default-Stop: 0 1 6
# Short-Description: Start Motion detection
# Description: loads motion and assigns privileges
### END INIT INFO

# Ported to new debian way using sh and /lib/lsb/init-functions
# by Angel Carpintero <ack@telefonica.net>
# Modified by : Juan Angulo Moreno <juan@apuntale.com>
#               Eddy Petrisor <eddy.petrisor@gmail.com>
#               ArAge <ArAge@gmx.co.uk>

NAME=motion
PATH_BIN=/bin:/usr/bin:/sbin:/usr/sbin
DAEMON=/usr/bin/motion
PIDFILE=/var/run/$NAME.pid
DEFAULTS=/etc/default/$NAME
DESC="motion detection daemon"

ENV="env -i LANG=C PATH=$PATH_BIN"

. /lib/lsb/init-functions

test -x $DAEMON || exit 0

RET=0

[ -r "$DEFAULTS" ] && . "$DEFAULTS" || start_motion_daemon=yes


check_daemon_enabled () {
    if [ "$start_motion_daemon" = "yes" ] ; then
        return 0
    else
        log_warning_msg "Not starting $NAME daemon, disabled via /etc/default/$NAME"
        return 1
    fi

}


case "$1" in
  start)
    if check_daemon_enabled ; then
        if ! [ -d /var/run/motion ]; then
                mkdir /var/run/motion
        fi
        chown motion:motion /var/run/motion

	log_daemon_msg "Starting $DESC" "$NAME" 
if start-stop-daemon --start --oknodo --exec $DAEMON -b --chuid motion ; then
            log_end_msg 0
        else
            log_end_msg 1
            RET=1
        fi
    fi
    ;;

  stop)
    log_daemon_msg "Stopping $DESC" "$NAME"
    if start-stop-daemon --stop --oknodo --exec $DAEMON --retry 30 ; then
        log_end_msg 0
    else
        log_end_msg 1
        RET=1
    fi
    ;;

  reload|force-reload)
    log_daemon_msg "Reloading $NAME configuration"
    if start-stop-daemon --stop --signal HUP --exec $DAEMON ; then
        log_end_msg 0
    else
        log_end_msg 1
        RET=1
    fi
    ;;

  restart-motion)
    if check_daemon_enabled ; then
        log_action_begin_msg "Restarting $NAME"
        if $0 stop && $0 start ; then
            log_action_end_msg 0
        else
            log_action_cont_msg "(failed)"
            RET=1
        fi
    fi
    ;;

  restart)
    $0 restart-motion
    ;;

  *)
    echo "Usage: /etc/init.d/$NAME {start|stop|restart|reload}"
    RET=1
    ;;
esac


exit $RET
