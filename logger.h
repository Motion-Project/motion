/*
 *      logger.h
 *
 *      Include file for logger.c
 *
 *      Copyright 2005, William M. Brack
 *      Copyright 2008 by Angel Carpintero  (motiondevelop@gmail.com)
 *      This software is distributed under the GNU Public License Version 2
 *      See also the file 'COPYING'.
 *
 */
#ifndef _INCLUDE_LOGGER_H_
#define _INCLUDE_LOGGER_H_

#include "motion.h"
#include <syslog.h>

/* Logging mode */
#define LOGMODE_NONE            0   /* No logging             */
#define LOGMODE_FILE            1   /* Log messages to file   */
#define LOGMODE_SYSLOG          2   /* Log messages to syslog */

#define NO_ERRNO                0   /* Flag to avoid how message associated to errno */
#define SHOW_ERRNO              1   /* Flag to show message associated to errno */

/* Log levels */
#define LOG_ALL                 9
#define EMG                     LOG_EMERG     /* syslog 0 motion 1 */
#define ALR                     LOG_ALERT     /* syslog 1 motion 2 */
#define CRT                     LOG_CRIT      /* syslog 2 motion 3 */
#define ERR                     LOG_ERR       /* syslog 3 motion 4 */
#define WRN                     LOG_WARNING   /* syslog 4 motion 5 */
#define NTC                     LOG_NOTICE    /* syslog 5 motion 6 */
#define INF                     LOG_INFO      /* syslog 6 motion 7 */
#define DBG                     LOG_DEBUG     /* syslog 7 motion 8 */
#define ALL                     LOG_ALL       /* syslog 8 motion 9 */
#define LEVEL_DEFAULT           NTC           /* syslog 5 motion 6 default */
#define SHOW_LEVEL_VALUE(x)     (x+1)

/* Log types */
#define TYPE_CORE               1             /* Core logs         */
#define TYPE_STREAM             2             /* Stream logs       */
#define TYPE_ENCODER            3             /* Encoder logs      */
#define TYPE_NETCAM             4             /* Netcam logs       */
#define TYPE_DB                 5             /* Database logs     */
#define TYPE_EVENTS             6             /* Events logs       */
#define TYPE_TRACK              7             /* Track logs        */
#define TYPE_VIDEO              8             /* V4L1/2 Bktr logs  */
#define TYPE_ALL                9             /* All type logs     */
#define TYPE_DEFAULT            TYPE_ALL      /* Default type      */
#define TYPE_DEFAULT_STR        "ALL"         /* Default name logs */

#define MOTION_LOG(x, y, z, format, args...)  motion_log(x, y, z, "%s: " format, __FUNCTION__, ##args)

int get_log_type(const char* type);
const char* get_log_type_str(unsigned int type);
void set_log_type(unsigned int type);
const char* get_log_level_str(unsigned int level);
void set_log_level(unsigned int level);
void set_log_mode(int mode);
FILE * set_logfile(const char *logfile_name);
void motion_log(int, unsigned int, int, const char *, ...);

#endif
