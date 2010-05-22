/*
 *    netcam_jpeg.c
 *
 *    Module for handling JPEG decompression for network cameras.
 *
 *    This code was inspired by the original module written by
 *    Jeroen Vreeken and enhanced by several Motion project
 *    contributors, particularly Angel Carpintero and
 *    Christopher Price.
 *
 *    Copyright 2005, William M. Brack
 *    This program is published under the GNU Public license
 */

#include "rotate.h"    /* already includes motion.h */
#include <jpeglib.h>
#include <jerror.h>

/*
 * netcam_source_mgr is a locally-defined structure to contain elements
 * which are not present in the standard libjpeg (the element 'pub' is a
 * pointer to the standard information).
 */
typedef struct {
    struct jpeg_source_mgr pub;
    char           *data;
    int             length;
    JOCTET         *buffer;
    boolean         start_of_file;
} netcam_source_mgr;

typedef netcam_source_mgr *netcam_src_ptr;


/*
 * Here we declare function prototypes for all the routines involved with
 * overriding libjpeg functions.  The reason these are required is that,
 * although the standard library handles input and output with stdio,
 * we are working with "remote" data (from the camera or from a file).
 */
static void     netcam_init_source(j_decompress_ptr);
static boolean  netcam_fill_input_buffer(j_decompress_ptr);
static void     netcam_skip_input_data(j_decompress_ptr, long);
static void     netcam_term_source(j_decompress_ptr);
static void     netcam_memory_src(j_decompress_ptr, char *, int);
static void     netcam_error_exit(j_common_ptr);

static void netcam_init_source(j_decompress_ptr cinfo)
{
    /* Get our "private" structure from the libjpeg structure. */
    netcam_src_ptr  src = (netcam_src_ptr) cinfo->src;
    /*
     * Set the 'start_of_file' flag in our private structure
     * (used by my_fill_input_buffer).
     */
    src->start_of_file = TRUE;
}

static boolean netcam_fill_input_buffer(j_decompress_ptr cinfo)
{
    netcam_src_ptr  src = (netcam_src_ptr) cinfo->src;
    size_t nbytes;

    /*
     * start_of_file is set any time netcam_init_source has been called
     * since the last entry to this routine.  This would be the normal
     * path when a new image is to be processed.  It is assumed that
     * this routine will only be called once for the entire image.
     * If an unexpected call (with start_of_file FALSE) occurs, the
     * routine will insert a "fake" "end of image" marker and return
     * to the library to process whatever data remains from the original
     * image (the one with errors).
     *
     * I'm not yet clear on what the result (from the application's
     * point of view) will be from this logic.  If the application
     * expects that a completely new image will be started, this will
     * give trouble.
     */
    if (src->start_of_file) {
        nbytes = src->length;
        src->buffer = (JOCTET *) src->data;
    } else {
        /* Insert a fake EOI marker - as per jpeglib recommendation */
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, "%s: **fake EOI inserted**");
        src->buffer[0] = (JOCTET) 0xFF;
        src->buffer[1] = (JOCTET) JPEG_EOI;    /* 0xD9 */
        nbytes = 2;
    }

    src->pub.next_input_byte = src->buffer;
    src->pub.bytes_in_buffer = nbytes;
    src->start_of_file = FALSE;

    return TRUE;
}

static void netcam_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    netcam_src_ptr  src = (netcam_src_ptr) cinfo->src;

    if (num_bytes > 0) {
        while (num_bytes > (long) src->pub.bytes_in_buffer) {
            num_bytes -= (long) src->pub.bytes_in_buffer;
            (void) netcam_fill_input_buffer (cinfo);
        }
        src->pub.next_input_byte += (size_t) num_bytes;
        src->pub.bytes_in_buffer -= (size_t) num_bytes;
    }
}

static void netcam_term_source(j_decompress_ptr cinfo ATTRIBUTE_UNUSED)
{
}

/**
 * netcam_memory_src
 *
 *    Routine to setup for fetching data from a netcam buffer, used by the
 *    JPEG library decompression routine.
 *
 * Parameters:
 *    cinfo           pointer to the jpeg decompression object.
 *    data            pointer to the image data received from a netcam.
 *    length          total size of the image.
 *
 * Returns:             Nothing
 *
 */
static void netcam_memory_src(j_decompress_ptr cinfo, char *data, int length)
{
    netcam_src_ptr src;

    if (cinfo->src == NULL)
        cinfo->src = (struct jpeg_source_mgr *)
                     (*cinfo->mem->alloc_small)
                     ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                      sizeof (netcam_source_mgr));


    src = (netcam_src_ptr)cinfo->src;
    src->data = data;
    src->length = length;
    src->pub.init_source = netcam_init_source;
    src->pub.fill_input_buffer = netcam_fill_input_buffer;
    src->pub.skip_input_data = netcam_skip_input_data;
    src->pub.resync_to_restart = jpeg_resync_to_restart;
    src->pub.term_source = netcam_term_source;
    src->pub.bytes_in_buffer = 0;
    src->pub.next_input_byte = NULL;
}

/**
 * netcam_error_exit
 *
 *     Routine to override the libjpeg error exit routine so
 *     that we can just throw away the bad frame and continue
 *     with more data from the netcam.
 *
 * Parameters
 *
 *     cinfo           pointer to the decompression control structure.
 *
 * Returns:             does an (ugly) longjmp to get back to netcam_jpeg
 *                      code.
 *
 */
static void netcam_error_exit(j_common_ptr cinfo)
{
    /* Fetch our pre-stored pointer to the netcam context. */
    netcam_context_ptr netcam = cinfo->client_data;
    /* Output the message associated with the error. */
    (*cinfo->err->output_message)(cinfo);
    /* Set flag to show the decompression had errors. */
    netcam->jpeg_error |= 1;
    /* Need to "cleanup" the aborted decompression. */
    jpeg_destroy (cinfo);

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, "%s: netcam->jpeg_error %d",
               netcam->jpeg_error);

    /* Jump back to wherever we started. */
    longjmp(netcam->setjmp_buffer, 1);
}

/**
 * netcam_output_message
 *
 *     Routine to override the libjpeg error message output routine.
 *     We do this so that we can output our module and thread i.d.,
 *     as well as put the message to the motion log.
 *
 * Parameters
 *
 *     cinfo           pointer to the decompression control structure.
 *
 * Returns              Nothing
 *
 */
static void netcam_output_message(j_common_ptr cinfo)
{
    char buffer[JMSG_LENGTH_MAX];

    /* Fetch our pre-stored pointer to the netcam context. */
    netcam_context_ptr netcam = cinfo->client_data;

    /*
     * While experimenting with a "appro" netcam it was discovered
     * that the jpeg data produced by the camera caused warning
     * messages from libjpeg (JWRN_EXTRANEOUS_DATA).  The following
     * code is to assure that specific warning is ignored.
     *
     * NOTE: It's likely that we will discover other error message
     * codes which we want to ignore.  In that case, we should have
     * some sort of table-lookup to decide which messages we really
     * care about.
     */
    if ((cinfo->err->msg_code != JWRN_EXTRANEOUS_DATA) &&
        (cinfo->err->msg_code == JWRN_NOT_SEQUENTIAL) && (!netcam->netcam_tolerant_check))
        netcam->jpeg_error |= 2;    /* Set flag to show problem */

    /*
     * Format the message according to library standards.
     * Write it out to the motion log.
     */
     (*cinfo->err->format_message)(cinfo, buffer);
     MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO, "%s: %s", buffer);

}

/**
 * netcam_init_jpeg
 *
 *     Initialises the JPEG library prior to doing a
 *     decompression.
 *
 * Parameters:
 *     netcam          pointer to netcam_context.
 *     cinfo           pointer to JPEG decompression context.
 *
 * Returns:           Error code.
 */
static int netcam_init_jpeg(netcam_context_ptr netcam, j_decompress_ptr cinfo)
{
    netcam_buff_ptr buff;

    /*
     * First we check whether a new image has arrived.  If not, we
     * setup to wait for 1/2 a frame time.  This will (hopefully)
     * help in synchronizing the camera frames with the motion main
     * loop.
     */
    pthread_mutex_lock(&netcam->mutex);

    if (netcam->imgcnt_last == netcam->imgcnt) {    /* Need to wait */
        struct timespec waittime;
        struct timeval curtime;
        int retcode;

        /*
         * We calculate the delay time (representing the desired frame
         * rate).  This delay time is in *nanoseconds*.
         * We will wait 0.5 seconds which gives a practical minimum
         * framerate of 2 which is desired for the motion_loop to
         * function.
         */
        gettimeofday(&curtime, NULL);
        curtime.tv_usec += 500000;

        if (curtime.tv_usec > 1000000) {
            curtime.tv_usec -= 1000000;
            curtime.tv_sec++;
        }

        waittime.tv_sec = curtime.tv_sec;
        waittime.tv_nsec = 1000L * curtime.tv_usec;

        do {
            retcode = pthread_cond_timedwait(&netcam->pic_ready,
                                             &netcam->mutex, &waittime);
        } while (retcode == EINTR);

        if (retcode) {    /* We assume a non-zero reply is ETIMEOUT */
            pthread_mutex_unlock(&netcam->mutex);

            MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO,
                       "%s: no new pic, no signal rcvd");

            return NETCAM_GENERAL_ERROR | NETCAM_NOTHING_NEW_ERROR;
        }

        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,
                   "%s: ***new pic delay successful***");
    }

    netcam->imgcnt_last = netcam->imgcnt;

    /* Set latest buffer as "current". */
    buff = netcam->latest;
    netcam->latest = netcam->jpegbuf;
    netcam->jpegbuf = buff;
    pthread_mutex_unlock(&netcam->mutex);

    /* Clear any error flag from previous work. */
    netcam->jpeg_error = 0;

    buff = netcam->jpegbuf;
    /*
     * Prepare for the decompression.
     * Initialize the JPEG decompression object.
     */
    jpeg_create_decompress(cinfo);

    /* Set up own error exit routine. */
    cinfo->err = jpeg_std_error(&netcam->jerr);
    cinfo->client_data = netcam;
    netcam->jerr.error_exit = netcam_error_exit;
    netcam->jerr.output_message = netcam_output_message;

    /* Specify the data source as our own routine. */
    netcam_memory_src(cinfo, buff->ptr, buff->used);

    /* Read file parameters (rejecting tables-only). */
    jpeg_read_header(cinfo, TRUE);

    /* Override the desired colour space. */
    cinfo->out_color_space = JCS_YCbCr;

    /* Start the decompressor. */
    jpeg_start_decompress(cinfo);

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, "%s: jpeg_error %d",
               netcam->jpeg_error);

    return netcam->jpeg_error;
}

/**
 * netcam_image_conv
 *
 * Parameters:
 *      netcam          pointer to netcam_context
 *      cinfo           pointer to JPEG decompression context
 *      image           pointer to buffer of destination image (yuv420)
 *
 * Returns :  netcam->jpeg_error
 */
static int netcam_image_conv(netcam_context_ptr netcam,
                               struct jpeg_decompress_struct *cinfo,
                               unsigned char *image)
{
    JSAMPARRAY      line;           /* Array of decomp data lines */
    unsigned char  *wline;          /* Will point to line[0] */
    /* Working variables */
    int             linesize, i;
    unsigned char  *upic, *vpic;
    unsigned char  *pic = image;
    unsigned char   y;              /* Switch for decoding YUV data */
    unsigned int    width, height;

    width = cinfo->output_width;
    height = cinfo->output_height;

    if (width && ((width != netcam->width) || (height != netcam->height))) {
        MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO,
                   "%s: JPEG image size %dx%d, JPEG was %dx%d",
                    netcam->width, netcam->height, width, height);
        jpeg_destroy_decompress(cinfo);
        netcam->jpeg_error |= 4;
        return netcam->jpeg_error;
    }
    /* Set the output pointers (these come from YUV411P definition. */
    upic = pic + width * height;
    vpic = upic + (width * height) / 4;


    /* YCbCr format will give us one byte each for YUV. */
    linesize = cinfo->output_width * 3;

    /* Allocate space for one line. */
    line = (cinfo->mem->alloc_sarray)((j_common_ptr) cinfo, JPOOL_IMAGE,
                                       cinfo->output_width * cinfo->output_components, 1);

    wline = line[0];
    y = 0;

    while (cinfo->output_scanline < height) {
        jpeg_read_scanlines(cinfo, line, 1);

        for (i = 0; i < linesize; i += 3) {
            pic[i / 3] = wline[i];
            if (i & 1) {
                upic[(i / 3) / 2] = wline[i + 1];
                vpic[(i / 3) / 2] = wline[i + 2];
            }
        }

        pic += linesize / 3;

        if (y++ & 1) {
            upic += width / 2;
            vpic += width / 2;
        }
    }

    jpeg_finish_decompress(cinfo);
    jpeg_destroy_decompress(cinfo);

    if (netcam->cnt->rotate_data.degrees > 0)
        /* Rotate as specified */
        rotate_map(netcam->cnt, image);

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, "%s: jpeg_error %d",
               netcam->jpeg_error);

    return netcam->jpeg_error;
}

/**
 * netcam_proc_jpeg
 *
 *    Routine to decode an image received from a netcam into a YUV420P buffer
 *    suitable for processing by motion.
 *
 * Parameters:
 *    netcam    pointer to the netcam_context structure.
 *     image    pointer to a buffer for the returned image.
 *
 * Returns:
 *
 *      0         Success
 *      non-zero  error code from other routines
 *                (e.g. netcam_init_jpeg or netcam_image_conv)
 *                or just NETCAM_GENERAL_ERROR
 */
int netcam_proc_jpeg(netcam_context_ptr netcam, unsigned char *image)
{
    struct jpeg_decompress_struct cinfo;    /* Decompression control struct. */
    int retval = 0;                         /* Value returned to caller. */
    int ret;                                /* Working var. */

    /*
     * This routine is only called from the main thread.
     * We need to "protect" the "latest" image while we
     * decompress it.  netcam_init_jpeg uses
     * netcam->mutex to do this.
     */
    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, "%s: processing jpeg image"
               " - content length %d", netcam->latest->content_length);

    ret = netcam_init_jpeg(netcam, &cinfo);

    if (ret != 0) {
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, "%s: ret %d", ret);
        return ret;
    }

    /*
     * Do a sanity check on dimensions
     * If dimensions have changed we throw an
     * error message that will cause
     * restart of Motion.
     */
    if (netcam->width) {    /* 0 means not yet init'ed */
        if ((cinfo.output_width != netcam->width) ||
            (cinfo.output_height != netcam->height)) {
            retval = NETCAM_RESTART_ERROR;
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Camera width/height mismatch "
                       "with JPEG image - expected %dx%d, JPEG %dx%d",
                       " retval %d", netcam->width, netcam->height,
                       cinfo.output_width, cinfo.output_height, retval);
            return retval;
        }
    }

    /* Do the conversion */
    ret = netcam_image_conv(netcam, &cinfo, image);

    if (ret != 0) {
        retval |= NETCAM_JPEG_CONV_ERROR;
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, "%s: ret %d retval %d",
                   ret, retval);
    }

    return retval;
}

/**
 * netcam_get_dimensions
 *
 *    This function gets the height and width of the JPEG image
 *    located in the supplied netcam_image_buffer.
 *
 * Parameters
 *
 *    netcam     pointer to the netcam context.
 *
 * Returns:   Nothing, but fills in width and height into context.
 *
 */
void netcam_get_dimensions(netcam_context_ptr netcam)
{
    struct jpeg_decompress_struct cinfo; /* Decompression control struct. */
    int ret;

    ret = netcam_init_jpeg(netcam, &cinfo);

    netcam->width = cinfo.output_width;
    netcam->height = cinfo.output_height;
    netcam->JFIF_marker = cinfo.saw_JFIF_marker;

    jpeg_destroy_decompress(&cinfo);

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, "%s: JFIF_marker %s PRESENT ret %d",
               netcam->JFIF_marker ? "IS" : "NOT", ret);
}
