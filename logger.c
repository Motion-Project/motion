/*
 *      logger.c
 *
 *      Logger for motion 
 *
 *      Copyright 2005, William M. Brack 
 *      Copyright 2008 by Angel Carpintero  (ack@telefonica.net)
 *      This software is distributed under the GNU Public License Version 2
 *      See also the file 'COPYING'.
 *
 */

#include "logger.h"   /* already includes motion.h */
#include <stdarg.h>

/**
 * motion_log
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
 *                     (-1 implies no syslog message should be produced)
 *     errno_flag      if set, the log message should be followed by the
 *                     error message.
 *     fmt             the format string for producing the message
 *     ap              variable-length argument list
 *
 * Returns:
 *                     Nothing
 */
void motion_log(int level, int errno_flag, const char *fmt, ...)
{
    int errno_save, n;
    char buf[1024];
#if (!defined(BSD))
    char msg_buf[100];
#endif
    va_list ap;
    int threadnr;

    /* If pthread_getspecific fails (e.g., because the thread's TLS doesn't
     * contain anything for thread number, it returns NULL which casts to zero,
     * which is nice because that's what we want in that case.
     */
    threadnr = (unsigned long)pthread_getspecific(tls_key_threadnr);

    /*
     * First we save the current 'error' value.  This is required because
     * the subsequent calls to vsnprintf could conceivably change it!
     */
    errno_save = errno;

    /* Prefix the message with the thread number */
    n = snprintf(buf, sizeof(buf), "[%d] ", threadnr);

    /* Next add the user's message */
    va_start(ap, fmt);
    n += vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);

    /* If errno_flag is set, add on the library error message */
    if (errno_flag) {
        strncat(buf, ": ", 1024 - strlen(buf));
        n += 2;
        /*
         * this is bad - apparently gcc/libc wants to use the non-standard GNU
         * version of strerror_r, which doesn't actually put the message into
         * my buffer :-(.  I have put in a 'hack' to get around this.
         */
#if (defined(BSD))
        strerror_r(errno_save, buf + n, sizeof(buf) - n);    /* 2 for the ': ' */
#else
        strncat(buf, strerror_r(errno_save, msg_buf, sizeof(msg_buf)), 1024 - strlen(buf));
#endif
    }
    /* If 'level' is not negative, send the message to the syslog */
    if (level >= 0)
        syslog(level, "%s", buf);

    /* For printing to stderr we need to add a newline */
    strncat(buf, "\n", 1024 - strlen(buf));
    fputs(buf, stderr);
    fflush(stderr);

    /* Clean up the argument list routine */
    va_end(ap);
}


