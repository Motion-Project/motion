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
#include "translate.h"
#include "picture.h"
#include "jpegutils.h"
#include "event.h"

#include <assert.h>

#ifdef HAVE_WEBP
#include <webp/encode.h>
#include <webp/mux.h>
#endif /* HAVE_WEBP */


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
 * prepare_exif() is a comon function used to prepare
 * exif data to be inserted into jpeg or webp files
 *
 */
unsigned prepare_exif(unsigned char **exif,
              const struct context *cnt,
              const struct timeval *tv_in1,
              const struct coord *box)
{
    /* description, datetime, and subtime are the values that are actually
     * put into the EXIF data
    */
    char *description, *datetime, *subtime;
    char datetime_buf[22];
    char tmpbuf[45];
    struct tm timestamp_tm;
    struct timeval tv1;

    gettimeofday(&tv1, NULL);
    if (tv_in1 != NULL) {
        tv1.tv_sec = tv_in1->tv_sec;
        tv1.tv_usec = tv_in1->tv_usec;
    }

    localtime_r(&tv1.tv_sec, &timestamp_tm);
    /* Exif requires this exact format */
    /* The compiler is twitchy on truncating formats and the exif is twitchy
     * on the length of the whole string.  So we do it in two steps of printing
     * into a large buffer which compiler wants, then print that into the smaller
     * buffer that exif wants..TODO  Find better method
     */
    snprintf(tmpbuf, 45, "%04d:%02d:%02d %02d:%02d:%02d",
            timestamp_tm.tm_year + 1900,
            timestamp_tm.tm_mon + 1,
            timestamp_tm.tm_mday,
            timestamp_tm.tm_hour,
            timestamp_tm.tm_min,
            timestamp_tm.tm_sec);
    snprintf(datetime_buf, 22,"%.21s",tmpbuf);
    datetime = datetime_buf;

    // TODO: Extract subsecond timestamp from somewhere, but only
    // use as much of it as is indicated by conf->frame_limit
    subtime = NULL;

    if (cnt->conf.picture_exif) {
        description = malloc(PATH_MAX);
        mystrftime(cnt, description, PATH_MAX-1, cnt->conf.picture_exif, &tv1, NULL, 0);
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
    int ifds_size =
    ( ifd1_tagcount > 0 ? ( 12 * ifd1_tagcount + 6 ) : 0 ) +
    ( ifd0_tagcount > 0 ? ( 12 * ifd0_tagcount + 6 ) : 0 );

    if (ifds_size == 0) {
        /* We're not actually going to write any information. */
        return 0;
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
        put_stringentry(&writing, TIFF_TAG_IMAGE_DESCRIPTION, description, 1);

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
        put_sint16(writing.buf+8, timestamp_tm.tm_gmtoff / 3600);
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

    free(description);

    *exif = marker;
    return marker_len;
}


#ifdef HAVE_WEBP
/*
 * put_webp_exif writes the EXIF APP1 chunk to the webp file.
 * It must be called after WebPEncode() and the result
 * can then be written out to webp a file
 */
static void put_webp_exif(WebPMux* webp_mux,
              const struct context *cnt,
              const struct timeval *tv1,
              const struct coord *box)
{
    unsigned char *exif = NULL;
    unsigned exif_len = prepare_exif(&exif, cnt, tv1, box);

    if(exif_len > 0) {
        WebPData webp_exif;
        /* EXIF in WEBP does not need the EXIF marker signature (6 bytes) that are needed by jpeg */
        webp_exif.bytes = exif + 6;
        webp_exif.size = exif_len - 6;

        WebPMuxError err = WebPMuxSetChunk(webp_mux, "EXIF", &webp_exif, 1);
        if (err != WEBP_MUX_OK) {
            MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO
                , _("Unable to set set EXIF to webp chunk"));
        }
        free(exif);
    }
}
#endif /* HAVE_WEBP */



#ifdef HAVE_WEBP
/**
 * put_webp_yuv420p_file
 *      Converts an YUV420P coded image to a webp image and writes
 *      it to an already open file.
 *
 * Inputs:
 * - image is the image in YUV420P format.
 * - width and height are the dimensions of the image
 * - quality is the webp encoding quality 0-100%
 *
 * Output:
 * - The webp is written directly to the file given by the file pointer fp
 *
 * Returns nothing
 */
static void put_webp_yuv420p_file(FILE *fp,
                  unsigned char *image, int width, int height,
                  int quality, struct context *cnt, struct timeval *tv1, struct coord *box)
{
    /* Create a config present and check for compatible library version */
    WebPConfig webp_config;
    if (!WebPConfigPreset(&webp_config, WEBP_PRESET_DEFAULT, (float) quality)){
        MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO, _("libwebp version error"));
        return;
    }

    /* Create the input data structure and check for compatible library version */
    WebPPicture webp_image;
    if (!WebPPictureInit(&webp_image)){
        MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO,_("libwebp version error"));
        return;
    }

    /* Allocate the image buffer based on image width and height */
    webp_image.width = width;
    webp_image.height = height;
    if (!WebPPictureAlloc(&webp_image)){
        MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO,_("libwebp image buffer allocation error"));
        return;
    }

    /* Map the input YUV420P buffer as individual Y, U and V pointers */
    webp_image.y = image;
    webp_image.u = image + width * height;
    webp_image.v = webp_image.u + (width * height) / 4;

    /* Setup the memory writting method */
    WebPMemoryWriter webp_writer;
    WebPMemoryWriterInit(&webp_writer);
    webp_image.writer = WebPMemoryWrite;
    webp_image.custom_ptr = (void*) &webp_writer;

    /* Encode the YUV image as webp */
    if (!WebPEncode(&webp_config, &webp_image))
        MOTION_LOG(WRN, TYPE_CORE, NO_ERRNO,_("libwebp image compression error"));

    /* A bitstream object is needed for the muxing proces */
    WebPData webp_bitstream;
    webp_bitstream.bytes = webp_writer.mem;
    webp_bitstream.size = webp_writer.size;

    /* Create a mux from the prepared image data */
    WebPMux* webp_mux = WebPMuxCreate(&webp_bitstream, 1);
    put_webp_exif(webp_mux, cnt, tv1, box);

    /* Add Exif data to the webp image data */
    WebPData webp_output;
    WebPMuxError err = WebPMuxAssemble(webp_mux, &webp_output);
    if (err != WEBP_MUX_OK) {
        MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO,_("unable to assemble webp image"));
    }

    /* Write the webp final bitstream to the file */
    if (fwrite(webp_output.bytes, sizeof(uint8_t), webp_output.size, fp) != webp_output.size)
        MOTION_LOG(ERR, TYPE_CORE, NO_ERRNO,_("unable to save webp image to file"));

#if WEBP_ENCODER_ABI_VERSION > 0x0202
    /* writer.mem must be freed by calling WebPMemoryWriterClear */
    WebPMemoryWriterClear(&webp_writer);
#else
    /* writer.mem must be freed by calling 'free(writer.mem)' */
    free(webp_writer.mem);
#endif /* WEBP_ENCODER_ABI_VERSION */

    /* free the memory used by webp for image data */
    WebPPictureFree(&webp_image);
    /* free the memory used by webp mux object */
    WebPMuxDelete(webp_mux);
    /* free the memory used by webp for output data */
    WebPDataClear(&webp_output);
}
#endif /* HAVE_WEBP */

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
                  struct context *cnt, struct timeval *tv1, struct coord *box)
{
    int sz = 0;
    int image_size = cnt->imgs.size_norm;
    unsigned char *buf = mymalloc(image_size);

    sz = jpgutl_put_yuv420p(buf, image_size, image, width, height, quality, cnt ,tv1, box);
    fwrite(buf, sz, 1, fp);

    free(buf);

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
static void put_jpeg_grey_file(FILE *picture, unsigned char *image, int width, int height,
                  int quality, struct context *cnt, struct timeval *tv1, struct coord *box)

{
    int sz = 0;
    int image_size = cnt->imgs.size_norm;
    unsigned char *buf = mymalloc(image_size);

    sz = jpgutl_put_grey(buf, image_size, image, width, height, quality, cnt ,tv1, box);
    fwrite(buf, sz, 1, picture);

    free(buf);
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
            if (x%2 != 0) {
                u++;
                v++;
            }
            /* ppm is rgb not bgr */
            fwrite(rgb, 1, 3, picture);
        }
        if (y%2 == 0) {
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
 * - cnt is the thread context struct
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
int put_picture_memory(struct context *cnt, unsigned char* dest_image, int image_size, unsigned char *image,
        int quality, int width, int height)
{
    struct timeval tv1;

    /*
     * Reset the time for the current image since it is not reliable
     * for putting images to memory.
     */
    gettimeofday(&tv1, NULL);

    if (!cnt->conf.stream_grey){
        return jpgutl_put_yuv420p(dest_image, image_size, image,
                                       width, height, quality, cnt ,&tv1,NULL);
    } else {
        return jpgutl_put_grey(dest_image, image_size, image,
                                       width, height, quality, cnt,&tv1,NULL);
    }

    return 0;
}

static void put_picture_fd(struct context *cnt, FILE *picture, unsigned char *image, int quality, int ftype){
    int width, height;
    int passthrough;
    int dummy = 1;

    /* See comment in put_picture_memory regarding dummy*/

    passthrough = util_check_passthrough(cnt);
    if ((ftype == FTYPE_IMAGE) && (cnt->imgs.size_high > 0) && (!passthrough)) {
        width = cnt->imgs.width_high;
        height = cnt->imgs.height_high;
    } else {
        width = cnt->imgs.width;
        height = cnt->imgs.height;
    }

    if (cnt->imgs.picture_type == IMAGE_TYPE_PPM) {
        put_ppm_bgr24_file(picture, image, width, height);
    } else {
        if (dummy == 1){
            #ifdef HAVE_WEBP
            if (cnt->imgs.picture_type == IMAGE_TYPE_WEBP)
                put_webp_yuv420p_file(picture, image, width, height, quality, cnt, &(cnt->current_image->timestamp_tv), &(cnt->current_image->location));
            #endif /* HAVE_WEBP */
            if (cnt->imgs.picture_type == IMAGE_TYPE_JPEG)
                put_jpeg_yuv420p_file(picture, image, width, height, quality, cnt, &(cnt->current_image->timestamp_tv), &(cnt->current_image->location));
        } else {
            put_jpeg_grey_file(picture, image, width, height, quality, cnt, &(cnt->current_image->timestamp_tv), &(cnt->current_image->location));
       }
    }
}


void put_picture(struct context *cnt, char *file, unsigned char *image, int ftype)
{
    FILE *picture;

    picture = myfopen(file, "w");
    if (!picture) {
        /* Report to syslog - suggest solution if the problem is access rights to target dir. */
        if (errno ==  EACCES) {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                ,_("Can't write picture to file %s - check access rights to target directory\n"
                "Thread is going to finish due to this fatal error"), file);
            cnt->finish = 1;
            cnt->restart = 0;
            return;
        } else {
            /* If target dir is temporarily unavailable we may survive. */
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                ,_("Can't write picture to file %s"), file);
            return;
        }
    }

    put_picture_fd(cnt, picture, image, cnt->conf.picture_quality, ftype);

    myfclose(picture);
}

/**
 * get_pgm
 *      Get the pgm file used as fixed mask
 *
 */
unsigned char *get_pgm(FILE *picture, int width, int height)
{
    int x, y, mask_width, mask_height, maxval;
    char line[256];
    unsigned char *image, *resized_image;

    line[255] = 0;

    if (!fgets(line, 255, picture)) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO,_("Could not read from pgm file"));
        return NULL;
    }

    if (strncmp(line, "P5", 2)) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
            ,_("This is not a pgm file, starts with '%s'"), line);
        return NULL;
    }

    /* Skip comment */
    line[0] = '#';
    while (line[0] == '#')
        if (!fgets(line, 255, picture))
            return NULL;

    /* Read image size */
    if (sscanf(line, "%d %d", &mask_width, &mask_height) != 2) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
            ,_("Failed reading size in pgm file"));
        return NULL;
    }

    /* Maximum value */
    line[0] = '#';
    while (line[0] == '#')
        if (!fgets(line, 255, picture))
            return NULL;

    if (sscanf(line, "%d", &maxval) != 1) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
            ,_("Failed reading maximum value in pgm file"));
        return NULL;
    }

    /* Read data */
    /* We allocate the size for a 420P since we will use
    ** this image for masking privacy which needs the space for
    ** the cr / cb components
    */
    image = mymalloc((mask_width * mask_height * 3) / 2);

    for (y = 0; y < mask_height; y++) {
        if ((int)fread(&image[y * mask_width], 1, mask_width, picture) != mask_width)
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Failed reading image data from pgm file"));

        for (x = 0; x < mask_width; x++)
            image[y * mask_width + x] = (int)image[y * mask_width + x] * 255 / maxval;

    }

    /* Resize mask if required */
    if (mask_width != width || mask_height != height) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("The mask file specified is not the same size as image from camera."));
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Attempting to resize mask image from %dx%d to %dx%d")
            ,mask_width, mask_height, width, height);

        resized_image = mymalloc((width * height * 3) / 2);

        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                resized_image[y * width + x] = image[
                        (mask_height - 1) * y / (height - 1) * mask_width +
                        (mask_width  - 1) * x / (width  - 1)];
            }
        }

        free(image);
        image = resized_image;
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

    picture = myfopen(file, "w");
    if (!picture) {
        /* Report to syslog - suggest solution if the problem is access rights to target dir. */
        if (errno ==  EACCES) {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                ,_("can't write mask file %s - check access rights to target directory")
                ,file);
        } else {
            /* If target dir is temporarily unavailable we may survive. */
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                ,_("can't write mask file %s"), file);
        }
        return;
    }
    memset(cnt->imgs.img_motion.image_norm, 255, cnt->imgs.motionsize); /* Initialize to unset */

    /* Write pgm-header. */
    fprintf(picture, "P5\n");
    fprintf(picture, "%d %d\n", cnt->conf.width, cnt->conf.height);
    fprintf(picture, "%d\n", 255);

    /* Write pgm image data at once. */
    if ((int)fwrite(cnt->imgs.img_motion.image_norm, cnt->conf.width, cnt->conf.height, picture) != cnt->conf.height) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
            ,_("Failed writing default mask as pgm file"));
        return;
    }

    myfclose(picture);

    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
        ,_("Creating empty mask %s\nPlease edit this file and "
        "re-run motion to enable mask feature"), cnt->conf.mask_file);
}

void pic_scale_img(int width_src, int height_src, unsigned char *img_src, unsigned char *img_dst){

    int i = 0, x, y;
    for (y = 0; y < height_src; y+=2)
        for (x = 0; x < width_src; x+=2)
            img_dst[i++] = img_src[y * width_src + x];

    for (y = 0; y < height_src / 2; y+=2)
       for (x = 0; x < width_src; x += 4)
       {
          img_dst[i++] = img_src[(width_src * height_src) + (y * width_src) + x];
          img_dst[i++] = img_src[(width_src * height_src) + (y * width_src) + (x + 1)];
       }

    return;
}

