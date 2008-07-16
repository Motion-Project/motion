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

#define EVENT_FILECREATE        1
#define EVENT_MOTION            2
#define EVENT_FIRSTMOTION       3
#define EVENT_ENDMOTION         4
#define EVENT_STOP              5
#define EVENT_TIMELAPSE         6
#define EVENT_TIMELAPSEEND      7
#define EVENT_STREAM            8
#define EVENT_IMAGE_DETECTED    9
#define EVENT_IMAGEM_DETECTED   10
#define EVENT_IMAGE_SNAPSHOT    11
#define EVENT_IMAGE             12
#define EVENT_IMAGEM            13
#define EVENT_FILECLOSE         14
#define EVENT_DEBUG             15
#define EVENT_CRITICAL          16
#define EVENT_AREA_DETECTED     17
#define EVENT_CAMERA_LOST       18
#define EVENT_FFMPEG_PUT        19


typedef void(* event_handler)(struct context *, int, unsigned char *, char *, void *, struct tm *);

void event(struct context *, int, unsigned char *, char *, void *, struct tm *);
const char * imageext(struct context *);

#endif /* _INCLUDE_EVENT_H_ */
