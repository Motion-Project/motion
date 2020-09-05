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
 *    Copyright 2020 MotionMrDave@gmail.com
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
    #define LOG_ALL                 9
    #define EMG                     1
    #define ALR                     2
    #define CRT                     3
    #define ERR                     4
    #define WRN                     5
    #define NTC                     6
    #define INF                     7
    #define DBG                     8
    #define ALL                     9
    #define LEVEL_DEFAULT           NTC

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

    void motion_log(int loglevel, int logtype, int errno_flag,int fncname, const char *fmt, ...);
    void log_init(struct ctx_motapp *motapp);
    void log_deinit(struct ctx_motapp *motapp);
    void log_set_level(int new_level);
    void log_set_type(const char *new_logtype);
    void log_set_motapp(struct ctx_motapp *motapp);

#endif
