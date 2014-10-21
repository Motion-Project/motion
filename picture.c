/*    picture.c
 *
 *    Various funtions for saving/loading pictures.
 *    Copyright 2002 by Jeroen Vreeken (pe1rxq@amsat.org)
 *    Portions of this file are Copyright by Lionnel Maugis
 *    Portions of this file are Copyright 2010 by Wim Lewis (wiml@hhhh.org)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */

#include "picture.h"
#include "event.h"

#include <assert.h>

#undef HAVE_STDLIB_H
#include <jpeglib.h>
#include <jerror.h>

/*
 * The following declarations and 5 functions are jpeg related
 * functions used by put_jpeg_grey_memory and put_jpeg_yuv420p_memory.
 */
typedef struct {
    struct jpeg_destination_mgr pub;
    JOCTET *buf;
    size_t bufsize;
    size_t jpegsize;
} mem_destination_mgr;

typedef mem_destination_mgr *mem_dest_ptr;


METHODDEF(void) init_destination(j_compress_ptr cinfo)
{
    mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;
    dest->pub.next_output_byte = dest->buf;
    dest->pub.free_in_buffer = dest->bufsize;
    dest->jpegsize = 0;
}

METHODDEF(boolean) empty_output_buffer(j_compress_ptr cinfo)
{
    mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;
    dest->pub.next_output_byte = dest->buf;
    dest->pub.free_in_buffer = dest->bufsize;

    return FALSE;
    ERREXIT(cinfo, JERR_BUFFER_SIZE);
}

METHODDEF(void) term_destination(j_compress_ptr cinfo)
{
    mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;
    dest->jpegsize = dest->bufsize - dest->pub.free_in_buffer;
}

static GLOBAL(void) _jpeg_mem_dest(j_compress_ptr cinfo, JOCTET* buf, size_t bufsize)
{
    mem_dest_ptr dest;

    if (cinfo->dest == NULL) {
        cinfo->dest = (struct jpeg_destination_mgr *)
                      (*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT,
                       sizeof(mem_destination_mgr));
    }

    dest = (mem_dest_ptr) cinfo->dest;

    dest->pub.init_destination    = init_destination;
    dest->pub.empty_output_buffer = empty_output_buffer;
    dest->pub.term_destination    = term_destination;

    dest->buf      = buf;
    dest->bufsize  = bufsize;
    dest->jpegsize = 0;
}

static GLOBAL(int) _jpeg_mem_size(j_compress_ptr cinfo)
{
    mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;
    return dest->jpegsize;
}

/* EXIF image data is always in TIFF format, even if embedded in another
 * file type. This consists of a constant header (TIFF file header,
 * IFD header) followed by the tags in the IFD and then the data
 * from any tags which do not fit inline in the IFD.
 *
 * The tags we write in the main IFD are:
 *  0x010E   Image description
 *  0x8769   Exif sub-IFD
 *  0x882A   Time zone of time stamps
 * and in the Exif sub-IFD:
 *  0x9000   Exif version
 *  0x9003   File date and time
 *  0x9291   File date and time subsecond info
 * But we omit any empty IFDs.
 */

#define TIFF_TAG_IMAGE_DESCRIPTION    0x010E
#define TIFF_TAG_DATETIME             0x0132
#define TIFF_TAG_EXIF_IFD             0x8769
#define TIFF_TAG_TZ_OFFSET            0x882A

#define EXIF_TAG_EXIF_VERSION         0x9000
#define EXIF_TAG_ORIGINAL_DATETIME    0x9003
#define EXIF_TAG_SUBJECT_AREA         0x9214
#define EXIF_TAG_TIFF_DATETIME_SS     0x9290
#define EXIF_TAG_ORIGINAL_DATETIME_SS 0x9291

#define TIFF_TYPE_ASCII  2  /* ASCII text */
#define TIFF_TYPE_USHORT 3  /* Unsigned 16-bit int */
#define TIFF_TYPE_LONG   4  /* Unsigned 32-bit int */
#define TIFF_TYPE_UNDEF  7  /* Byte blob */
#define TIFF_TYPE_SSHORT 8  /* Signed 16-bit int */

static const char exif_marker_start[14] = {
    'E', 'x', 'i', 'f', 0, 0,   /* EXIF marker signature */
    'M', 'M', 0, 42,            /* TIFF file header (big-endian) */
    0, 0, 0, 8,                 /* Offset to first toplevel IFD */
};

static const char exif_version_tag[12] = {
    0x90, 0x00,                 /* EXIF version tag, 0x9000 */
    0x00, 0x07,                 /* Data type 7 = "unknown" (raw byte blob) */
    0x00, 0x00, 0x00, 0x04,     /* Data length */
    0x30, 0x32, 0x32, 0x30      /* Inline data, EXIF version 2.2 */
};

static const char exif_subifd_tag[8] = {
    0x87, 0x69,                 /* EXIF Sub-IFD tag */
    0x00, 0x04,                 /* Data type 4 = uint32 */
    0x00, 0x00, 0x00, 0x01,     /* Number of values */
};

static const char exif_tzoffset_tag[12] = {
    0x88, 0x2A,                 /* TIFF/EP time zone offset tag */
    0x00, 0x08,                 /* Data type 8 = sint16 */
    0x00, 0x00, 0x00, 0x01,     /* Number of values */
    0, 0, 0, 0                  /* Dummy data */
};

static void put_uint16(JOCTET *buf, unsigned value)
{
    buf[0] = ( value & 0xFF00 ) >> 8;
    buf[1] = ( value & 0x00FF );
}

static void put_sint16(JOCTET *buf, int value)
{
    buf[0] = ( value & 0xFF00 ) >> 8;
    buf[1] = ( value & 0x00FF );
}

static void put_uint32(JOCTET *buf, unsigned value)
{
    buf[0] = ( value & 0xFF000000 ) >> 24;
    buf[1] = ( value & 0x00FF0000 ) >> 16;
    buf[2] = ( value & 0x0000FF00 ) >> 8;
    buf[3] = ( value & 0x000000FF );
}

struct tiff_writing {
    JOCTET * const base;
    JOCTET *buf;
    unsigned data_offset;
};

static void put_direntry(struct tiff_writing *into, const char *data, unsigned length)
{
    if (length <= 4) {
	    /* Entries that fit in the directory entry are stored there */
	    memset(into->buf, 0, 4);
	    memcpy(into->buf, data, length);
    } else {
	    /* Longer entries are stored out-of-line */
	    unsigned offset = into->data_offset;

        while ((offset & 0x03) != 0) {  /* Alignment */
	        into->base[offset] = 0;
	        offset ++;
	    }

	    put_uint32(into->buf, offset);
	    memcpy(into->base + offset, data, length);
	    into->data_offset = offset + length;
    }
}

static void put_stringentry(struct tiff_writing *into, unsigned tag, const char *str, int with_nul)
{
    unsigned stringlength = strlen(str) + (with_nul?1:0);

    put_uint16(into->buf, tag);
    put_uint16(into->buf + 2, TIFF_TYPE_ASCII);
    put_uint32(into->buf + 4, stringlength);
    into->buf += 8;
    put_direntry(into, str, stringlength);
    into->buf += 4;
}

static void put_subjectarea(struct tiff_writing *into, const struct coord *box)
{
    put_uint16(into->buf    , EXIF_TAG_SUBJECT_AREA);
    put_uint16(into->buf + 2, TIFF_TYPE_USHORT);
    put_uint32(into->buf + 4, 4 /* Four USHORTs */);
    put_uint32(into->buf + 8, into->data_offset);
    into->buf += 12;
    JOCTET *ool = into->base + into->data_offset;
    put_uint16(ool  , box->x); /* Center.x */
    put_uint16(ool+2, box->y); /* Center.y */
    put_uint16(ool+4, box->width);
    put_uint16(ool+6, box->height);
    into->data_offset += 8;
}

/*
 * put_jpeg_exif writes the EXIF APP1 chunk to the jpeg file.
 * It must be called after jpeg_start_compress() but before
 * any image data is written by jpeg_write_scanlines().
 */
static void put_jpeg_exif(j_compress_ptr cinfo,
			  const struct context *cnt,
			  const struct tm *timestamp,
			  const struct coord *box)
{
    /* description, datetime, and subtime are the values that are actually
     * put into the EXIF data
    */
    char *description, *datetime, *subtime;
    char datetime_buf[22];

    if (timestamp) {
	/* Exif requires this exact format */
	    snprintf(datetime_buf, 21, "%04d:%02d:%02d %02d:%02d:%02d",
		        timestamp->tm_year + 1900,
		        timestamp->tm_mon + 1,
		        timestamp->tm_mday,
		        timestamp->tm_hour,
		        timestamp->tm_min,
		        timestamp->tm_sec);
	    datetime = datetime_buf;
    } else {
	    datetime = NULL;
    }

    // TODO: Extract subsecond timestamp from somewhere, but only
    // use as much of it as is indicated by conf->frame_limit
    subtime = NULL;

    if (cnt->conf.exif_text) {
	    description = malloc(PATH_MAX);
	    mystrftime(cnt, description, PATH_MAX-1,
		        cnt->conf.exif_text,
		        timestamp, NULL, 0);
    } else {
	    description = NULL;
    }

    /* Calculate an upper bound on the size of the APP1 marker so
     * we can allocate a buffer for it.
     */

    /* Count up the number of tags and max amount of OOL data */
    int ifd0_tagcount = 0;
    int ifd1_tagcount = 0;
    unsigned datasize = 0;

    if (description) {
	    ifd0_tagcount ++;
	    datasize += 5 + strlen(description); /* Add 5 for NUL and alignment */
    }

    if (datetime) {
	/* We write this to both the TIFF datetime tag (which most programs
	 * treat as "last-modified-date") and the EXIF "time of creation of
	 * original image" tag (which many programs ignore). This is
	 * redundant but seems to be the thing to do.
	 */
	    ifd0_tagcount++;
	    ifd1_tagcount++;
	    /* We also write the timezone-offset tag in IFD0 */
	    ifd0_tagcount++;
	    /* It would be nice to use the same offset for both tags' values,
	    * but I don't want to write the bookkeeping for that right now */
	    datasize += 2 * (5 + strlen(datetime));
    }

    if (subtime) {
	    ifd1_tagcount++;
	    datasize += 5 + strlen(subtime);
    }

    if (box) {
	    ifd1_tagcount++;
	    datasize += 2 * 4;  /* Four 16-bit ints */
    }

    if (ifd1_tagcount > 0) {
	    /* If we're writing the Exif sub-IFD, account for the
	    * two tags that requires */
	    ifd0_tagcount ++; /* The tag in IFD0 that points to IFD1 */
	    ifd1_tagcount ++; /* The EXIF version tag */
    }

    /* Each IFD takes 12 bytes per tag, plus six more (the tag count and the
     * pointer to the next IFD, always zero in our case)
     */
    unsigned int ifds_size =
	( ifd1_tagcount > 0 ? ( 12 * ifd1_tagcount + 6 ) : 0 ) +
	( ifd0_tagcount > 0 ? ( 12 * ifd0_tagcount + 6 ) : 0 );

    if (ifds_size == 0) {
	    /* We're not actually going to write any information. */
	    return;
    }

    unsigned int buffer_size = 6 /* EXIF marker signature */ +
                               8 /* TIFF file header */ +
                               ifds_size /* the tag directories */ +
                               datasize;

    JOCTET *marker = malloc(buffer_size);
    memcpy(marker, exif_marker_start, 14); /* EXIF and TIFF headers */

    struct tiff_writing writing = (struct tiff_writing) {
	.base = marker + 6, /* base address for intra-TIFF offsets */
	.buf = marker + 14, /* current write position */
	.data_offset = 8 + ifds_size, /* where to start storing data */
    };

    /* Write IFD 0 */
    /* Note that tags are stored in numerical order */
    put_uint16(writing.buf, ifd0_tagcount);
    writing.buf += 2;

    if (description)
	    put_stringentry(&writing, TIFF_TAG_IMAGE_DESCRIPTION, description, 0);

    if (datetime)
	    put_stringentry(&writing, TIFF_TAG_DATETIME, datetime, 1);

    if (ifd1_tagcount > 0) {
	    /* Offset of IFD1 - TIFF header + IFD0 size. */
	    unsigned ifd1_offset = 8 + 6 + ( 12 * ifd0_tagcount );
	    memcpy(writing.buf, exif_subifd_tag, 8);
	    put_uint32(writing.buf + 8, ifd1_offset);
	    writing.buf += 12;
    }

    if (datetime) {
        memcpy(writing.buf, exif_tzoffset_tag, 12);
        put_sint16(writing.buf+8, timestamp->tm_gmtoff / 3600);
        writing.buf += 12;
    }

    put_uint32(writing.buf, 0); /* Next IFD offset = 0 (no next IFD) */
    writing.buf += 4;

    /* Write IFD 1 */
    if (ifd1_tagcount > 0) {
	    /* (remember that the tags in any IFD must be in numerical order
	    * by tag) */
	    put_uint16(writing.buf, ifd1_tagcount);
	    memcpy(writing.buf + 2, exif_version_tag, 12); /* tag 0x9000 */
	    writing.buf += 14;

	    if (datetime)
	        put_stringentry(&writing, EXIF_TAG_ORIGINAL_DATETIME, datetime, 1);

        if (box)
	        put_subjectarea(&writing, box);

        if (subtime)
	        put_stringentry(&writing, EXIF_TAG_ORIGINAL_DATETIME_SS, subtime, 0);

	    put_uint32(writing.buf, 0); /* Next IFD = 0 (no next IFD) */
	    writing.buf += 4;
    }

    /* We should have met up with the OOL data */
    assert( (writing.buf - writing.base) == 8 + ifds_size );

    /* The buffer is complete; write it out */
    unsigned marker_len = 6 + writing.data_offset;

    /* assert we didn't underestimate the original buffer size */
    assert(marker_len <= buffer_size);

    /* EXIF data lives in a JPEG APP1 marker */
    jpeg_write_marker(cinfo, JPEG_APP0 + 1, marker, marker_len);

    free(description);

    free(marker);
}

/**
 * put_jpeg_yuv420p_memory
 *      Converts an input image in the YUV420P format into a jpeg image and puts
 *      it in a memory buffer.
 * Inputs:
 * - image_size is the size of the input image buffer.
 * - input_image is the image in YUV420P format.
 * - width and height are the dimensions of the image
 * - quality is the jpeg encoding quality 0-100%
 *
 * Output:
 * - dest_image is a pointer to the jpeg image buffer
 *
 * Returns buffer size of jpeg image
 */
static int put_jpeg_yuv420p_memory(unsigned char *dest_image, int image_size,
				   unsigned char *input_image, int width, int height, int quality,
				   struct context *cnt, struct tm *tm, struct coord *box)

{
    int i, j, jpeg_image_size;

    JSAMPROW y[16],cb[16],cr[16]; // y[2][5] = color sample of row 2 and pixel column 5; (one plane)
    JSAMPARRAY data[3]; // t[0][2][5] = color sample 0 of row 2 and column 5

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    data[0] = y;
    data[1] = cb;
    data[2] = cr;

    cinfo.err = jpeg_std_error(&jerr);  // Errors get written to stderr

    jpeg_create_compress(&cinfo);
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    jpeg_set_defaults(&cinfo);

    jpeg_set_colorspace(&cinfo, JCS_YCbCr);

    cinfo.raw_data_in = TRUE; // Supply downsampled data
#if JPEG_LIB_VERSION >= 70
    cinfo.do_fancy_downsampling = FALSE;  // Fix segfault with v7
#endif
    cinfo.comp_info[0].h_samp_factor = 2;
    cinfo.comp_info[0].v_samp_factor = 2;
    cinfo.comp_info[1].h_samp_factor = 1;
    cinfo.comp_info[1].v_samp_factor = 1;
    cinfo.comp_info[2].h_samp_factor = 1;
    cinfo.comp_info[2].v_samp_factor = 1;

    jpeg_set_quality(&cinfo, quality, TRUE);
    cinfo.dct_method = JDCT_FASTEST;

    _jpeg_mem_dest(&cinfo, dest_image, image_size);  // Data written to mem

    jpeg_start_compress(&cinfo, TRUE);

    put_jpeg_exif(&cinfo, cnt, tm, box);

    for (j = 0; j < height; j += 16) {
        for (i = 0; i < 16; i++) {
            y[i] = input_image + width * (i + j);

            if (i % 2 == 0) {
                cb[i / 2] = input_image + width * height + width / 2 * ((i + j) /2);
                cr[i / 2] = input_image + width * height + width * height / 4 + width / 2 * ((i + j) / 2);
            }
        }
        jpeg_write_raw_data(&cinfo, data, 16);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_image_size = _jpeg_mem_size(&cinfo);
    jpeg_destroy_compress(&cinfo);

    return jpeg_image_size;
}

/**
 * put_jpeg_grey_memory
 *      Converts an input image in the grayscale format into a jpeg image.
 *
 * Inputs:
 * - image_size is the size of the input image buffer.
 * - input_image is the image in grayscale format.
 * - width and height are the dimensions of the image
 * - quality is the jpeg encoding quality 0-100%
 *
 * Output:
 * - dest_image is a pointer to the jpeg image buffer
 *
 * Returns buffer size of jpeg image.
 */
static int put_jpeg_grey_memory(unsigned char *dest_image, int image_size, unsigned char *input_image, int width, int height, int quality)
{
    int y, dest_image_size;
    JSAMPROW row_ptr[1];
    struct jpeg_compress_struct cjpeg;
    struct jpeg_error_mgr jerr;

    cjpeg.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cjpeg);
    cjpeg.image_width = width;
    cjpeg.image_height = height;
    cjpeg.input_components = 1; /* One colour component */
    cjpeg.in_color_space = JCS_GRAYSCALE;

    jpeg_set_defaults(&cjpeg);

    jpeg_set_quality(&cjpeg, quality, TRUE);
    cjpeg.dct_method = JDCT_FASTEST;
    _jpeg_mem_dest(&cjpeg, dest_image, image_size);  // Data written to mem

    jpeg_start_compress (&cjpeg, TRUE);

    put_jpeg_exif(&cjpeg, NULL, NULL, NULL);

    row_ptr[0] = input_image;

    for (y = 0; y < height; y++) {
        jpeg_write_scanlines(&cjpeg, row_ptr, 1);
        row_ptr[0] += width;
    }

    jpeg_finish_compress(&cjpeg);
    dest_image_size = _jpeg_mem_size(&cjpeg);
    jpeg_destroy_compress(&cjpeg);

    return dest_image_size;
}

/**
 * put_jpeg_yuv420p_file
 *      Converts an YUV420P coded image to a jpeg image and writes
 *      it to an already open file.
 *
 * Inputs:
 * - image is the image in YUV420P format.
 * - width and height are the dimensions of the image
 * - quality is the jpeg encoding quality 0-100%
 *
 * Output:
 * - The jpeg is written directly to the file given by the file pointer fp
 *
 * Returns nothing
 */
static void put_jpeg_yuv420p_file(FILE *fp,
				  unsigned char *image, int width, int height,
				  int quality,
				  struct context *cnt, struct tm *tm, struct coord *box)
{
    int i, j;

    JSAMPROW y[16],cb[16],cr[16]; // y[2][5] = color sample of row 2 and pixel column 5; (one plane)
    JSAMPARRAY data[3]; // t[0][2][5] = color sample 0 of row 2 and column 5

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    data[0] = y;
    data[1] = cb;
    data[2] = cr;

    cinfo.err = jpeg_std_error(&jerr);  // Errors get written to stderr

    jpeg_create_compress(&cinfo);
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    jpeg_set_defaults(&cinfo);

    jpeg_set_colorspace(&cinfo, JCS_YCbCr);

    cinfo.raw_data_in = TRUE; // Supply downsampled data
#if JPEG_LIB_VERSION >= 70
    cinfo.do_fancy_downsampling = FALSE;  // Fix segfault with v7
#endif
    cinfo.comp_info[0].h_samp_factor = 2;
    cinfo.comp_info[0].v_samp_factor = 2;
    cinfo.comp_info[1].h_samp_factor = 1;
    cinfo.comp_info[1].v_samp_factor = 1;
    cinfo.comp_info[2].h_samp_factor = 1;
    cinfo.comp_info[2].v_samp_factor = 1;

    jpeg_set_quality(&cinfo, quality, TRUE);
    cinfo.dct_method = JDCT_FASTEST;

    jpeg_stdio_dest(&cinfo, fp);        // Data written to file
    jpeg_start_compress(&cinfo, TRUE);

    put_jpeg_exif(&cinfo, cnt, tm, box);

    for (j = 0; j < height; j += 16) {
        for (i = 0; i < 16; i++) {
            y[i] = image + width * (i + j);
            if (i % 2 == 0) {
                cb[i / 2] = image + width * height + width / 2 * ((i + j) / 2);
                cr[i / 2] = image + width * height + width * height / 4 + width / 2 * ((i + j) / 2);
            }
        }
        jpeg_write_raw_data(&cinfo, data, 16);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
}


/**
 * put_jpeg_grey_file
 *      Converts an greyscale image to a jpeg image and writes
 *      it to an already open file.
 *
 * Inputs:
 * - image is the image in greyscale format.
 * - width and height are the dimensions of the image
 * - quality is the jpeg encoding quality 0-100%
 * Output:
 * - The jpeg is written directly to the file given by the file pointer fp
 *
 * Returns nothing
 */
static void put_jpeg_grey_file(FILE *picture, unsigned char *image, int width, int height, int quality)
{
    int y;
    JSAMPROW row_ptr[1];
    struct jpeg_compress_struct cjpeg;
    struct jpeg_error_mgr jerr;

    cjpeg.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cjpeg);
    cjpeg.image_width = width;
    cjpeg.image_height = height;
    cjpeg.input_components = 1; /* One colour component */
    cjpeg.in_color_space = JCS_GRAYSCALE;

    jpeg_set_defaults(&cjpeg);

    jpeg_set_quality(&cjpeg, quality, TRUE);
    cjpeg.dct_method = JDCT_FASTEST;
    jpeg_stdio_dest(&cjpeg, picture);

    jpeg_start_compress(&cjpeg, TRUE);

    put_jpeg_exif(&cjpeg, NULL, NULL, NULL);

    row_ptr[0] = image;

    for (y = 0; y < height; y++) {
        jpeg_write_scanlines(&cjpeg, row_ptr, 1);
        row_ptr[0] += width;
    }

    jpeg_finish_compress(&cjpeg);
    jpeg_destroy_compress(&cjpeg);
}


/**
 * put_ppm_bgr24_file
 *      Converts an greyscale image to a PPM image and writes
 *      it to an already open file.
 * Inputs:
 * - image is the image in YUV420P format.
 * - width and height are the dimensions of the image
 *
 * Output:
 * - The PPM is written directly to the file given by the file pointer fp
 *
 * Returns nothing
 */
static void put_ppm_bgr24_file(FILE *picture, unsigned char *image, int width, int height)
{
    int x, y;
    unsigned char *l = image;
    unsigned char *u = image + width * height;
    unsigned char *v = u + (width * height) / 4;
    int r, g, b;
    unsigned char rgb[3];

    /*
     *  ppm header
     *  width height
     *  maxval
     */
    fprintf(picture, "P6\n");
    fprintf(picture, "%d %d\n", width, height);
    fprintf(picture, "%d\n", 255);
    for (y = 0; y < height; y++) {

        for (x = 0; x < width; x++) {
            r = 76283 * (((int)*l) - 16)+104595*(((int)*u) - 128);
            g = 76283 * (((int)*l) - 16)- 53281*(((int)*u) - 128) - 25625 * (((int)*v) - 128);
            b = 76283 * (((int)*l) - 16) + 132252 * (((int)*v) - 128);
            r = r >> 16;
            g = g >> 16;
            b = b >> 16;
            if (r < 0)
                r = 0;
            else if (r > 255)
                r = 255;
            if (g < 0)
                g = 0;
            else if (g > 255)
                g = 255;
            if (b < 0)
                b = 0;
            else if (b > 255)
                b = 255;

            rgb[0] = b;
            rgb[1] = g;
            rgb[2] = r;

            l++;
            if (x & 1) {
                u++;
                v++;
            }
            /* ppm is rgb not bgr */
            fwrite(rgb, 1, 3, picture);
        }
        if (y & 1) {
            u -= width / 2;
            v -= width / 2;
        }
    }
}

/**
 * overlay_smartmask
 *      Copies smartmask as an overlay into motion images and movies.
 *
 * Returns nothing.
 */
void overlay_smartmask(struct context *cnt, unsigned char *out)
{
    int i, x, v, width, height, line;
    struct images *imgs = &cnt->imgs;
    unsigned char *smartmask = imgs->smartmask_final;
    unsigned char *out_y, *out_u, *out_v;

    i = imgs->motionsize;
    v = i + ((imgs->motionsize) / 4);
    width = imgs->width;
    height = imgs->height;

    /* Set V to 255 to make smartmask appear red. */
    out_v = out + v;
    out_u = out + i;
    for (i = 0; i < height; i += 2) {
        line = i * width;
        for (x = 0; x < width; x += 2) {
            if (smartmask[line + x] == 0 || smartmask[line + x + 1] == 0 ||
                smartmask[line + width + x] == 0 ||
                smartmask[line + width + x + 1] == 0) {

                *out_v = 255;
                *out_u = 128;
            }
            out_v++;
            out_u++;
        }
    }
    out_y = out;
    /* Set colour intensity for smartmask. */
    for (i = 0; i < imgs->motionsize; i++) {
        if (smartmask[i] == 0)
            *out_y = 0;
        out_y++;
    }
}

/**
 * overlay_fixed_mask
 *      Copies fixed mask as green overlay into motion images and movies.
 *
 * Returns nothing.
 */
void overlay_fixed_mask(struct context *cnt, unsigned char *out)
{
    int i, x, v, width, height, line;
    struct images *imgs = &cnt->imgs;
    unsigned char *mask = imgs->mask;
    unsigned char *out_y, *out_u, *out_v;

    i = imgs->motionsize;
    v = i + ((imgs->motionsize) / 4);
    width = imgs->width;
    height = imgs->height;

    /* Set U and V to 0 to make fixed mask appear green. */
    out_v = out + v;
    out_u = out + i;
    for (i = 0; i < height; i += 2) {
        line = i * width;
        for (x = 0; x < width; x += 2) {
            if (mask[line + x] == 0 || mask[line + x + 1] == 0 ||
                mask[line + width + x] == 0 ||
                mask[line + width + x + 1] == 0) {

                *out_v = 0;
                *out_u = 0;
            }
            out_v++;
            out_u++;
        }
    }
    out_y = out;
    /* Set colour intensity for mask. */
    for (i = 0; i < imgs->motionsize; i++) {
        if (mask[i] == 0)
            *out_y = 0;
        out_y++;
    }
}

/**
 * overlay_largest_label
 *      Copies largest label as an overlay into motion images and movies.
 *
 * Returns nothing.
 */
void overlay_largest_label(struct context *cnt, unsigned char *out)
{
    int i, x, v, width, height, line;
    struct images *imgs = &cnt->imgs;
    int *labels = imgs->labels;
    unsigned char *out_y, *out_u, *out_v;

    i = imgs->motionsize;
    v = i + ((imgs->motionsize) / 4);
    width = imgs->width;
    height = imgs->height;

    /* Set U to 255 to make label appear blue. */
    out_u = out + i;
    out_v = out + v;
    for (i = 0; i < height; i += 2) {
        line = i * width;
        for (x = 0; x < width; x += 2) {
            if (labels[line + x] & 32768 || labels[line + x + 1] & 32768 ||
                labels[line + width + x] & 32768 ||
                labels[line + width + x + 1] & 32768) {

                *out_u = 255;
                *out_v = 128;
            }
            out_u++;
            out_v++;
        }
    }
    out_y = out;
    /* Set intensity for coloured label to have better visibility. */
    for (i = 0; i < imgs->motionsize; i++) {
        if (*labels++ & 32768)
            *out_y = 0;
        out_y++;
    }
}

/**
 * put_picture_mem
 *      Is used for the webcam feature. Depending on the image type
 *      (colour YUV420P or greyscale) the corresponding put_jpeg_X_memory function is called.
 * Inputs:
 * - cnt is the global context struct and only cnt->imgs.type is used.
 * - image_size is the size of the input image buffer
 * - *image points to the image buffer that contains the YUV420P or Grayscale image about to be put
 * - quality is the jpeg quality setting from the config file.
 *
 * Output:
 * - **dest_image is a pointer to a pointer that points to the destination buffer in which the
 *   converted image it put
 *
 * Returns the dest_image_size if successful. Otherwise 0.
 */
int put_picture_memory(struct context *cnt, unsigned char* dest_image, int image_size,
                       unsigned char *image, int quality)
{
    switch (cnt->imgs.type) {
    case VIDEO_PALETTE_YUV420P:
        return put_jpeg_yuv420p_memory(dest_image, image_size, image,
                                       cnt->imgs.width, cnt->imgs.height, quality, cnt, &(cnt->current_image->timestamp_tm), &(cnt->current_image->location));
    case VIDEO_PALETTE_GREY:
        return put_jpeg_grey_memory(dest_image, image_size, image,
                                    cnt->imgs.width, cnt->imgs.height, quality);
    default:
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO, "%s: Unknow image type %d",
                   cnt->imgs.type);
    }

    return 0;
}

void put_picture_fd(struct context *cnt, FILE *picture, unsigned char *image, int quality)
{
    if (cnt->imgs.picture_type == IMAGE_TYPE_PPM) {
        put_ppm_bgr24_file(picture, image, cnt->imgs.width, cnt->imgs.height);
    } else {
        switch (cnt->imgs.type) {
        case VIDEO_PALETTE_YUV420P:
            put_jpeg_yuv420p_file(picture, image, cnt->imgs.width, cnt->imgs.height, quality, cnt, &(cnt->current_image->timestamp_tm), &(cnt->current_image->location));
            break;
        case VIDEO_PALETTE_GREY:
            put_jpeg_grey_file(picture, image, cnt->imgs.width, cnt->imgs.height, quality);
            break;
        default:
            MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO, "%s: Unknow image type %d",
                       cnt->imgs.type);
        }
    }
}


void put_picture(struct context *cnt, char *file, unsigned char *image, int ftype)
{
    FILE *picture;

    picture = myfopen(file, "w", BUFSIZE_1MEG);
    if (!picture) {
        /* Report to syslog - suggest solution if the problem is access rights to target dir. */
        if (errno ==  EACCES) {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO,
                       "%s: Can't write picture to file %s - check access rights to target directory\n"
                       "Thread is going to finish due to this fatal error", file);
            cnt->finish = 1;
            cnt->restart = 0;
            return;
        } else {
            /* If target dir is temporarily unavailable we may survive. */
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, "%s: Can't write picture to file %s", file);
            return;
        }
    }

    put_picture_fd(cnt, picture, image, cnt->conf.quality);
    myfclose(picture);
    event(cnt, EVENT_FILECREATE, NULL, file, (void *)(unsigned long)ftype, NULL);
}

/**
 * get_pgm
 *      Get the pgm file used as fixed mask
 *
 */
unsigned char *get_pgm(FILE *picture, int width, int height)
{
    int x = 0 , y = 0, maxval;
    char line[256];
    unsigned char *image;

    line[255] = 0;

    if (!fgets(line, 255, picture)) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, "%s: Could not read from ppm file");
        return NULL;
    }

    if (strncmp(line, "P5", 2)) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, "%s: This is not a ppm file, starts with '%s'",
                   line);
        return NULL;
    }

    /* Skip comment */
    line[0] = '#';
    while (line[0] == '#')
        if (!fgets(line, 255, picture))
            return NULL;

    /* Check size */
    if (sscanf(line, "%d %d", &x, &y) != 2) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, "%s: Failed reading size in pgm file");
        return NULL;
    }

    if (x != width || y != height) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, "%s: Wrong image size %dx%d should be %dx%d",
                   x, y, width, height);
        return NULL;
    }

    /* Maximum value */
    line[0] = '#';
    while (line[0] == '#')
        if (!fgets(line, 255, picture))
            return NULL;

    if (sscanf(line, "%d", &maxval) != 1) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, "%s: Failed reading maximum value in pgm file");
        return NULL;
    }

    /* Read data */

    image = mymalloc(width * height);

    for (y = 0; y < height; y++) {
        if ((int)fread(&image[y * width], 1, width, picture) != width)
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, "%s: Failed reading image data from pgm file");

        for (x = 0; x < width; x++)
            image[y * width + x] = (int)image[y * width + x] * 255 / maxval;

    }

    return image;
}

/**
 * put_fixed_mask
 *      If a mask file is asked for but does not exist this function
 *      creates an empty mask file in the right binary pgm format and
 *      and the right size - easy to edit with Gimp or similar tool.
 *
 * Returns nothing.
 */
void put_fixed_mask(struct context *cnt, const char *file)
{
    FILE *picture;

    picture = myfopen(file, "w", BUFSIZE_1MEG);
    if (!picture) {
        /* Report to syslog - suggest solution if the problem is access rights to target dir. */
        if (errno ==  EACCES) {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO,
                       "%s: can't write mask file %s - check access rights to target directory",
                       file);
        } else {
            /* If target dir is temporarily unavailable we may survive. */
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, "%s: can't write mask file %s", file);
        }
        return;
    }
    memset(cnt->imgs.out, 255, cnt->imgs.motionsize); /* Initialize to unset */

    /* Write pgm-header. */
    fprintf(picture, "P5\n");
    fprintf(picture, "%d %d\n", cnt->conf.width, cnt->conf.height);
    fprintf(picture, "%d\n", 255);

    /* Write pgm image data at once. */
    if ((int)fwrite(cnt->imgs.out, cnt->conf.width, cnt->conf.height, picture) != cnt->conf.height) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, "%s: Failed writing default mask as pgm file");
        return;
    }

    myfclose(picture);

    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, "%s: Creating empty mask %s\nPlease edit this file and "
               "re-run motion to enable mask feature", cnt->conf.mask_file);
}

/**
 * preview_save
 *      save preview_shot
 *
 * Returns nothing.
 */
void preview_save(struct context *cnt)
{
    int use_imagepath;
    int basename_len;
    const char *imagepath;
    char previewname[PATH_MAX];
    char filename[PATH_MAX];
    struct image_data *saved_current_image;

    if (cnt->imgs.preview_image.diffs) {
        /* Save current global context. */
        saved_current_image = cnt->current_image;
        /* Set global context to the image we are processing. */
        cnt->current_image = &cnt->imgs.preview_image;

        /* Use filename of movie i.o. jpeg_filename when set to 'preview'. */
        use_imagepath = strcmp(cnt->conf.imagepath, "preview");

#ifdef HAVE_FFMPEG
        if ((cnt->ffmpeg_output || (cnt->conf.useextpipe && cnt->extpipe))
            && !use_imagepath) {
#else
        if ((cnt->conf.useextpipe && cnt->extpipe) && !use_imagepath) {
#endif
            if (cnt->conf.useextpipe && cnt->extpipe) {
                basename_len = strlen(cnt->extpipefilename) + 1;
                strncpy(previewname, cnt->extpipefilename, basename_len);
                previewname[basename_len - 1] = '.';
            } else {
                /* Replace avi/mpg with jpg/ppm and keep the rest of the filename. */
                basename_len = strlen(cnt->newfilename) - 3;
                strncpy(previewname, cnt->newfilename, basename_len);
            }

            previewname[basename_len] = '\0';
            strcat(previewname, imageext(cnt));
            put_picture(cnt, previewname, cnt->imgs.preview_image.image , FTYPE_IMAGE);
        } else {
            /*
             * Save best preview-shot also when no movies are recorded or imagepath
             * is used. Filename has to be generated - nothing available to reuse!
             */
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "%s: different filename or picture only!");
            /*
             * conf.imagepath would normally be defined but if someone deleted it by
             * control interface it is better to revert to the default than fail.
             */
            if (cnt->conf.imagepath)
                imagepath = cnt->conf.imagepath;
            else
                imagepath = (char *)DEF_IMAGEPATH;

            mystrftime(cnt, filename, sizeof(filename), imagepath, &cnt->imgs.preview_image.timestamp_tm, NULL, 0);
            snprintf(previewname, PATH_MAX, "%s/%s.%s", cnt->conf.filepath, filename, imageext(cnt));

            put_picture(cnt, previewname, cnt->imgs.preview_image.image, FTYPE_IMAGE);
        }

        /* Restore global context values. */
        cnt->current_image = saved_current_image;
    }
}
