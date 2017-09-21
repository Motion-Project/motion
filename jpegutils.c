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

/*  These huffman tables are required by the old jpeg libs included with 14.04 */
static void add_huff_table(j_decompress_ptr dinfo, JHUFF_TBL **htblptr, const UINT8 *bits, const UINT8 *val){
/* Define a Huffman table */
    int nsymbols, len;

    if (*htblptr == NULL)
        *htblptr = jpeg_alloc_huff_table((j_common_ptr) dinfo);

    /* Copy the number-of-symbols-of-each-code-length counts. */
    memcpy((*htblptr)->bits, bits, sizeof((*htblptr)->bits));

    /*
     * Validate the counts.  We do this here mainly so we can copy the right
     * number of symbols from the val[] array, without risking marching off
     * the end of memory.  jchuff.c will do a more thorough test later.
     */
    nsymbols = 0;

    for (len = 1; len <= 16; len++)
        nsymbols += bits[len];

    if (nsymbols < 1 || nsymbols > 256)
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, "%s: Given jpeg buffer was too small");

    memcpy((*htblptr)->huffval, val, nsymbols * sizeof(UINT8));
}

static void std_huff_tables (j_decompress_ptr dinfo){
/* Set up the standard Huffman tables (cf. JPEG standard section K.3) */
/* IMPORTANT: these are only valid for 8-bit data precision! */

    static const UINT8 bits_dc_luminance[17] =
    { /* 0-base */ 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
    static const UINT8 val_dc_luminance[] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

    static const UINT8 bits_dc_chrominance[17] =
    { /* 0-base */ 0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
    static const UINT8 val_dc_chrominance[] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

    static const UINT8 bits_ac_luminance[17] =
    { /* 0-base */ 0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
    static const UINT8 val_ac_luminance[] =
    { 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
      0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
      0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
      0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
      0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
      0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
      0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
      0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
      0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
      0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
      0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
      0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
      0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
      0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
      0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
      0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
      0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
      0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
      0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
      0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
      0xf9, 0xfa };

    static const UINT8 bits_ac_chrominance[17] =
    { /* 0-base */ 0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
    static const UINT8 val_ac_chrominance[] =
    { 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
      0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
      0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
      0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
      0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
      0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
      0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
      0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
      0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
      0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
      0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
      0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
      0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
      0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
      0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
      0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
      0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
      0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
      0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
      0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
      0xf9, 0xfa };

    add_huff_table(dinfo, &dinfo->dc_huff_tbl_ptrs[0],
                   bits_dc_luminance, val_dc_luminance);
    add_huff_table(dinfo, &dinfo->ac_huff_tbl_ptrs[0],
                   bits_ac_luminance, val_ac_luminance);
    add_huff_table(dinfo, &dinfo->dc_huff_tbl_ptrs[1],
                   bits_dc_chrominance, val_dc_chrominance);
    add_huff_table(dinfo, &dinfo->ac_huff_tbl_ptrs[1],
                   bits_ac_chrominance, val_ac_chrominance);
}

static void guarantee_huff_tables(j_decompress_ptr dinfo)
{
    if ((dinfo->dc_huff_tbl_ptrs[0] == NULL) &&
        (dinfo->dc_huff_tbl_ptrs[1] == NULL) &&
        (dinfo->ac_huff_tbl_ptrs[0] == NULL) &&
        (dinfo->ac_huff_tbl_ptrs[1] == NULL)) {
        std_huff_tables(dinfo);
    }
}

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
            MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "msg_level: %d, %s", msg_level, buffer);
    }

}

static int jpgutl_setjmp_error(struct jpgutl_error_mgr *jerr){
    /* This is a separate function to isolate the jump */
    return setjmp (jerr->setjmp_buffer);
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
    unsigned int    i;
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
    if (jpgutl_setjmp_error(&jerr)) {
        jpeg_destroy_decompress (&dinfo);
        return -1;
    }

    jpgutl_buffer_src (&dinfo, jpeg_data_in, jpeg_data_len);

    jpeg_read_header (&dinfo, TRUE);

    //420 sampling is the default for YCbCr so no need to override.
    dinfo.out_color_space = JCS_YCbCr;
    dinfo.dct_method = JDCT_DEFAULT;
    guarantee_huff_tables(&dinfo);  /* Required by older versions of the jpeg libs */
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

    /*
     * If there are too many warnings, this means that
     * only a partial image could be returned which would
     * trigger many false positive motion detections
    */
    if (jerr.warning_seen > 2) return -1;

    return 0;

}
