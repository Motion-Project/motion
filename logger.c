/*
 *      logger.c
 *
 *      Logger for motion
 *
 *      Copyright 2005, William M. Brack
 *      Copyright 2008 by Angel Carpintero  (motiondevelop@gmail.com)
 *      This software is distributed under the GNU Public License Version 2
 *      See also the file 'COPYING'.
 *
 */

#include "logger.h"   /* already includes motion.h */
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
 *      Sets mode of logging , could be using syslog or files.
 *
 * Returns: nothing.
 */
void set_log_mode(int mode)
{
    log_mode = mode;
    //printf("set log mode %d\n", mode);
}

/**
 * set_logfile
 *      Sets logfile to be used instead of syslog.
 *
 * Returns: pointer to log file.
 */
FILE * set_logfile(const char *logfile_name)
{
    log_mode = LOGMODE_SYSLOG;  /* Setup temporary to let log if myfopen fails */
    logfile = myfopen(logfile_name, "a", 0);

    /* If logfile was opened correctly */
    if (logfile)
        log_mode = LOGMODE_FILE;

    return logfile;
}

/**
 * str_time
 *
 * Return: string with human readable time
 */
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
void motion_log(int level, unsigned int type, int errno_flag, const char *fmt, ...)
{
    int errno_save, n;
    char buf[1024];
/* GNU-specific strerror_r() */
#if (!defined(XSI_STRERROR_R))
    char msg_buf[100];
#endif
    va_list ap;
    int threadnr;

    /* Exit if level is greater than log_level */
    if ((unsigned int)level > log_level)
        return;

    /* Exit if type is not equal to log_type and not TYPE_ALL */
    if ((log_type != TYPE_ALL) && (type != log_type))
        return;

    //printf("log_type %d, type %d level %d\n", log_type, type, level);

    /*
     * If pthread_getspecific fails (e.g., because the thread's TLS doesn't
     * contain anything for thread number, it returns NULL which casts to zero,
     * which is nice because that's what we want in that case.
     */
    threadnr = (unsigned long)pthread_getspecific(tls_key_threadnr);

    /*
     * First we save the current 'error' value.  This is required because
     * the subsequent calls to vsnprintf could conceivably change it!
     */
    errno_save = errno;

    /*
     * Prefix the message with the log level string, log type string,
     * time and thread number. e.g. [1] [ERR] [ENC] [Apr 03 00:08:44] blah
     *
     */
    if (!log_mode) {
        n = snprintf(buf, sizeof(buf), "[%d] [%s] [%s] [%s] ",
                     threadnr, get_log_level_str(level), get_log_type_str(type),
                     str_time());
    } else {
    /*
     * Prefix the message with the log level string, log type string
     * and thread number. e.g. [1] [DBG] [TRK] blah
     */
        n = snprintf(buf, sizeof(buf), "[%d] [%s] [%s] ",
                     threadnr, get_log_level_str(level), get_log_type_str(type));
    }

    /* Next add the user's message. */
    va_start(ap, fmt);
    n += vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
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

    if (!log_mode) {
        strncat(buf, "\n", 1024 - strlen(buf));
        fputs(buf, logfile);
        fflush(logfile);

    /* If log_mode, send the message to the syslog. */
    } else {
        syslog(level, "%s", buf);
        strncat(buf, "\n", 1024 - strlen(buf));
        fputs(buf, stderr);
        fflush(stderr);
    }

    /* Clean up the argument list routine. */
    va_end(ap);
}

