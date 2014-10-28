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
    EVENT_STOP,
    EVENT_TIMELAPSE,
    EVENT_TIMELAPSEEND,
    EVENT_STREAM,
    EVENT_IMAGE_DETECTED,
    EVENT_IMAGEM_DETECTED,
    EVENT_IMAGE_SNAPSHOT,
    EVENT_IMAGE,
    EVENT_IMAGEM,
    EVENT_FILECLOSE,
    EVENT_DEBUG,
    EVENT_CRITICAL,
    EVENT_AREA_DETECTED,
    EVENT_CAMERA_LOST,
    EVENT_FFMPEG_PUT,
    EVENT_SDL_PUT,
    EVENT_LAST,
} motion_event;


typedef void(* event_handler)(struct context *, motion_event, unsigned char *,
             char *, void *, struct tm *);

void event(struct context *, motion_event, unsigned char *, char *, void *,
           struct tm *);
const char * imageext(struct context *);

#endif /* _INCLUDE_EVENT_H_ */
