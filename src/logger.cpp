/*
 *    This file is part of MotionPlus.
 *
 *    MotionPlus is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    MotionPlus is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with MotionPlus.  If not, see <https://www.gnu.org/licenses/>.
 *
 *    Copyright 2020-2021 MotionMrDave@gmail.com
 */
#include "motionplus.hpp"
#include "conf.hpp"
#include "util.hpp"
#include "logger.hpp"
#include <stdarg.h>
#include <syslog.h>

static int log_mode = LOGMODE_SYSLOG;
static FILE *logfile  = NULL;
static int log_level = LEVEL_DEFAULT;
static int log_type = TYPE_DEFAULT;

static const char *log_type_str[]  = {NULL, "COR", "STR", "ENC", "NET", "DBL", "EVT", "TRK", "VID", "ALL"};
static const char *log_level_str[] = {NULL, "EMG", "ALR", "CRT", "ERR", "WRN", "NTC", "INF", "DBG", "ALL"};
static struct ctx_motapp *log_motapp;  /*Used to access the parms mutex for updates*/

/** Returns index of log type or 0 if not valid type. */
static int log_get_type(const char *type)
{
    unsigned int i, ret = 0;
    unsigned int maxtype = sizeof(log_type_str)/sizeof(const char *);

    for (i = 1;i < maxtype; i++) {
        if (!strncasecmp(type, log_type_str[i], 3)) {
            ret = i;
            break;
        }
    }

    return ret;
}

void log_set_type(const char *new_logtype)
{

    if ( mystreq(new_logtype, log_type_str[log_type]) ) {
        return;
    }

    pthread_mutex_lock(&log_motapp->mutex_parms);
        log_type = log_get_type(new_logtype);
    pthread_mutex_unlock(&log_motapp->mutex_parms);

}

void log_set_level(int new_loglevel)
{

    if (new_loglevel == log_level) {
        return;
    }

    pthread_mutex_lock(&log_motapp->mutex_parms);
        log_level = new_loglevel;
    pthread_mutex_unlock(&log_motapp->mutex_parms);

}

/** Sets mode of logging, could be using syslog or files. */
static void log_set_mode(int mode)
{
    int prev_mode = log_mode;

    log_mode = mode;

    if (mode == LOGMODE_SYSLOG && prev_mode != LOGMODE_SYSLOG) {
        openlog("motion", LOG_PID, LOG_USER);
    }

    if (mode != LOGMODE_SYSLOG && prev_mode == LOGMODE_SYSLOG) {
        closelog();
    }
}

/** Sets logfile to be used instead of syslog. */
static void log_set_logfile(const char *logfile_name)
{
    /* Setup temporary to let log if myfopen fails */
    log_set_mode(LOGMODE_SYSLOG);

    logfile = myfopen(logfile_name, "a");

    /* If logfile was opened correctly */
    if (logfile) {
        log_set_mode(LOGMODE_FILE);
    }

    return;
}

/** Return string with human readable time */
static char *str_time(void)
{
    static char buffer[16];
    time_t now = 0;

    now = time(0);
    strftime(buffer, 16, "%b %d %H:%M:%S", localtime(&now));
    return buffer;
}

/**
 *    This routine is used for printing all informational, debug or error
 *    messages produced by any of the other motion functions.
 */
void motion_log(int level, int type, int errno_flag,int fncname, const char *fmt, ...)
{
    int errno_save, n, prefixlen, timelen;
    char buf[1024]= {0};
    char usrfmt[1024]= {0};
    char msg_buf[100]= {0};

    va_list ap;
    int threadnr;

    static int flood_cnt = 0;
    static char flood_msg[1024];
    static char prefix_msg[1024];
    char flood_repeats[1024];
    char threadname[32];
    int  applvl, apptyp;

    pthread_mutex_lock(&log_motapp->mutex_parms);
        applvl = log_level;
        apptyp = log_type;
    pthread_mutex_unlock(&log_motapp->mutex_parms);

    /*Exit if not our level or type */
    if (level > applvl) {
        return;
    }
    if ((apptyp != TYPE_ALL) && (apptyp != type)) {
        return;
    }

    threadnr = (unsigned long)pthread_getspecific(tls_key_threadnr);

    snprintf(buf, sizeof(buf), "%s","");
    n = 0;
    errno_save = errno;
    mythreadname_get(threadname);

    if (log_mode == LOGMODE_FILE) {
        n = snprintf(buf, sizeof(buf), "%s [%s][%s][%02d:%s] "
            , str_time(), log_level_str[level], log_type_str[type]
            , threadnr, threadname );
        timelen = 16;
    } else {
        n = snprintf(buf, sizeof(buf), "[%s][%s][%02d:%s] "
            , log_level_str[level], log_type_str[type]
            , threadnr, threadname );
        timelen = 0;
    }
    prefixlen = n;

    /* Prepend the format specifier for the function name */
    if (fncname) {
        va_start(ap, fmt);
        prefixlen += snprintf(usrfmt, sizeof(usrfmt),"%s: ", va_arg(ap, char *));
        va_end(ap);
        snprintf(usrfmt, sizeof (usrfmt),"%s: %s", "%s", fmt);
    } else {
        snprintf(usrfmt, sizeof (usrfmt),"%s",fmt);
    }

    /* Next add the user's message. */
    va_start(ap, fmt);
    n += vsnprintf(buf + n, sizeof(buf) - n, usrfmt, ap);
    va_end(ap);
    buf[1023] = '\0';

    /* If errno_flag is set, add on the library error message. */
    if (errno_flag) {
      size_t buf_len = strlen(buf);

      // just knock off 10 characters if we're that close...
      if (buf_len + 10 > 1024) {
          buf[1024 - 10] = '\0';
          buf_len = 1024 - 10;
      }

      strncat(buf, ": ", 1024 - buf_len);
      n += 2;
        /*
         * This is bad - apparently gcc/libc wants to use the non-standard GNU
         * version of strerror_r, which doesn't actually put the message into
         * my buffer :-(.  I have put in a 'hack' to get around this.
         */
        #if defined(XSI_STRERROR_R)
            /* XSI-compliant strerror_r() */
            strerror_r(errno_save, buf + n, sizeof(buf) - n);    /* 2 for the ': ' */
            (void)msg_buf;
        #else
            /* GNU-specific strerror_r() */
            strncat(buf, strerror_r(errno_save, msg_buf, sizeof(msg_buf)), 1024 - strlen(buf));
        #endif
    }

    if ((mystreq(&buf[timelen], flood_msg)) && (flood_cnt <= 5000)) {
        flood_cnt++;
    } else {
        if (flood_cnt > 1) {
            snprintf(flood_repeats, 1024
                , "%s Above message repeats %d times"
                , prefix_msg, flood_cnt-1);
            switch (log_mode) {
            case LOGMODE_FILE:
                strncat(flood_repeats, "\n", 1024 - strlen(flood_repeats));
                fputs(flood_repeats, logfile);
                fflush(logfile);
                break;

            case LOGMODE_SYSLOG:
                /* The syslog level values are one less than the motion numeric values*/
                syslog(level-1, "%s", flood_repeats);
                strncat(flood_repeats, "\n", 1024 - strlen(flood_repeats));
                fputs(flood_repeats, stderr);
                fflush(stderr);
                break;
            }
        }
        flood_cnt = 1;
        snprintf(flood_msg, 1024, "%s", &buf[timelen]);
        snprintf(prefix_msg, prefixlen, "%s", buf);
        switch (log_mode) {
        case LOGMODE_FILE:
            strncat(buf, "\n", 1024 - strlen(buf));
            fputs(buf, logfile);
            fflush(logfile);
            break;

        case LOGMODE_SYSLOG:
            /* The syslog level values are one less than the motion numeric values*/
            syslog(level-1, "%s", buf);
            strncat(buf, "\n", 1024 - strlen(buf));
            fputs(buf, stderr);
            fflush(stderr);
            break;
        }
    }

}

void log_init(struct ctx_motapp *motapp)
{

    if ((motapp->log_level > ALL) ||
        (motapp->log_level == 0)) {
        motapp->log_level = LEVEL_DEFAULT;
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Using default log level (%s) (%d)")
            ,log_level_str[motapp->log_level]
            ,motapp->log_level);
    }


    if (motapp->log_file != "") {
        if (motapp->log_file != "syslog") {
            log_set_mode(LOGMODE_FILE);
            log_set_logfile(motapp->log_file.c_str());
            if (logfile) {
                log_set_mode(LOGMODE_SYSLOG);
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                    ,_("Logging to file (%s)"),motapp->log_file.c_str());
                log_set_mode(LOGMODE_FILE);
            } else {
                MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO
                    ,_("Exit motion, cannot create log file %s")
                    ,motapp->log_file.c_str());
                exit(0);
            }
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Logging to syslog"));
        }
    } else {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Logging to syslog"));
    }
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "MotionPlus %s started",VERSION);

    motapp->log_type = log_get_type(motapp->log_type_str.c_str());

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Using log type (%s) log level (%s)"),
               log_type_str[motapp->log_type], log_level_str[motapp->log_level]);

    log_set_level(motapp->log_level);
    log_type = motapp->log_type;

}

void log_deinit(struct ctx_motapp *motapp)
{

    if (logfile != NULL) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Closing logfile (%s)."),
                   motapp->log_file.c_str());
        myfclose(logfile);
        log_set_mode(LOGMODE_NONE);
        logfile = NULL;
    }

}

void log_set_motapp(struct ctx_motapp *motapp)
{
    /* Need better design to avoid the need to do this.  Extern motapp to whole app? */
    log_motapp = motapp;  /* Set our static pointer used for locking parms mutex*/

}