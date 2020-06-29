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
#ifndef _INCLUDE_EVENT_H_
#define _INCLUDE_EVENT_H_

typedef enum {
    EVENT_FILECREATE = 1,
    EVENT_MOTION,
    EVENT_FIRSTMOTION,
    EVENT_ENDMOTION,
    EVENT_TIMELAPSE,
    EVENT_TIMELAPSEEND,
    EVENT_STREAM,
    EVENT_IMAGE_DETECTED,
    EVENT_IMAGEM_DETECTED,
    EVENT_IMAGE_SNAPSHOT,
    EVENT_IMAGE,
    EVENT_IMAGEM,
    EVENT_IMAGE_PREVIEW,
    EVENT_FILECLOSE,
    EVENT_DEBUG,
    EVENT_CRITICAL,
    EVENT_AREA_DETECTED,
    EVENT_CAMERA_LOST,
    EVENT_CAMERA_FOUND,
    EVENT_MOVIE_PUT,
    EVENT_SECDETECT,
    EVENT_LAST,
} motion_event;

typedef void(* event_handler)(struct ctx_cam *cam, motion_event, struct ctx_image_data *,
             char *, void *, struct timespec *);

void event(struct ctx_cam *cam, motion_event, struct ctx_image_data *img_data, char *, void *, struct timespec *);
const char * imageext(struct ctx_cam *cam);

#endif /* _INCLUDE_EVENT_H_ */
