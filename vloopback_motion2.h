/*  vloopback_motion.h
 *
 *  Include file for vloopback_motion.c
 *      Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *      Copyright 2008 by Angel Carpintero (motiondevelop@gmail.com)
 *      This software is distributed under the GNU public license version 2
 *      See also the file 'COPYING'.
 *
 */
#ifndef _INCLUDE_VLOOPBACK_MOTION_H
#define _INCLUDE_VLOOPBACK_MOTION_H

#include "motion.h"

int vid_startpipe(const char *dev_name, int width, int height, int);
int vid_putpipe(int dev, unsigned char *image, int);
#endif
