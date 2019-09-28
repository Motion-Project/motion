/*
 *    event.h
 *
 *    Include file for event.c
 *
 *    Copyright Jeroen Vreeken, 2002
 *    This software is distributed under the GNU Public License Version 2
 *    see also the file 'COPYING'.
 *
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
    EVENT_LAST,
} motion_event;

typedef void(* event_handler)(struct ctx_cam *cam, motion_event, struct image_data *,
             char *, void *, struct timeval *);

void event(struct ctx_cam *cam, motion_event, struct image_data *img_data, char *, void *, struct timeval *);
const char * imageext(struct ctx_cam *cam);

#endif /* _INCLUDE_EVENT_H_ */
