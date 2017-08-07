/*
 *  jpegutils.c: Some Utility programs for dealing with JPEG encoded images
 *
 *  Copyright (C) 1999 Rainer Johanni <Rainer@Johanni.de>
 *  Copyright (C) 2001 pHilipp Zabel  <pzabel@gmx.de>
 *  Copyright (C) 2008 Angel Carpintero <motiondevelop@gmail.com>
 *
 *  based on jdatasrc.c and jdatadst.c from the Independent
 *  JPEG Group's software by Thomas G. Lane
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

 /*
 * jpegutils.c
 *  Purpose:
 *    Decompress jpeg data into images for use in other parts of program.
 *    Currently this module only decompresses and it is only called from
 *    the vid_mjpegtoyuv420p function
 *  Functional Prefixes
 *    All functions within the module will use the prefix "jpgutl" for identification
 *  Module Level Variables:
 *    EOI_data    Constant value to indicate the end of an image.
 *  Module Level Structures:
 *    jpgutl_error_mgr  Used by the JPEG libraries as the error manager to catch/trap messages from library.
 *  Static Functions:
 *    The following functions are required by the JPEG library to decompress images.
 *      jpgutl_init_source
 *      jpgutl_fill_input_buffer
 *      jpgutl_skip_data
 *      jpgutl_term_source
 *      jpgutl_buffer_src
 *      jpgutl_error_exit
 *      jpgutl_emit_message
 *  Exposed Functions
 *    jpgutl_decode_jpeg
 */

#include "config.h"
#include "motion.h"
#include "jpegutils.h"
#include <setjmp.h>
#include <jpeglib.h>
#include <jerror.h>
#include <assert.h>

static const uint8_t EOI_data[2] = { 0xFF, 0xD9 };

struct jpgutl_error_mgr {
    struct jpeg_error_mgr pub;   /* "public" fields */
    jmp_buf setjmp_buffer;       /* For return to caller */

    /* Original emit_message method. */
    JMETHOD(void, original_emit_message, (j_common_ptr cinfo, int msg_level));
    /* Was a corrupt-data warning seen. */
    int warning_seen;
};

/*
 * Initialize source --- called by jpeg_read_header
 * before any data is actually read.
 */
static void jpgutl_init_source(j_decompress_ptr cinfo ATTRIBUTE_UNUSED)
{
    /* No work necessary here */
}

/*
 * Fill the input buffer --- called whenever buffer is emptied.
 *
 * Should never be called since all data should be already provided.
 * Is nevertheless sometimes called - sets the input buffer to data
 * which is the JPEG EOI marker;
 *
 */
static boolean jpgutl_fill_input_buffer(j_decompress_ptr cinfo)
{
    cinfo->src->next_input_byte = EOI_data;
    cinfo->src->bytes_in_buffer = 2;
    return TRUE;
}

/*
 * Skip data --- used to skip over a potentially large amount of
 * uninteresting data (such as an APPn marker).
 *
 */
static void jpgutl_skip_data(j_decompress_ptr cinfo, long num_bytes)
{
    if (num_bytes > 0) {
        if (num_bytes > (long) cinfo->src->bytes_in_buffer)
            num_bytes = (long) cinfo->src->bytes_in_buffer;
        cinfo->src->next_input_byte += (size_t) num_bytes;
        cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
    }
}

/*
 * Terminate source --- called by jpeg_finish_decompress
 * after all data has been read.  Often a no-op.
 */
static void jpgutl_term_source(j_decompress_ptr cinfo ATTRIBUTE_UNUSED)
{
    /* No work necessary here */
}

/*
 * The source object and input buffer are made permanent so that a series
 * of JPEG images can be read from the same buffer by calling jpgutl_buffer_src
 * only before the first one.  (If we discarded the buffer at the end of
 * one image, we'd likely lose the start of the next one.)
 * This makes it unsafe to use this manager and a different source
 * manager serially with the same JPEG object.  Caveat programmer.
 */
/**
 * jpgutl_buffer_src
 *  Purpose:
 *    Establish the input buffer source for the JPEG libary and associated helper functions.
 *  Parameters:
 *    cinfo      The jpeg library compression/decompression information
 *    buffer     The buffer of JPEG data to decompress.
 *    buffer_len The length of the buffer.
 *  Return values:
 *    None
 */
static void jpgutl_buffer_src(j_decompress_ptr cinfo, unsigned char *buffer, long buffer_len)
{

    if (cinfo->src == NULL) {    /* First time for this JPEG object? */
        cinfo->src = (struct jpeg_source_mgr *)
                     (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                     sizeof (struct jpeg_source_mgr));
    }

    cinfo->src->init_source = jpgutl_init_source;
    cinfo->src->fill_input_buffer = jpgutl_fill_input_buffer;
    cinfo->src->skip_input_data = jpgutl_skip_data;
    cinfo->src->resync_to_restart = jpeg_resync_to_restart;    /* Use default method */
    cinfo->src->term_source = jpgutl_term_source;
    cinfo->src->bytes_in_buffer = buffer_len;
    cinfo->src->next_input_byte = (JOCTET *) buffer;


}

/**
 * jpgutl_error_exit
 *  Purpose:
 *    Exit routine for errors thrown by JPEG library.
 *  Parameters:
 *    cinfo      The jpeg library compression/decompression information
 *  Return values:
 *    None
 */
static void jpgutl_error_exit(j_common_ptr cinfo)
{
    char buffer[JMSG_LENGTH_MAX];

    /* cinfo->err really points to a jpgutl_error_mgr struct, so coerce pointer. */
    struct jpgutl_error_mgr *myerr = (struct jpgutl_error_mgr *) cinfo->err;

    /*
     * Always display the message.
     * We could postpone this until after returning, if we chose.
     */
    (*cinfo->err->format_message) (cinfo, buffer);

    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, "%s", buffer);

    /* Return control to the setjmp point. */
    longjmp (myerr->setjmp_buffer, 1);
}

/**
 * jpgutl_emit_message
 *  Purpose:
 *    Process the messages thrown by the JPEG library
 *  Parameters:
 *    cinfo      The jpeg library compression/decompression information
 *    msg_level  Integer indicating the severity of the message.
 *  Return values:
 *    None
 */
static void jpgutl_emit_message(j_common_ptr cinfo, int msg_level)
{
    char buffer[JMSG_LENGTH_MAX];
    /* cinfo->err really points to a jpgutl_error_mgr struct, so coerce pointer. */
    struct jpgutl_error_mgr *myerr = (struct jpgutl_error_mgr *) cinfo->err;
    /*
     *  The JWRN_EXTRANEOUS_DATA is sent a lot without any particular negative effect.
     *  There are some messages above zero but they are just informational and not something
     *  that we are interested in.
    */
    if ((cinfo->err->msg_code != JWRN_EXTRANEOUS_DATA) && (msg_level < 0) ) {
        myerr->warning_seen++ ;
        (*cinfo->err->format_message) (cinfo, buffer);
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "msg_level: %d, %s", msg_level, buffer);
    }

}

/**
 * jpgutl_decode_jpeg
 *  Purpose:  Decompress the jpeg data_in into the img_out buffer.
 *
 *  Parameters:
 *  jpeg_data_in     The jpeg data sent in
 *  jpeg_data_len    The length of the jpeg data
 *  width            The width of the image
 *  height           The height of the image
 *  img_out          Pointer to the image output
 *
 *  Return Values
 *    Success 0, Failure -1
 */
int jpgutl_decode_jpeg (unsigned char *jpeg_data_in, int jpeg_data_len,
                     unsigned int width, unsigned int height, unsigned char *img_out)
{
    JSAMPARRAY      line;           /* Array of decomp data lines */
    unsigned char  *wline;          /* Will point to line[0] */
    int             i;
    unsigned char  *img_y, *img_cb, *img_cr;
    unsigned char   offset_y;

    struct jpeg_decompress_struct dinfo;
    struct jpgutl_error_mgr jerr;

    /* We set up the normal JPEG error routines, then override error_exit. */
    dinfo.err = jpeg_std_error (&jerr.pub);
    jerr.pub.error_exit = jpgutl_error_exit;
    /* Also hook the emit_message routine to note corrupt-data warnings. */
    jerr.original_emit_message = jerr.pub.emit_message;
    jerr.pub.emit_message = jpgutl_emit_message;
    jerr.warning_seen = 0;

    jpeg_create_decompress (&dinfo);

    /* Establish the setjmp return context for jpgutl_error_exit to use. */
    if (setjmp (jerr.setjmp_buffer)) {
        /* If we get here, the JPEG code has signaled an error. */
        jpeg_destroy_decompress (&dinfo);
        return -1;
    }

    jpgutl_buffer_src (&dinfo, jpeg_data_in, jpeg_data_len);

    jpeg_read_header (&dinfo, TRUE);

    //420 sampling is the default for YCbCr so no need to override.
    dinfo.out_color_space = JCS_YCbCr;
    dinfo.dct_method = JDCT_DEFAULT;

    jpeg_start_decompress (&dinfo);

    if ((dinfo.output_width == 0) || (dinfo.output_height == 0)) {
        MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO,"Invalid JPEG image dimensions");
        jpeg_destroy_decompress(&dinfo);
        return -1;
    }

    if ((dinfo.output_width != width) || (dinfo.output_height != height)) {
        MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO,
                   "JPEG image size %dx%d, JPEG was %dx%d",
                    width, height, dinfo.output_width, dinfo.output_height);
        jpeg_destroy_decompress(&dinfo);
        return -1;
    }

    img_y  = img_out;
    img_cb = img_y + dinfo.output_width * dinfo.output_height;
    img_cr = img_cb + (dinfo.output_width * dinfo.output_height) / 4;

    /* Allocate space for one line. */
    line = (*dinfo.mem->alloc_sarray)((j_common_ptr) &dinfo, JPOOL_IMAGE,
                                       dinfo.output_width * dinfo.output_components, 1);

    wline = line[0];
    offset_y = 0;

    while (dinfo.output_scanline < dinfo.output_height) {
        jpeg_read_scanlines(&dinfo, line, 1);

        for (i = 0; i < (dinfo.output_width * 3); i += 3) {
            img_y[i / 3] = wline[i];
            if (i & 1) {
                img_cb[(i / 3) / 2] = wline[i + 1];
                img_cr[(i / 3) / 2] = wline[i + 2];
            }
        }

        img_y += dinfo.output_width;

        if (offset_y++ & 1) {
            img_cb += dinfo.output_width / 2;
            img_cr += dinfo.output_width / 2;
        }
    }

    jpeg_finish_decompress(&dinfo);
    jpeg_destroy_decompress(&dinfo);

    /* The 10% was determined by trial. */
    if ((jerr.warning_seen / dinfo.output_height)  > 0.10) return -1;

    return 0;

}
