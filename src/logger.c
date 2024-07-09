/*   This file is part of Motion.
 *
 *   Motion is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   Motion is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 *      logger.c
 *
 *      Logger for motion
 *
 *      Copyright 2005, William M. Brack
 *      Copyright 2008 by Angel Carpintero  (motiondevelop@gmail.com)
 *
 */

#include "logger.h"   /* already includes motion.h */
#include "util.h"
#include <stdarg.h>

static int log_mode = LOGMODE_SYSLOG;
static FILE *logfile;
static unsigned int log_level = LEVEL_DEFAULT;
static unsigned int log_type = TYPE_DEFAULT;

static const char *log_type_str[] = {NULL, "COR", "STR", "ENC", "NET", "DBL", "EVT", "TRK", "VID", "ALL"};
static const char *log_level_str[] = {"EMG", "ALR", "CRT", "ERR", "WRN", "NTC", "INF", "DBG", "ALL", NULL};


/**
 * get_log_type
 *
 *
 * Returns: index of log type or 0 if not valid type.
 */
int get_log_type(const char *type)
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

/**
 * get_log_type_str
 *      Gets string value for type log level.
 *
 * Returns: name of type log level.
 */
const char* get_log_type_str(unsigned int type)
{
    return log_type_str[type];
}

/**
 * set_log_type
 *      Sets log type level.
 *
 * Returns: nothing.
 */
void set_log_type(unsigned int type)
{
    log_type = type;
    //printf("set log type %d\n", type);
}

/**
 * get_log_level_str
 *      Gets string value for log level.
 *
 * Returns: name of log level.
 */
const char* get_log_level_str(unsigned int level)
{
    return log_level_str[level];
}

/**
 * set_log_level
 *      Sets log level.
 *
 * Returns nothing.
 */
void set_log_level(unsigned int level)
{
    log_level = level;
    //printf("set log level %d\n", level);
}

/**
 * set_log_mode
 *      Sets mode of logging, could be using syslog or files.
 *
 * Returns: nothing.
 */
void set_log_mode(int mode)
{
    int prev_mode = log_mode;

    log_mode = mode;
    //printf("set log mode %d\n", mode);

    if (mode == LOGMODE_SYSLOG && prev_mode != LOGMODE_SYSLOG) {
        openlog("motion", LOG_PID, LOG_USER);
    }

    if (mode != LOGMODE_SYSLOG && prev_mode == LOGMODE_SYSLOG) {
        closelog();
    }
}

/**
 * set_logfile
 *      Sets logfile to be used instead of syslog.
 *
 * Returns: pointer to log file.
 */
FILE * set_logfile(const char *logfile_name)
{
    /* Setup temporary to let log if myfopen fails */
    set_log_mode(LOGMODE_SYSLOG);

    logfile = myfopen(logfile_name, "ae");

    /* If logfile was opened correctly */
    if (logfile) {
        set_log_mode(LOGMODE_FILE);
    }

    return logfile;
}

/**
 * str_time
 *
 * Return: string with human readable time
 */
static char *str_time(void)
{
    static char buffer[30]; /* Arbitrary length*/
    time_t now = 0;

    now = time(0);
    strftime(buffer, sizeof(buffer), "%b %d %H:%M:%S", localtime(&now));
    return buffer;
}

/**
 * MOTION_LOG
 *
 *    This routine is used for printing all informational, debug or error
 *    messages produced by any of the other motion functions.  It always
 *    produces a message of the form "[n] {message}", and (if the param
 *    'errno_flag' is set) follows the message with the associated error
 *    message from the library.
 *
 * Parameters:
 *
 *     level           logging level for the 'syslog' function
 *
 *     type            logging type.
 *
 *     errno_flag      if set, the log message should be followed by the
 *                     error message.
 *     fmt             the format string for producing the message
 *     ap              variable-length argument list
 *
 * Returns:
 *                     Nothing
 */
void motion_log(int level, unsigned int type, int errno_flag,int fncname, const char *fmt, ...)
{
    int errno_save, n;
    char buf[1024];
    char usrfmt[1024];

    /* GNU-specific strerror_r() */
    #if (!defined(XSI_STRERROR_R))
        char msg_buf[100];
    #endif
    va_list ap;
    int threadnr;

    static int flood_cnt = 0;
    static char flood_msg[1024];
    char flood_repeats[1024];


    /* Exit if level is greater than log_level */
    if ((unsigned int)level > log_level) {
        return;
    }

    /* Exit if type is not equal to log_type and not TYPE_ALL */
    if ((log_type != TYPE_ALL) && (type != log_type)) {
        return;
    }

    //printf("log_type %d, type %d level %d\n", log_type, type, level);

    threadnr = (unsigned long)pthread_getspecific(tls_key_threadnr);

    /*
     * First we save the current 'error' value.  This is required because
     * the subsequent calls to vsnprintf could conceivably change it!
     */
    errno_save = errno;

    char threadname[32];
    util_threadname_get(threadname);

    /*
     * Prefix the message with the thread number and name,
     * log level string, log type string, and time.
     * e.g. [1:enc] [ERR] [ALL] [Apr 03 00:08:44] blah
     */
    if (log_mode == LOGMODE_FILE) {
        n = snprintf(buf, sizeof(buf), "[%d:%s] [%s] [%s] [%s] ",
                     threadnr, threadname, get_log_level_str(level), get_log_type_str(type),
                     str_time());
    } else {
    /*
     * Prefix the message with the thread number and name,
     * log level string and log type string.
     * e.g. [1:trk] [DBG] [ALL] blah
     */
        n = snprintf(buf, sizeof(buf), "[%d:%s] [%s] [%s] ",
                     threadnr, threadname, get_log_level_str(level), get_log_type_str(type));
    }

    /* Prepend the format specifier for the function name */
    if (fncname) {
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
        #else
            /* GNU-specific strerror_r() */
            strncat(buf, strerror_r(errno_save, msg_buf, sizeof(msg_buf)), 1024 - strlen(buf));
        #endif
    }

    if ((mystreq(buf,flood_msg)) && (flood_cnt <= 5000)) {
        flood_cnt++;
    } else {
        if (flood_cnt > 1) {
            snprintf(flood_repeats,1024,"[%d:%s] [%s] [%s] Above message repeats %d times",
                     threadnr, threadname, get_log_level_str(level)
                     , get_log_type_str(type), flood_cnt-1);
            switch (log_mode) {
            case LOGMODE_FILE:
                strncat(flood_repeats, "\n", 1024 - strlen(flood_repeats));
                fputs(flood_repeats, logfile);
                fflush(logfile);
                break;

            case LOGMODE_SYSLOG:
                syslog(level, "%s", flood_repeats);
                strncat(flood_repeats, "\n", 1024 - strlen(flood_repeats));
                fputs(flood_repeats, stderr);
                fflush(stderr);
                break;
            }
        }
        flood_cnt = 1;
        snprintf(flood_msg,1024,"%s",buf);
        switch (log_mode) {
        case LOGMODE_FILE:
            strncat(buf, "\n", 1024 - strlen(buf));
            fputs(buf, logfile);
            fflush(logfile);
            break;

        case LOGMODE_SYSLOG:
            syslog(level, "%s", buf);
            strncat(buf, "\n", 1024 - strlen(buf));
            fputs(buf, stderr);
            fflush(stderr);
            break;
        }
    }

}

