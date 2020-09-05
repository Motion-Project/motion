/*
 *    This file is part of MotionPlus.
 *
 *    MotionPlus is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    MotionPlus is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with MotionPlus.  If not, see <https://www.gnu.org/licenses/>.
 *
 *    Copyright 2020 MotionMrDave@gmail.com
 */



#include "motionplus.hpp"
#include "conf.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "exif.hpp"
#include <jpeglib.h>

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

static unsigned const char exif_version_tag[12] = {
    0x90, 0x00,                 /* EXIF version tag, 0x9000 */
    0x00, 0x07,                 /* Data type 7 = "unknown" (raw byte blob) */
    0x00, 0x00, 0x00, 0x04,     /* Data length */
    0x30, 0x32, 0x32, 0x30      /* Inline data, EXIF version 2.2 */
};

static unsigned const char exif_subifd_tag[8] = {
    0x87, 0x69,                 /* EXIF Sub-IFD tag */
    0x00, 0x04,                 /* Data type 4 = uint32 */
    0x00, 0x00, 0x00, 0x01,     /* Number of values */
};

static unsigned const char exif_tzoffset_tag[12] = {
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

static void put_subjectarea(struct tiff_writing *into, const struct ctx_coord *box)
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
unsigned exif_prepare(unsigned char **exif,
              const struct ctx_cam *cam,
              const struct timespec *ts_in1,
              const struct ctx_coord *box)
{
    /* description, datetime, and subtime are the values that are actually
     * put into the EXIF data
    */
    char *description, *datetime, *subtime;
    char datetime_buf[22];
    char tmpbuf[45];
    struct tm timestamp_tm;
    struct timespec ts1;

    clock_gettime(CLOCK_REALTIME, &ts1);
    if (ts_in1 != NULL) {
        ts1.tv_sec = ts_in1->tv_sec;
        ts1.tv_nsec = ts_in1->tv_nsec;
    }

    localtime_r(&ts1.tv_sec, &timestamp_tm);
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

    if (cam->conf->picture_exif != "") {
        description =(char*) malloc(PATH_MAX);
        mystrftime(cam, description, PATH_MAX-1, cam->conf->picture_exif.c_str(), &ts1, NULL, 0);
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

    JOCTET *marker =(JOCTET *) malloc(buffer_size);
    memcpy(marker, exif_marker_start, 14); /* EXIF and TIFF headers */

    struct tiff_writing writing = (struct tiff_writing) {
    .base = marker + 6, /* base address for intra-TIFF offsets */
    .buf = marker + 14, /* current write position */
    .data_offset =(unsigned int) (8 + ifds_size), /* where to start storing data */
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


