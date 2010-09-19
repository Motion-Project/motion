/*  sdl.h
 *
 *  Include file for sdl.c
 *      Copyright 2009 by Peter Holik (peter@holik.at)
 *      This software is distributed under the GNU public license version 2
 *      See also the file 'COPYING'.
 */
#ifndef _INCLUDE_SDL_H
#define _INCLUDE_SDL_H

#include "motion.h"

int sdl_start(int width, int height);
void sdl_put(unsigned char *image, int width, int height);
void sdl_stop(void);

#endif
