/*  vloopback_motion.h
 *
 *  Include file for video_loopback.c
 *      Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *      Copyright 2008 by Angel Carpintero (motiondevelop@gmail.com)
 *      This software is distributed under the GNU public license version 2
 *      See also the file 'COPYING'.
 *
 */
#ifndef _INCLUDE_VIDEO_LOOPBACK_H
#define _INCLUDE_VIDEO_LOOPBACK_H

int vlp_startpipe(const char *dev_name, int width, int height);
int vlp_putpipe(int dev, unsigned char *image, int imgsize);

#endif
