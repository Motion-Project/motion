/*
 *      logger.hpp
 *
 *      Include file for logger.cpp
 *
 *      Copyright 2005, William M. Brack
 *      Copyright 2008 by Angel Carpintero  (motiondevelop@gmail.com)
 *      This software is distributed under the GNU Public License Version 2
 *      See also the file 'COPYING'.
 *
 */
#ifndef _INCLUDE_LOGGER_H_
#define _INCLUDE_LOGGER_H_

    struct ctx_cam;

    /* Logging mode */
    #define LOGMODE_NONE            0   /* No logging             */
    #define LOGMODE_FILE            1   /* Log messages to file   */
    #define LOGMODE_SYSLOG          2   /* Log messages to syslog */

    #define NO_ERRNO                0   /* Flag to avoid how message associated to errno */
    #define SHOW_ERRNO              1   /* Flag to show message associated to errno */

    /* Log levels */
    /* It would be HUGELY helpful to have the motion log values start at zero instead of
       1 but that would be a headache for historical compatability...maybe think about this
       as a break later on.
    */
    #define LOG_ALL                 9
    #define EMG                     1       /* syslog 0 motion 1 */
    #define ALR                     2       /* syslog 1 motion 2 */
    #define CRT                     3       /* syslog 2 motion 3 */
    #define ERR                     4       /* syslog 3 motion 4 */
    #define WRN                     5       /* syslog 4 motion 5 */
    #define NTC                     6       /* syslog 5 motion 6 */
    #define INF                     7       /* syslog 6 motion 7 */
    #define DBG                     8       /* syslog 7 motion 8 */
    #define ALL                     9       /* syslog 8 motion 9 */
    #define LEVEL_DEFAULT           NTC     /* syslog 5 motion 6 default */

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

    #define MOTION_LOG(x, y, z, format, args...)  motion_log(x, y, z, 1, format, __FUNCTION__, ##args)

    void motion_log(int level, unsigned int type, int errno_flag,int fncname, const char *fmt, ...);
    void log_init(struct ctx_motapp *motapp);
    void log_deinit(struct ctx_motapp *motapp);

#endif
