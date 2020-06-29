/*
 *    This file is part of Motionplus.
 *
 *    MotionPlus is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motionplus is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motionplus.  If not, see <https://www.gnu.org/licenses/>.
 *
 *    Copyright 2020 MotionMrDave@gmail.com
 */
#ifndef _INCLUDE_VIDEO_LOOPBACK_H
#define _INCLUDE_VIDEO_LOOPBACK_H

    int vlp_startpipe(const char *dev_name, int width, int height);
    int vlp_putpipe(int dev, unsigned char *image, int imgsize);
    void vlp_init(struct ctx_cam *cam);
#endif
