/*
 *      logger.cpp
 *
 *      Logger for motion
 *
 *      Copyright 2005, William M. Brack
 *      Copyright 2008 by Angel Carpintero  (motiondevelop@gmail.com)
 *      This software is distributed under the GNU Public License Version 2
 *      See also the file 'COPYING'.
 *
 */
#include "motion.hpp"
#include "conf.hpp"
#include "util.hpp"
#include "logger.hpp"
#include <stdarg.h>

static int log_mode = LOGMODE_SYSLOG;
static FILE *logfile  = NULL;
static unsigned int log_level = LEVEL_DEFAULT;
static unsigned int log_type = TYPE_DEFAULT;

static const char *log_type_str[] = {NULL, "COR", "STR", "ENC", "NET", "DBL", "EVT", "TRK", "VID", "ALL"};
static const char *log_level_str[] = {"EMG", "ALR", "CRT", "ERR", "WRN", "NTC", "INF", "DBG", "ALL", NULL};


/** Returns index of log type or 0 if not valid type. */
static int log_get_type(const char *type) {
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

/** Gets string value for type log level. */
static const char* log_get_type_str(unsigned int type) {
    return log_type_str[type];
}

/** Sets log type level. */
static void log_set_type(unsigned int type) {
    log_type = type;
}

/** Gets string value for log level. */
static const char* log_get_level_str(unsigned int level) {
    return log_level_str[level];
}

/** Sets log level. */
static void log_set_level(unsigned int level) {
    log_level = level;
}

/** Sets mode of logging, could be using syslog or files. */
static void log_set_mode(int mode) {
    int prev_mode = log_mode;

    log_mode = mode;

    if (mode == LOGMODE_SYSLOG && prev_mode != LOGMODE_SYSLOG){
        openlog("motion", LOG_PID, LOG_USER);
    }

    if (mode != LOGMODE_SYSLOG && prev_mode == LOGMODE_SYSLOG){
        closelog();
    }
}

/** Sets logfile to be used instead of syslog. */
static void log_set_logfile(const char *logfile_name) {
    /* Setup temporary to let log if myfopen fails */
    log_set_mode(LOGMODE_SYSLOG);

    logfile = myfopen(logfile_name, "a");

    /* If logfile was opened correctly */
    if (logfile){
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
void motion_log(int level, unsigned int type, int errno_flag,int fncname, const char *fmt, ...){
    int errno_save, n;
    char buf[1024]= {0};
    char usrfmt[1024]= {0};
    char msg_buf[100]= {0};

    va_list ap;
    int threadnr;

    static int flood_cnt = 0;
    static char flood_msg[1024];
    char flood_repeats[1024];
    char threadname[32];


    /* Exit if level is greater than log_level */
    if ((unsigned int)level > log_level){
        return;
    }

    /* Exit if type is not equal to log_type and not TYPE_ALL */
    if ((log_type != TYPE_ALL) && (type != log_type)){
        return;
    }

    threadnr = (unsigned long)pthread_getspecific(tls_key_threadnr);

    snprintf(buf, sizeof(buf), "%s","");
    n = 0;

    errno_save = errno;

    mythreadname_get(threadname);

    if (log_mode == LOGMODE_FILE) {
        n = snprintf(buf, sizeof(buf), "[%d:%s] [%s] [%s] [%s] ",
                     threadnr, threadname, log_get_level_str(level), log_get_type_str(type),
                     str_time());
    } else {
        n = snprintf(buf, sizeof(buf), "[%d:%s] [%s] [%s] ",
                     threadnr, threadname, log_get_level_str(level), log_get_type_str(type));
    }

    /* Prepend the format specifier for the function name */
    if (fncname){
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

    if ((mystreq(buf,flood_msg)) && (flood_cnt <= 5000)){
        flood_cnt++;
    } else {
        if (flood_cnt > 1){
            snprintf(flood_repeats,1024,"[%d:%s] [%s] [%s] Above message repeats %d times",
                     threadnr, threadname, log_get_level_str(level)
                     , log_get_type_str(type), flood_cnt-1);
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

void log_init(struct ctx_motapp *motapp){

    if ((motapp->log_level > ALL) ||
        (motapp->log_level == 0)) {
        motapp->log_level = LEVEL_DEFAULT;
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Using default log level (%s) (%d)")
            ,log_get_level_str(motapp->log_level)
            ,SHOW_LEVEL_VALUE(motapp->log_level));
    } else {
        motapp->log_level--; // Let's make syslog compatible
    }


    if (motapp->log_file != "") {
        if (motapp->log_file != "syslog") {
            log_set_mode(LOGMODE_FILE);
            log_set_logfile(motapp->log_file.c_str());
            if (logfile) {
                log_set_mode(LOGMODE_SYSLOG);
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                    ,_("Logging to file (%s)"),motapp->log_file);
                log_set_mode(LOGMODE_FILE);
            } else {
                MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO
                    ,_("Exit motion, cannot create log file %s")
                    ,motapp->log_file);
                exit(0);
            }
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Logging to syslog"));
        }
    } else {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Logging to syslog"));
    }
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "Motion %s Started",VERSION);

    motapp->log_type = log_get_type(motapp->log_type_str.c_str());

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Using log type (%s) log level (%s)"),
               log_get_type_str(motapp->log_type), log_get_level_str(motapp->log_level));

    log_set_level(motapp->log_level);
    log_set_type(motapp->log_type);

}

void log_deinit(struct ctx_motapp *motapp){

    if (logfile != NULL) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Closing logfile (%s)."),
                   motapp->log_file);
        myfclose(logfile);
        log_set_mode(LOGMODE_NONE);
        logfile = NULL;
    }

}