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
 *  video_loopback.h
 *    Headers associated with functions in the video_loopback.c module.
 *    Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *    Copyright 2008 by Angel Carpintero (motiondevelop@gmail.com)
 */

#ifndef _INCLUDE_VIDEO_LOOPBACK_H
#define _INCLUDE_VIDEO_LOOPBACK_H

int vlp_startpipe(const char *dev_name, int width, int height);
int vlp_putpipe(int dev, unsigned char *image, int imgsize);

#endif
