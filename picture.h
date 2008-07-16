/*
 *    picture.h
 *
 *      Copyright 2002 by Jeroen Vreeken (pe1rxq@amsat.org)
 *      Portions of this file are Copyright by Lionnel Maugis
 *      This software is distributed under the GNU public license version 2
 *      See also the file 'COPYING'.
 *      
 */
#ifndef _INCLUDE_PICTURE_H_
#define _INCLUDE_PICTURE_H_

#include "motion.h"

void overlay_smartmask(struct context *, unsigned char *);
void overlay_fixed_mask(struct context *, unsigned char *);
void put_fixed_mask(struct context *, const char *);
void overlay_largest_label(struct context *, unsigned char *);
void put_picture_fd(struct context *, FILE *, unsigned char *, int);
int put_picture_memory(struct context *, unsigned char*, int, unsigned char *, int);
void put_picture(struct context *, char *, unsigned char *, int);
unsigned char *get_pgm(FILE *, int, int);
void preview_save(struct context *);

#endif /* _INCLUDE_PICTURE_H_ */
