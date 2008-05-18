/*
 *      logger.h
 *
 *      Include file for logger.h 
 *
 *      Copyright 2008 by Angel Carpintero  (ack@telefonica.net)
 *      This software is distributed under the GNU Public License Version 2
 *      See also the file 'COPYING'.
 *
 */
#ifndef _INCLUDE_LOGGER_H_
#define _INCLUDE_LOGGER_H_

#include "motion.h"
#include <syslog.h>

void motion_log(int, int, const char *, ...);

#endif 
