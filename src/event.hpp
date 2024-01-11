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
 *    Copyright 2020-2023 MotionMrDave@gmail.com
 */
#ifndef _INCLUDE_EVENT_HPP_
#define _INCLUDE_EVENT_HPP_

typedef enum {
    EVENT_MOTION = 1,
    EVENT_START,
    EVENT_END,
    EVENT_TLAPSE_START,
    EVENT_TLAPSE_END,
    EVENT_STREAM,
    EVENT_IMAGE_DETECTED,
    EVENT_IMAGEM_DETECTED,
    EVENT_IMAGE_SNAPSHOT,
    EVENT_IMAGE,
    EVENT_IMAGEM,
    EVENT_IMAGE_PREVIEW,
    EVENT_FILECLOSE,
    EVENT_AREA_DETECTED,
    EVENT_CAMERA_LOST,
    EVENT_CAMERA_FOUND,
    EVENT_MOVIE_PUT,
    EVENT_MOVIE_START,
    EVENT_MOVIE_END,
    EVENT_SECDETECT,
    EVENT_LAST,
} motion_event;

typedef void(* event_handler)(ctx_dev *cam, char *fname);

void event(ctx_dev *cam, motion_event evnt, char *fname);

const char * imageext(ctx_dev *cam);

#endif /* _INCLUDE_EVENT_HPP_ */
