/*
 * jpegutils.h: Some Utility programs for dealing with
 *               JPEG encoded images
 *
 *  Copyright (C) 1999 Rainer Johanni <Rainer@Johanni.de>
 *  Copyright (C) 2001 pHilipp Zabel  <pzabel@gmx.de>
 *  Copyright (C) 2008 Angel Carpintero <motiondevelop@gmail.com>
 *
 */

#ifndef __JPEGUTILS_H__
#define __JPEGUTILS_H__

int jpgutl_decode_jpeg (unsigned char *jpeg_data_in, int jpeg_data_len,
                     unsigned int width, unsigned int height, unsigned char *volatile img_out);

unsigned prepare_exif(unsigned char **, const struct context *, const struct timeval *, const struct coord *);

int put_jpeg_yuv420p_memory(unsigned char *, int image, unsigned char *, int, int, int, struct context *, struct timeval *, struct coord *);
int    put_jpeg_grey_memory(unsigned char *, int image, unsigned char *, int, int, int, struct context *, struct timeval *, struct coord *);

#endif
