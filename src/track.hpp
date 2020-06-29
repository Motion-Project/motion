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
 *
*/

#ifndef _INCLUDE_TRACK_H
#define _INCLUDE_TRACK_H

struct ctx_coord;

struct ctx_track {
    int             dev;
    int     maxx;
    int     minx;
    int     maxy;
    int     miny;

    int    pan_angle; // degrees
    int    tilt_angle; // degrees
    int    posx;
    int    posy;
    int    minmaxfound;
};


enum track_action { TRACK_CENTER, TRACK_MOVE };

void track_init(struct ctx_cam *cam);
void track_deinit(struct ctx_cam *cam);

int track_center(struct ctx_cam *cam, int dev,
        int manual, int xoff, int yoff);

int track_move(struct ctx_cam *cam, int dev,struct ctx_coord *cent
        , struct ctx_images *imgs, int manual);

#endif /* _INCLUDE_TRACK_H */
