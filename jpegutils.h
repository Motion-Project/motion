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

int jpgutl_decode_jpeg (unsigned char *jpeg_data, int len,
                     unsigned int width_in, unsigned int height_in, unsigned char *img);


#endif
