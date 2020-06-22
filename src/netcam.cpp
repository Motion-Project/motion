/***********************************************************
 *
 *  The functions:
 *      netcam_setup
 *      netcam_next
 *      netcam_cleanup
 *  are called from video_common.c which is on the main thread
 *
 ***********************************************************/

#include <stdio.h>
#include <regex.h>
#include <time.h>
#include "motion.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "rotate.hpp"
#include "netcam.hpp"
#include "video_v4l2.hpp"  /* Needed to validate palette for v4l2 via netcam */
#include "movie.hpp"

static void netcam_check_buffsize(netcam_buff_ptr buff, size_t numbytes)
{
    int min_size_to_alloc;
    int real_alloc;
    int new_size;

    if ((buff->size - buff->used) >= numbytes)
        return;

    min_size_to_alloc = numbytes - (buff->size - buff->used);
    real_alloc = ((min_size_to_alloc / NETCAM_BUFFSIZE) * NETCAM_BUFFSIZE);

    if ((min_size_to_alloc - real_alloc) > 0)
        real_alloc += NETCAM_BUFFSIZE;

    new_size = buff->size + real_alloc;

    MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO
        ,_("expanding buffer from [%d/%d] to [%d/%d] bytes.")
        ,(int) buff->used, (int) buff->size
        ,(int) buff->used, new_size);

    buff->ptr =(char*) myrealloc(buff->ptr, new_size,
                          "netcam_check_buf_size");
    buff->size = new_size;
}

/*
 * The following three routines (netcam_url_match, netcam_url_parse and
 * netcam_url_free are for 'parsing' (i.e. separating into the relevant
 * components) the URL provided by the user.  They make use of regular
 * expressions (which is outside the scope of this module, so detailed
 * comments are not provided).  netcam_url_parse is called from netcam_start,
 * and puts the "broken-up" components of the URL into the "url" element of
 * the netcam_context structure.
 *
 * Note that the routines are not "very clever", but they work sufficiently
 * well for the limited requirements of this module.  The expression:
 *   (http)://(((.*):(.*))@)?([^/:]|[-.a-z0-9]+)(:([0-9]+))?($|(/[^:]*))
 * requires
 *   1) a string which begins with 'http', followed by '://'
 *   2) optionally a '@' which is preceded by two strings
 *      (with 0 or more characters each) separated by a ':'
 *      [this is for an optional username:password]
 *   3) a string comprising alpha-numerics, '-' and '.' characters
 *      [this is for the hostname]
 *   4) optionally a ':' followed by one or more numeric characters
 *      [this is for an optional port number]
 *   5) finally, either an end of line or a series of segments,
 *      each of which begins with a '/', and contains anything
 *      except a ':'
 */

/**
 * netcam_url_match
 *
 *      Finds the matched part of a regular expression
 *
 * Parameters:
 *
 *      m          A structure containing the regular expression to be used
 *      input      The input string
 *
 * Returns:        The string which was matched
 *
 */
static char *netcam_url_match(regmatch_t m, const char *input)
{
    char *match = NULL;
    int len;

    if (m.rm_so != -1) {
        len = m.rm_eo - m.rm_so;

        if ((match =(char*) mymalloc(len + 1)) != NULL) {
            strncpy(match, input + m.rm_so, len);
            match[len] = '\0';
        }
    }

    return match;
}

static void netcam_url_invalid(struct url_t *parse_url){

    MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Invalid URL.  Can not parse values."));

    parse_url->host =(char*) malloc(5);
    parse_url->service =(char*) malloc(5);
    parse_url->path =(char*) malloc(10);
    parse_url->userpass =(char*) malloc(10);
    parse_url->port = 0;
    sprintf(parse_url->host, "%s","????");
    sprintf(parse_url->service, "%s","????");
    sprintf(parse_url->path, "%s","INVALID");
    sprintf(parse_url->userpass, "%s","INVALID");

}
/**
 * netcam_url_parse
 *
 *      parses a string containing a URL into it's components
 *
 * Parameters:
 *      parse_url          A structure which will receive the results
 *                         of the parsing
 *      text_url           The input string containing the URL
 *
 * Returns:                Nothing
 *
 */
static void netcam_url_parse(struct url_t *parse_url, const char *text_url)
{
    char *s;
    int i;

    const char *re = "(http|ftp|mjpg|mjpeg|rtsp|rtmp)://(((.*):(.*))@)?"
                     "([^/:]|[-_.a-z0-9]+)(:([0-9]+))?($|(/[^*]*))";
    regex_t pattbuf;
    regmatch_t matches[10];

    if (!strncmp(text_url, "file", 4))
        re = "(file)://(((.*):(.*))@)?([/:])?(:([0-9]+))?($|(/[^*]*))";

    if (!strncmp(text_url, "jpeg", 4))
        re = "(jpeg)://(((.*):(.*))@)?([/:])?(:([0-9]+))?($|(/[^*]*))";

    if (!strncmp(text_url, "v4l2", 4))
        re = "(v4l2)://(((.*):(.*))@)?([/:])?(:([0-9]+))?($|(/[^*]*))";

    /*  Note that log messages are commented out to avoid leaking info related
     *  to user/host/pass etc.  Keeing them in the code for easier debugging if
     *  it is needed
     */

    //MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO, "Entry netcam_url_parse data %s",text_url);

    memset(parse_url, 0, sizeof(struct url_t));
    /*
     * regcomp compiles regular expressions into a form that is
     * suitable for regexec searches
     * regexec matches the URL string against the regular expression
     * and returns an array of pointers to strings matching each match
     * within (). The results that we need are finally placed in parse_url.
     */
    if (!regcomp(&pattbuf, re, REG_EXTENDED | REG_ICASE)) {
        if (regexec(&pattbuf, text_url, 10, matches, 0) != REG_NOMATCH) {
            for (i = 0; i < 10; i++) {
                if ((s = netcam_url_match(matches[i], text_url)) != NULL) {
                    //MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO, "Parse case %d data %s", i, s);
                    switch (i) {
                    case 1:
                        parse_url->service = s;
                        break;
                    case 3:
                        parse_url->userpass = s;
                        break;
                    case 6:
                        parse_url->host = s;
                        break;
                    case 8:
                        parse_url->port = atoi(s);
                        free(s);
                        break;
                    case 9:
                        parse_url->path = s;
                        break;
                        /* Other components ignored */
                    default:
                        free(s);
                        break;
                    }
                }
            }
        } else {
            netcam_url_invalid(parse_url);
        }
    } else {
        netcam_url_invalid(parse_url);
    }
    if (((!parse_url->port) && (parse_url->service)) ||
        ((parse_url->port > 65535) && (parse_url->service))) {
        if (mystreq(parse_url->service, "http"))
            parse_url->port = 80;
        else if (mystreq(parse_url->service, "ftp"))
            parse_url->port = 21;
        else if (mystreq(parse_url->service, "rtmp"))
            parse_url->port = 1935;
        else if (mystreq(parse_url->service, "rtsp"))
            parse_url->port = 554;
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Using port number %d"),parse_url->port);
    }

    regfree(&pattbuf);
}

/**
 * netcam_url_free
 *
 *      General cleanup of the URL structure, called from netcam_cleanup.
 *
 * Parameters:
 *
 *      parse_url       Structure containing the parsed data.
 *
 * Returns:             Nothing
 *
 */
static void netcam_url_free(struct url_t *parse_url)
{
    free(parse_url->service);
    parse_url->service = NULL;

    free(parse_url->userpass);
    parse_url->userpass = NULL;

    free(parse_url->host);
    parse_url->host = NULL;

    free(parse_url->path);
    parse_url->path = NULL;
}


static int netcam_check_pixfmt(struct ctx_netcam *netcam){
    /* Determine if the format is YUV420P */
    int retcd;

    retcd = -1;
    if ((netcam->codec_context->pix_fmt == MY_PIX_FMT_YUV420P) ||
        (netcam->codec_context->pix_fmt == MY_PIX_FMT_YUVJ420P)) retcd = 0;

    return retcd;

}

static void netcam_pktarray_free(struct ctx_netcam *netcam){

    int indx;
    pthread_mutex_lock(&netcam->mutex_pktarray);
        if (netcam->pktarray_size > 0){
            for(indx = 0; indx < netcam->pktarray_size; indx++) {
                if (netcam->pktarray[indx].packet.data != NULL) {
                    mypacket_unref(netcam->pktarray[indx].packet);
                }
            }
        }
        free(netcam->pktarray);
        netcam->pktarray = NULL;
        netcam->pktarray_size = 0;
        netcam->pktarray_index = -1;
    pthread_mutex_unlock(&netcam->mutex_pktarray);

}

static void netcam_null_context(struct ctx_netcam *netcam){

    netcam->swsctx          = NULL;
    netcam->swsframe_in     = NULL;
    netcam->swsframe_out    = NULL;
    netcam->frame           = NULL;
    netcam->codec_context   = NULL;
    netcam->format_context  = NULL;
    netcam->transfer_format = NULL;

}

static void netcam_close_context(struct ctx_netcam *netcam){

    if (netcam->swsctx       != NULL) sws_freeContext(netcam->swsctx);
    if (netcam->swsframe_in  != NULL) myframe_free(netcam->swsframe_in);
    if (netcam->swsframe_out != NULL) myframe_free(netcam->swsframe_out);
    if (netcam->frame        != NULL) myframe_free(netcam->frame);
    if (netcam->pktarray     != NULL) netcam_pktarray_free(netcam);
    if (netcam->codec_context    != NULL) myavcodec_close(netcam->codec_context);
    if (netcam->format_context   != NULL) avformat_close_input(&netcam->format_context);
    if (netcam->transfer_format != NULL) avformat_close_input(&netcam->transfer_format);
    netcam_null_context(netcam);

}

static void netcam_pktarray_resize(struct ctx_cam *cam, int is_highres){
    /* This is called from netcam_next and is on the motion loop thread
     * The netcam->mutex is locked around the call to this function.
    */

    /* Remember that this is a ring and we have two threads chasing around it
     * the ffmpeg is writing out of this ring while we are filling it up.  "Bad"
     * things will occur if the "add" thread catches up with the "write" thread.
     * We need this ring to be big enough so they don't collide.
     * The alternative is that we'd need to make a copy of the entire packet
     * array in the ffmpeg module and do our writing from that copy.  The
     * downside is that is a lot to be copying around for each image we want
     * to write out.  And putting a mutex on the array during adding function would
     * slow down the capture thread to the speed of the writing thread.  And that
     * writing thread operates at the user specified FPS which could be really slow
     * ...So....make this array big enough so we never catch our tail.  :)
     */

    int64_t               idnbr_last, idnbr_first;
    int                   indx;
    struct ctx_netcam  *netcam;
    struct packet_item   *tmp;
    int                   newsize;

    if (is_highres){
        idnbr_last = cam->imgs.image_ring[cam->imgs.ring_out].idnbr_high;
        idnbr_first = cam->imgs.image_ring[cam->imgs.ring_in].idnbr_high;
        netcam = cam->netcam_high;
    } else {
        idnbr_last = cam->imgs.image_ring[cam->imgs.ring_out].idnbr_norm;
        idnbr_first = cam->imgs.image_ring[cam->imgs.ring_in].idnbr_norm;
        netcam = cam->netcam;
    }

    if (!netcam->passthrough) return;

    /* The 30 is arbitrary */
    /* Double the size plus double last diff so we don't catch our tail */
    newsize =((idnbr_first - idnbr_last) * 2 ) + ((netcam->idnbr - idnbr_last ) * 2);
    if (newsize < 30) newsize = 30;

    pthread_mutex_lock(&netcam->mutex_pktarray);
        if ((netcam->pktarray_size < newsize) ||  (netcam->pktarray_size < 30)){
            tmp =(packet_item*) mymalloc(newsize * sizeof(struct packet_item));
            if (netcam->pktarray_size > 0 ){
                memcpy(tmp, netcam->pktarray, sizeof(struct packet_item) * netcam->pktarray_size);
            }
            for(indx = netcam->pktarray_size; indx < newsize; indx++) {
                av_init_packet(&tmp[indx].packet);
                tmp[indx].packet.data=NULL;
                tmp[indx].packet.size=0;
                tmp[indx].idnbr = 0;
                tmp[indx].iskey = FALSE;
                tmp[indx].iswritten = FALSE;
            }

            if (netcam->pktarray != NULL) free(netcam->pktarray);
            netcam->pktarray = tmp;
            netcam->pktarray_size = newsize;

            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Resized packet array to %d"), netcam->cameratype,newsize);
        }
    pthread_mutex_unlock(&netcam->mutex_pktarray);

}

static void netcam_pktarray_add(struct ctx_netcam *netcam){

    int indx_next;
    int retcd;
    char errstr[128];

    pthread_mutex_lock(&netcam->mutex_pktarray);

        if (netcam->pktarray_size == 0){
            pthread_mutex_unlock(&netcam->mutex_pktarray);
            return;
        }

        /* Recall pktarray_size is one based but pktarray is zero based */
        if (netcam->pktarray_index == (netcam->pktarray_size-1) ){
            indx_next = 0;
        } else {
            indx_next = netcam->pktarray_index + 1;
        }

        netcam->pktarray[indx_next].idnbr = netcam->idnbr;

        mypacket_unref(netcam->pktarray[indx_next].packet);
        av_init_packet(&netcam->pktarray[indx_next].packet);
        netcam->pktarray[indx_next].packet.data = NULL;
        netcam->pktarray[indx_next].packet.size = 0;

        retcd = mycopy_packet(&netcam->pktarray[indx_next].packet, &netcam->packet_recv);
        if ((netcam->interrupted) || (retcd < 0)) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: av_copy_packet: %s ,Interrupt: %s")
                ,netcam->cameratype
                ,errstr, netcam->interrupted ? _("True"):_("False"));
            mypacket_unref(netcam->pktarray[indx_next].packet);
            netcam->pktarray[indx_next].packet.data = NULL;
            netcam->pktarray[indx_next].packet.size = 0;
        }

        if (netcam->pktarray[indx_next].packet.flags & AV_PKT_FLAG_KEY) {
            netcam->pktarray[indx_next].iskey = TRUE;
        } else {
            netcam->pktarray[indx_next].iskey = FALSE;
        }
        netcam->pktarray[indx_next].iswritten = FALSE;
        netcam->pktarray[indx_next].timestamp_ts.tv_sec = netcam->img_recv->image_time.tv_sec;
        netcam->pktarray[indx_next].timestamp_ts.tv_nsec = netcam->img_recv->image_time.tv_nsec;
        netcam->pktarray_index = indx_next;
    pthread_mutex_unlock(&netcam->mutex_pktarray);

}


/* netcam_decode_video
 *
 * Return values:
 *   <0 error
 *   0 invalid but continue
 *   1 valid data
 */
static int netcam_decode_video(struct ctx_netcam *netcam){

    #if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))

        int retcd;
        char errstr[128];

        /* The Invalid data problem comes frequently.  Usually at startup of rtsp cameras.
        * We now ignore those packets so this function would need to fail on a different error.
        * We should consider adding a maximum count of these errors and reset every time
        * we get a good image.
        */
        if (netcam->finish) return 0;   /* This just speeds up the shutdown time */

        retcd = avcodec_send_packet(netcam->codec_context, &netcam->packet_recv);
        if ((netcam->interrupted) || (netcam->finish)) return -1;
        if (retcd == AVERROR_INVALIDDATA) {
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("Ignoring packet with invalid data"));
            return 0;
        }
        if (retcd < 0 && retcd != AVERROR_EOF){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("Error sending packet to codec: %s"), errstr);
            return -1;
        }

        retcd = avcodec_receive_frame(netcam->codec_context, netcam->frame);
        if ((netcam->interrupted) || (netcam->finish)) return -1;

        if (retcd == AVERROR(EAGAIN)) return 0;

        if (retcd == AVERROR_INVALIDDATA) {
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("Ignoring packet with invalid data"));
            return 0;
        }

        if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("Error receiving frame from codec: %s"), errstr);
            return -1;
        }

        return 1;

    #else

        int retcd;
        int check = 0;
        char errstr[128];

        if (netcam->finish) return 0;   /* This just speeds up the shutdown time */

        retcd = avcodec_decode_video2(netcam->codec_context, netcam->frame, &check, &netcam->packet_recv);
        if ((netcam->interrupted) || (netcam->finish)) return -1;

        if (retcd == AVERROR_INVALIDDATA) {
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Ignoring packet with invalid data"));
            return 0;
        }

        if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Error decoding packet: %s"),errstr);
            return -1;
        }

        if (check == 0 || retcd == 0) return 0;

        return 1;

    #endif

}

static int netcam_decode_packet(struct ctx_netcam *netcam){

    int frame_size;
    int retcd;

    if (netcam->finish) return -1;   /* This just speeds up the shutdown time */

    retcd = netcam_decode_video(netcam);
    if (retcd <= 0) return retcd;

    frame_size = myimage_get_buffer_size(netcam->codec_context->pix_fmt
                                          ,netcam->codec_context->width
                                          ,netcam->codec_context->height);

    netcam_check_buffsize(netcam->img_recv, frame_size);
    netcam_check_buffsize(netcam->img_latest, frame_size);

    retcd = myimage_copy_to_buffer(netcam->frame
                                    ,(uint8_t *)netcam->img_recv->ptr
                                    ,netcam->codec_context->pix_fmt
                                    ,netcam->codec_context->width
                                    ,netcam->codec_context->height
                                    ,frame_size);
    if ((retcd < 0) || (netcam->interrupted)) {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("Error decoding video packet: Copying to buffer"));
        return -1;
    }

    netcam->img_recv->used = frame_size;

    return frame_size;
}

static int netcam_open_codec(struct ctx_netcam *netcam){

    #if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
        int retcd;
        char errstr[128];
        AVStream *st;
        AVCodec *decoder = NULL;

        if (netcam->finish) return -1;   /* This just speeds up the shutdown time */

        retcd = av_find_best_stream(netcam->format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if ((retcd < 0) || (netcam->interrupted)){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: av_find_best_stream: %s,Interrupt %s")
                ,netcam->cameratype, errstr, netcam->interrupted ? _("True"):_("False"));
            return -1;
        }
        netcam->video_stream_index = retcd;
        st = netcam->format_context->streams[netcam->video_stream_index];

        decoder = avcodec_find_decoder(st->codecpar->codec_id);
        if ((decoder == NULL) || (netcam->interrupted)){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: avcodec_find_decoder: Failed,Interrupt %s")
                ,netcam->cameratype, netcam->interrupted ? _("True"):_("False"));
            return -1;
        }

        netcam->codec_context = avcodec_alloc_context3(decoder);
        if ((netcam->codec_context == NULL) || (netcam->interrupted)){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: avcodec_alloc_context3: Failed,Interrupt %s")
                ,netcam->cameratype, netcam->interrupted ? _("True"):_("False"));
            return -1;
        }

        retcd = avcodec_parameters_to_context(netcam->codec_context, st->codecpar);
        if ((retcd < 0) || (netcam->interrupted)) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: avcodec_parameters_to_context: %s,Interrupt %s")
                ,netcam->cameratype, errstr, netcam->interrupted ? _("True"):_("False"));
            return -1;
        }

        netcam->codec_context->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
        netcam->codec_context->err_recognition = AV_EF_EXPLODE;

        retcd = avcodec_open2(netcam->codec_context, decoder, NULL);
        if ((retcd < 0) || (netcam->interrupted)){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: avcodec_open2: %s,Interrupt %s")
                ,netcam->cameratype, errstr, netcam->interrupted ? _("True"):_("False"));
            return -1;
        }

        return 0;
    #else

        int retcd;
        char errstr[128];
        AVStream *st;
        AVCodec *decoder = NULL;

        if (netcam->finish) return -1;   /* This just speeds up the shutdown time */

        retcd = av_find_best_stream(netcam->format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if ((retcd < 0) || (netcam->interrupted)){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: av_find_best_stream: %s,Interrupt %s")
                ,netcam->cameratype, errstr, netcam->interrupted ? _("True"):_("False"));
            return -1;
        }
        netcam->video_stream_index = retcd;
        st = netcam->format_context->streams[netcam->video_stream_index];

        netcam->codec_context = st->codec;
        decoder = avcodec_find_decoder(netcam->codec_context->codec_id);
        if ((decoder == NULL) || (netcam->interrupted)) {
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: avcodec_find_decoder: Failed,Interrupt %s")
                ,netcam->cameratype, netcam->interrupted ? _("True"):_("False"));
            return -1;
        }
        retcd = avcodec_open2(netcam->codec_context, decoder, NULL);
        if ((retcd < 0) || (netcam->interrupted)){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: avcodec_open2: %s,Interrupt %s")
                ,netcam->cameratype, errstr, netcam->interrupted ? _("True"):_("False"));
            return -1;
        }

        return 0;
    #endif


}

static struct ctx_netcam *netcam_new_context(void){
    struct ctx_netcam *ret;

    /* Note that mymalloc will exit on any problem. */
    ret =(struct ctx_netcam*) mymalloc(sizeof(struct ctx_netcam));

    memset(ret, 0, sizeof(struct ctx_netcam));

    return ret;
}

static int netcam_interrupt(void *ctx){
    struct ctx_netcam *netcam = (struct ctx_netcam *)ctx;

    if (netcam->finish){
        netcam->interrupted = TRUE;
        return TRUE;
    }

    if (netcam->status == NETCAM_CONNECTED) {
        return FALSE;
    } else if (netcam->status == NETCAM_READINGIMAGE) {
        clock_gettime(CLOCK_REALTIME, &netcam->interruptcurrenttime);
        if ((netcam->interruptcurrenttime.tv_sec - netcam->interruptstarttime.tv_sec ) > netcam->interruptduration){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Camera reading (%s) timed out")
                , netcam->cameratype, netcam->camera_name);
            netcam->interrupted = TRUE;
            return TRUE;
        } else{
            return FALSE;
        }
    } else {
        /* This is for NOTCONNECTED and RECONNECTING status.  We give these
         * options more time because all the ffmpeg calls that are inside the
         * netcam_connect function will use the same start time.  Otherwise we
         * would need to reset the time before each call to a ffmpeg function.
        */
        clock_gettime(CLOCK_REALTIME, &netcam->interruptcurrenttime);
        if ((netcam->interruptcurrenttime.tv_sec - netcam->interruptstarttime.tv_sec ) > netcam->interruptduration){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Camera (%s) timed out")
                , netcam->cameratype, netcam->camera_name);
            netcam->interrupted = TRUE;
            return TRUE;
        } else{
            return FALSE;
        }
    }

    /* should not be possible to get here */
    return FALSE;
}

static int netcam_resize(struct ctx_netcam *netcam){

    int      retcd;
    char     errstr[128];
    uint8_t *buffer_out;

    if (netcam->finish) return -1;   /* This just speeds up the shutdown time */

    retcd=myimage_fill_arrays(
        netcam->swsframe_in
        ,(uint8_t*)netcam->img_recv->ptr
        ,netcam->codec_context->pix_fmt
        ,netcam->codec_context->width
        ,netcam->codec_context->height);
    if (retcd < 0) {
        if (netcam->status == NETCAM_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("Error allocating picture in: %s"), errstr);
        }
        netcam_close_context(netcam);
        return -1;
    }

    buffer_out=(uint8_t *)av_malloc(netcam->swsframe_size*sizeof(uint8_t));

    retcd=myimage_fill_arrays(
        netcam->swsframe_out
        ,buffer_out
        ,MY_PIX_FMT_YUV420P
        ,netcam->imgsize.width
        ,netcam->imgsize.height);
    if (retcd < 0) {
        if (netcam->status == NETCAM_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("Error allocating picture out: %s"), errstr);
        }
        netcam_close_context(netcam);
        return -1;
    }

    retcd = sws_scale(
        netcam->swsctx
        ,(const uint8_t* const *)netcam->swsframe_in->data
        ,netcam->swsframe_in->linesize
        ,0
        ,netcam->codec_context->height
        ,netcam->swsframe_out->data
        ,netcam->swsframe_out->linesize);
    if (retcd < 0) {
        if (netcam->status == NETCAM_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("Error resizing/reformatting: %s"), errstr);
        }
        netcam_close_context(netcam);
        return -1;
    }

    retcd=myimage_copy_to_buffer(
         netcam->swsframe_out
        ,(uint8_t *)netcam->img_recv->ptr
        ,MY_PIX_FMT_YUV420P
        ,netcam->imgsize.width
        ,netcam->imgsize.height
        ,netcam->swsframe_size);
    if (retcd < 0) {
        if (netcam->status == NETCAM_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("Error putting frame into output buffer: %s"), errstr);
        }
        netcam_close_context(netcam);
        return -1;
    }
    netcam->img_recv->used = netcam->swsframe_size;

    av_free(buffer_out);

    return 0;

}

static int netcam_read_image(struct ctx_netcam *netcam){

    int  size_decoded;
    int  retcd;
    int  haveimage;
    char errstr[128];
    netcam_buff *xchg;

    if (netcam->finish) return -1;   /* This just speeds up the shutdown time */

    av_init_packet(&netcam->packet_recv);
    netcam->packet_recv.data = NULL;
    netcam->packet_recv.size = 0;

    netcam->interrupted=FALSE;
    clock_gettime(CLOCK_REALTIME, &netcam->interruptstarttime);
    netcam->interruptduration = 10;

    netcam->status = NETCAM_READINGIMAGE;
    netcam->img_recv->used = 0;
    size_decoded = 0;
    haveimage = FALSE;

    while ((!haveimage) && (!netcam->interrupted)) {
        retcd = av_read_frame(netcam->format_context, &netcam->packet_recv);
        if ((netcam->interrupted) || (retcd < 0)) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: av_read_frame: %s ,Interrupt: %s")
                ,netcam->cameratype
                ,errstr, netcam->interrupted ? _("True"):_("False"));
            mypacket_unref(netcam->packet_recv);
            netcam_close_context(netcam);
            return -1;
        }

        if (netcam->packet_recv.stream_index == netcam->video_stream_index){
            /* For a high resolution pass-through we don't decode the image */
            if (netcam->high_resolution && netcam->passthrough){
                if (netcam->packet_recv.data != NULL) size_decoded = 1;
            } else {
                size_decoded = netcam_decode_packet(netcam);
            }
        }

        if (size_decoded > 0 ){
            haveimage = TRUE;
        } else if (size_decoded == 0){
            /* Did not fail, just didn't get anything.  Try again */
            mypacket_unref(netcam->packet_recv);
            av_init_packet(&netcam->packet_recv);
            netcam->packet_recv.data = NULL;
            netcam->packet_recv.size = 0;
        } else {
            mypacket_unref(netcam->packet_recv);
            netcam_close_context(netcam);
            return -1;
        }
    }
    clock_gettime(CLOCK_REALTIME, &netcam->img_recv->image_time);
    /* Skip status change on our first image to keep the "next" function waiting
     * until the handler thread gets going
     */
    if (!netcam->first_image) netcam->status = NETCAM_CONNECTED;

    /* Skip resize/pix format for high pass-through */
    if (!(netcam->high_resolution && netcam->passthrough)){
        if ((netcam->imgsize.width  != netcam->codec_context->width) ||
            (netcam->imgsize.height != netcam->codec_context->height) ||
            (netcam_check_pixfmt(netcam) != 0) ){
            if (netcam_resize(netcam) < 0){
                mypacket_unref(netcam->packet_recv);
                netcam_close_context(netcam);
                return -1;
            }
        }
    }

    pthread_mutex_lock(&netcam->mutex);
        netcam->idnbr++;
        if (netcam->passthrough) netcam_pktarray_add(netcam);
        if (!(netcam->high_resolution && netcam->passthrough)) {
            xchg = netcam->img_latest;
            netcam->img_latest = netcam->img_recv;
            netcam->img_recv = xchg;
        }
    pthread_mutex_unlock(&netcam->mutex);

    mypacket_unref(netcam->packet_recv);

    if (netcam->format_context->streams[netcam->video_stream_index]->avg_frame_rate.den > 0){
        netcam->src_fps = (
            (netcam->format_context->streams[netcam->video_stream_index]->avg_frame_rate.num /
            netcam->format_context->streams[netcam->video_stream_index]->avg_frame_rate.den) +
            0.5);
    }

    return 0;
}

static int netcam_ntc(struct ctx_netcam *netcam){

    if ((netcam->finish) || (!netcam->first_image)) return 0;

    if ((netcam->imgsize.width  != netcam->codec_context->width) ||
        (netcam->imgsize.height != netcam->codec_context->height) ||
        (netcam_check_pixfmt(netcam) != 0) ){
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "");
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "******************************************************");
        if ((netcam->imgsize.width  != netcam->codec_context->width) ||
            (netcam->imgsize.height != netcam->codec_context->height)) {
            if (netcam_check_pixfmt(netcam) != 0) {
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("The network camera is sending pictures in a different"));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("size than specified in the config and also a "));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("different picture format.  The picture is being"));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("transcoded to YUV420P and into the size requested"));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("in the config file.  If possible change netcam to"));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("be in YUV420P format and the size requested in the"));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("config to possibly lower CPU usage."));
            } else {
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("The network camera is sending pictures in a different"));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("size than specified in the configuration file."));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("The picture is being transcoded into the size "));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("requested in the configuration.  If possible change"));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("netcam or configuration to indicate the same size"));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("to possibly lower CPU usage."));
            }
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("Netcam: %d x %d => Config: %d x %d")
            ,netcam->codec_context->width,netcam->codec_context->height
            ,netcam->imgsize.width,netcam->imgsize.height);
        } else {
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("The image sent is being "));
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("trancoded to YUV420P.  If possible change netcam "));
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("picture format to YUV420P to possibly lower CPU usage."));
        }
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "******************************************************");
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "");
    }

    return 0;

}

static int netcam_open_sws(struct ctx_netcam *netcam){

    if (netcam->finish) return -1;   /* This just speeds up the shutdown time */

    netcam->swsframe_in = myframe_alloc();
    if (netcam->swsframe_in == NULL) {
        if (netcam->status == NETCAM_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("Unable to allocate swsframe_in."));
        }
        netcam_close_context(netcam);
        return -1;
    }

    netcam->swsframe_out = myframe_alloc();
    if (netcam->swsframe_out == NULL) {
        if (netcam->status == NETCAM_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("Unable to allocate swsframe_out."));
        }
        netcam_close_context(netcam);
        return -1;
    }

    /*
     *  The scaling context is used to change dimensions to config file and
     *  also if the format sent by the camera is not YUV420.
     */
    netcam->swsctx = sws_getContext(
         netcam->codec_context->width
        ,netcam->codec_context->height
        ,netcam->codec_context->pix_fmt
        ,netcam->imgsize.width
        ,netcam->imgsize.height
        ,MY_PIX_FMT_YUV420P
        ,SWS_BICUBIC,NULL,NULL,NULL);
    if (netcam->swsctx == NULL) {
        if (netcam->status == NETCAM_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("Unable to allocate scaling context."));
        }
        netcam_close_context(netcam);
        return -1;
    }

    netcam->swsframe_size = myimage_get_buffer_size(
            MY_PIX_FMT_YUV420P
            ,netcam->imgsize.width
            ,netcam->imgsize.height);
    if (netcam->swsframe_size <= 0) {
        if (netcam->status == NETCAM_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("Error determining size of frame out"));
        }
        netcam_close_context(netcam);
        return -1;
    }

    /* the image buffers must be big enough to hold the final frame after resizing */
    netcam_check_buffsize(netcam->img_recv, netcam->swsframe_size);
    netcam_check_buffsize(netcam->img_latest, netcam->swsframe_size);

    return 0;

}

static void netcam_set_http(struct ctx_netcam *netcam){

    netcam->format_context->iformat = av_find_input_format("mjpeg");
    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Setting http input_format mjpeg"),netcam->cameratype);

}

static void netcam_set_rtsp(struct ctx_netcam *netcam){

    if (netcam->rtsp_uses_tcp) {
        av_dict_set(&netcam->opts, "rtsp_transport", "tcp", 0);
        av_dict_set(&netcam->opts, "allowed_media_types", "video", 0);
        if (netcam->status == NETCAM_NOTCONNECTED)
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Setting rtsp transport to tcp"),netcam->cameratype);
    } else {
        av_dict_set(&netcam->opts, "rtsp_transport", "udp", 0);
        av_dict_set(&netcam->opts, "max_delay", "500000", 0);
        av_dict_set(&netcam->opts, "buffer_size", "2560000", 0);
        av_dict_set(&netcam->opts, "fifo_size", "1000000", 0);
        if (netcam->status == NETCAM_NOTCONNECTED)
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Setting rtsp transport to udp"),netcam->cameratype);
    }
}

static void netcam_set_file(struct ctx_netcam *netcam){

    /* This is a place holder for the moment.  We will add into
     * this function any options that must be set for ffmpeg to
     * read a particular file.  To date, it does not need any
     * additional options and works fine with defaults.
     */
    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Setting attributes to read file"),netcam->cameratype);

}

static void netcam_set_v4l2(struct ctx_netcam *netcam){

    char optsize[10], optfmt[10], optfps[10];
    char *fourcc;

    netcam->format_context->iformat = av_find_input_format("video4linux2");

    fourcc=(char*)malloc(5*sizeof(char));

    v4l2_palette_fourcc(netcam->v4l2_palette, fourcc);

    if (mystreq(fourcc,"MJPG")) {
        if (v4l2_palette_valid(netcam->path,netcam->v4l2_palette)){
            sprintf(optfmt, "%s","mjpeg");
            av_dict_set(&netcam->opts, "input_format", optfmt, 0);
        } else {
            sprintf(optfmt, "%s","default");
        }
    } else if (mystreq(fourcc,"H264")){
        if (v4l2_palette_valid(netcam->path,netcam->v4l2_palette)){
            sprintf(optfmt, "%s","h264");
            av_dict_set(&netcam->opts, "input_format", optfmt, 0);
        } else {
            sprintf(optfmt, "%s","default");
        }
    } else {
        sprintf(optfmt, "%s","default");
    }

    if (mystrne(optfmt,"default")) {
        if (v4l2_parms_valid(netcam->path
                             ,netcam->v4l2_palette
                             ,netcam->framerate
                             ,netcam->imgsize.width
                             ,netcam->imgsize.height)) {
            sprintf(optfps, "%d",netcam->framerate);
            av_dict_set(&netcam->opts, "framerate", optfps, 0);

            sprintf(optsize, "%dx%d",netcam->imgsize.width,netcam->imgsize.height);
            av_dict_set(&netcam->opts, "video_size", optsize, 0);
        } else {
            sprintf(optfps, "%s","default");
            sprintf(optsize, "%s","default");
        }
    } else {
        sprintf(optfps, "%s","default");
        sprintf(optsize, "%s","default");
    }

    if (netcam->status == NETCAM_NOTCONNECTED){
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Requested v4l2_palette option: %d")
            ,netcam->cameratype,netcam->v4l2_palette);
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Requested FOURCC code: %s"),netcam->cameratype,fourcc);
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Setting v4l2 input_format: %s"),netcam->cameratype,optfmt);
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Setting v4l2 framerate: %s"),netcam->cameratype, optfps);
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Setting v4l2 video_size: %s"),netcam->cameratype, optsize);
    }

    free(fourcc);

}

static void netcam_set_path (struct ctx_cam *cam, struct ctx_netcam *netcam ) {

    char   userpass[PATH_MAX];
    struct url_t url;
    int retcd;

    netcam->path = NULL;

    memset(&url, 0, sizeof(url));
    memset(userpass,0,PATH_MAX);

    if (netcam->high_resolution){
        netcam_url_parse(&url, cam->conf->netcam_highres.c_str());
    } else {
        netcam_url_parse(&url, cam->conf->netcam_url.c_str());
    }

    if (cam->conf->netcam_userpass != "") {
        cam->conf->netcam_userpass.copy(userpass, PATH_MAX);
    } else if (url.userpass != NULL) {
        retcd = snprintf(userpass,PATH_MAX,"%s",url.userpass);
        if ((retcd <0) || (retcd>=PATH_MAX)){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("Error getting userpass"));
        }
    }

    if (mystreq(url.service, "v4l2")) {
        netcam->path =(char*) mymalloc(strlen(url.path) + 1);
        sprintf(netcam->path, "%s",url.path);
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("Setting up v4l2 via ffmpeg netcam"));
    } else if (mystreq(url.service, "file")) {
        netcam->path =(char*) mymalloc(strlen(url.path) + 1);
        sprintf(netcam->path, "%s",url.path);
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("Setting up file via ffmpeg netcam"));
    } else {
        if (mystreq(url.service, "mjpeg")) {
            sprintf(url.service, "%s","http");
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("Setting up http via ffmpeg netcam"));
        } else {
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("Setting up %s via ffmpeg netcam"),url.service);
        }
        if (userpass != NULL) {
            netcam->path =(char*) mymalloc(strlen(url.service) + 3 + strlen(userpass)
                  + 1 + strlen(url.host) + 6 + strlen(url.path) + 2 );
            sprintf((char *)netcam->path, "%s://%s@%s:%d%s",
                    url.service, userpass, url.host, url.port, url.path);
        } else {
            netcam->path =(char*) mymalloc(strlen(url.service) + 3 + strlen(url.host)
                  + 6 + strlen(url.path) + 2);
            sprintf((char *)netcam->path, "%s://%s:%d%s", url.service,
                url.host, url.port, url.path);
        }
    }

    sprintf(netcam->service, "%s",url.service);

    netcam_url_free(&url);

}

static void netcam_set_parms (struct ctx_cam *cam, struct ctx_netcam *netcam ) {
    /* Set the parameters to be used with our camera */
    int retcd;

    if (netcam->high_resolution) {
        netcam->imgsize.width = 0;
        netcam->imgsize.height = 0;
        retcd = snprintf(netcam->cameratype,29, "%s",_("High resolution"));
    } else {
        netcam->imgsize.width = cam->conf->width;
        netcam->imgsize.height = cam->conf->height;
        retcd = snprintf(netcam->cameratype,29, "%s",_("Normal resolution"));
    }
    if ((retcd <0) || (retcd >= 30)){
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("Error setting name of resolution %s."),netcam->cameratype);
    }

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("Setting up %s stream."),netcam->cameratype);

    mycheck_passthrough(cam); /* In case it was turned on via webcontrol */
    netcam->status = NETCAM_NOTCONNECTED;
    netcam->rtsp_uses_tcp =cam->conf->netcam_use_tcp;
    netcam->v4l2_palette = cam->conf->v4l2_palette;
    netcam->framerate = cam->conf->framerate;
    netcam->src_fps =  cam->conf->framerate; /* Default to conf fps */
    netcam->motapp = cam->motapp;
    netcam->conf = cam->conf;
    netcam->img_recv =(netcam_buff_ptr) mymalloc(sizeof(netcam_buff));
    netcam->img_recv->ptr =(char*) mymalloc(NETCAM_BUFFSIZE);
    netcam->img_latest =(netcam_buff_ptr) mymalloc(sizeof(netcam_buff));
    netcam->img_latest->ptr =(char*) mymalloc(NETCAM_BUFFSIZE);
    netcam->pktarray_size = 0;
    netcam->pktarray_index = -1;
    netcam->pktarray = NULL;
    netcam->handler_finished = TRUE;
    netcam->first_image = TRUE;
    netcam->reconnect_count = 0;
    cam->conf->camera_name.copy(netcam->camera_name,PATH_MAX);

    snprintf(netcam->threadname, 15, "%s",_("Unknown"));

    clock_gettime(CLOCK_REALTIME, &netcam->interruptstarttime);
    clock_gettime(CLOCK_REALTIME, &netcam->interruptcurrenttime);

    /* If this is the norm and we have a highres, then disable passthru on the norm */
    if ((!netcam->high_resolution) &&
        (cam->conf->netcam_highres != "")) {
        netcam->passthrough = FALSE;
    } else {
        netcam->passthrough = mycheck_passthrough(cam);
    }
    netcam->interruptduration = 5;
    netcam->interrupted = FALSE;

    clock_gettime(CLOCK_REALTIME, &netcam->frame_curr_tm);
    clock_gettime(CLOCK_REALTIME, &netcam->frame_prev_tm);

    netcam_set_path(cam, netcam);

}

static int netcam_set_dimensions (struct ctx_cam *cam) {

    cam->imgs.width = 0;
    cam->imgs.height = 0;
    cam->imgs.size_norm = 0;
    cam->imgs.motionsize = 0;

    cam->imgs.width_high  = 0;
    cam->imgs.height_high = 0;
    cam->imgs.size_high   = 0;

    if (cam->conf->width % 8) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image width (%d) requested is not modulo 8."), cam->conf->width);
        cam->conf->width = cam->conf->width - (cam->conf->width % 8) + 8;
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Adjusting width to next higher multiple of 8 (%d)."), cam->conf->width);
    }
    if (cam->conf->height % 8) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image height (%d) requested is not modulo 8."), cam->conf->height);
        cam->conf->height = cam->conf->height - (cam->conf->height % 8) + 8;
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Adjusting height to next higher multiple of 8 (%d)."), cam->conf->height);
    }

    /* Fill in camera details into context structure. */
    cam->imgs.width = cam->conf->width;
    cam->imgs.height = cam->conf->height;
    cam->imgs.size_norm = (cam->conf->width * cam->conf->height * 3) / 2;
    cam->imgs.motionsize = cam->conf->width * cam->conf->height;

    return 0;
}

static int netcam_copy_stream(struct ctx_netcam *netcam){
    /* Make a static copy of the stream information for use in passthrough processing */
    #if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
        AVStream  *transfer_stream, *stream_in;
        int        retcd;

        pthread_mutex_lock(&netcam->mutex_transfer);
            if (netcam->transfer_format != NULL) avformat_close_input(&netcam->transfer_format);
            netcam->transfer_format = avformat_alloc_context();
            transfer_stream = avformat_new_stream(netcam->transfer_format, NULL);
            stream_in = netcam->format_context->streams[netcam->video_stream_index];
            retcd = avcodec_parameters_copy(transfer_stream->codecpar, stream_in->codecpar);
            if (retcd < 0){
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("Unable to copy codec parameters"));
                pthread_mutex_unlock(&netcam->mutex_transfer);
                return -1;
            }
            transfer_stream->time_base         = stream_in->time_base;
        pthread_mutex_unlock(&netcam->mutex_transfer);

        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Stream copied for pass-through"));
        return 0;
    #elif (LIBAVFORMAT_VERSION_MAJOR >= 55)

        AVStream  *transfer_stream, *stream_in;
        int        retcd;

        pthread_mutex_lock(&netcam->mutex_transfer);
            if (netcam->transfer_format != NULL) avformat_close_input(&netcam->transfer_format);
            netcam->transfer_format = avformat_alloc_context();
            transfer_stream = avformat_new_stream(netcam->transfer_format, NULL);
            stream_in = netcam->format_context->streams[netcam->video_stream_index];
            retcd = avcodec_copy_context(transfer_stream->codec, stream_in->codec);
            if (retcd < 0){
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("Unable to copy codec parameters"));
                pthread_mutex_unlock(&netcam->mutex_transfer);
                return -1;
            }
            transfer_stream->time_base         = stream_in->time_base;
        pthread_mutex_unlock(&netcam->mutex_transfer);

        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Stream copied for pass-through"));
        return 0;
    #else
        /* This is disabled in the mycheck_passthrough but we need it here for compiling */
        if (netcam != NULL) MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, _("ffmpeg too old"));
        return -1;
    #endif

}

static int netcam_open_context(struct ctx_netcam *netcam){

    int  retcd;
    char errstr[128];

    if (netcam->finish) return -1;

    if (netcam->path == NULL) {
        if (netcam->status == NETCAM_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("Null path passed to connect"));
        }
        return -1;
    }

    netcam->opts = NULL;
    netcam->format_context = avformat_alloc_context();
    netcam->format_context->interrupt_callback.callback = netcam_interrupt;
    netcam->format_context->interrupt_callback.opaque = netcam;
    netcam->interrupted = FALSE;

    clock_gettime(CLOCK_REALTIME, &netcam->interruptstarttime);

    netcam->interruptduration = 20;

    if (strncmp(netcam->service, "http", 4) == 0 ){
        netcam_set_http(netcam);
    } else if (strncmp(netcam->service, "rtsp", 4) == 0 ){
        netcam_set_rtsp(netcam);
    } else if (strncmp(netcam->service, "rtmp", 4) == 0 ){
        netcam_set_rtsp(netcam);
    } else if (strncmp(netcam->service, "v4l2", 4) == 0 ){
        netcam_set_v4l2(netcam);
    } else if (strncmp(netcam->service, "file", 4) == 0 ){
        netcam_set_file(netcam);
    } else {
        av_dict_free(&netcam->opts);
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Invalid camera service"), netcam->cameratype);
        return -1;
    }
    /*
     * There is not many av functions above this (av_dict_free?) but we are not getting clean
     * interrupts or shutdowns via valgrind and they all point to issues with the avformat_open_input
     * right below so we make sure that we are not in a interrupt / finish situation before calling it
     */
    if ((netcam->interrupted) || (netcam->finish) ){
        if (netcam->status == NETCAM_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Unable to open camera(%s)")
                , netcam->cameratype, netcam->camera_name);
        }
        av_dict_free(&netcam->opts);
        if (netcam->interrupted) netcam_close_context(netcam);
        return -1;
    }

    retcd = avformat_open_input(&netcam->format_context, netcam->path, NULL, &netcam->opts);
    if ((retcd < 0) || (netcam->interrupted) || (netcam->finish) ){
        if (netcam->status == NETCAM_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Unable to open camera(%s): %s")
                , netcam->cameratype, netcam->camera_name, errstr);
        }
        av_dict_free(&netcam->opts);
        if (netcam->interrupted) netcam_close_context(netcam);
        return -1;
    }
    av_dict_free(&netcam->opts);
    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Opened camera(%s)"), netcam->cameratype, netcam->camera_name);

    /* fill out stream information */
    retcd = avformat_find_stream_info(netcam->format_context, NULL);
    if ((retcd < 0) || (netcam->interrupted) || (netcam->finish) ){
        if (netcam->status == NETCAM_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Unable to find stream info: %s")
                ,netcam->cameratype, errstr);
        }
        netcam_close_context(netcam);
        return -1;
    }

    /* there is no way to set the avcodec thread names, but they inherit
     * our thread name - so temporarily change our thread name to the
     * desired name */

    mythreadname_get(netcam->threadname);

    mythreadname_set("av",netcam->threadnbr,netcam->camera_name);

    retcd = netcam_open_codec(netcam);

    mythreadname_set(NULL, 0, netcam->threadname);

    if ((retcd < 0) || (netcam->interrupted) || (netcam->finish) ){
        if (netcam->status == NETCAM_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Unable to open codec context: %s")
                ,netcam->cameratype, errstr);
        }
        netcam_close_context(netcam);
        return -1;
    }

    if (netcam->codec_context->width <= 0 ||
        netcam->codec_context->height <= 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Camera image size is invalid"),netcam->cameratype);
        netcam_close_context(netcam);
        return -1;
    }

    if (netcam->high_resolution){
        netcam->imgsize.width = netcam->codec_context->width;
        netcam->imgsize.height = netcam->codec_context->height;
    } else {
        if (netcam_open_sws(netcam) < 0) return -1;
    }

    netcam->frame = myframe_alloc();
    if (netcam->frame == NULL) {
        if (netcam->status == NETCAM_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Unable to allocate frame."),netcam->cameratype);
        }
        netcam_close_context(netcam);
        return -1;
    }

    if (netcam->passthrough){
        retcd = netcam_copy_stream(netcam);
        if ((retcd < 0) || (netcam->interrupted)){
            if (netcam->status == NETCAM_NOTCONNECTED){
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("%s: Failed to copy stream for pass-through.")
                    ,netcam->cameratype);
            }
            netcam->passthrough = FALSE;
        }
    }

    /* Validate that the previous steps opened the camera */
    retcd = netcam_read_image(netcam);
    if ((retcd < 0) || (netcam->interrupted)){
        if (netcam->status == NETCAM_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Failed to read first image"),netcam->cameratype);
        }
        netcam_close_context(netcam);
        return -1;
    }

    return 0;

}

static int netcam_connect(struct ctx_netcam *netcam){

    if (netcam_open_context(netcam) < 0) return -1;

    if (netcam_ntc(netcam) < 0 ) return -1;

    if (netcam_read_image(netcam) < 0) return -1;

    /* We use the status for determining whether to grab a image from
     * the Motion loop(see "next" function).  When we are initially starting,
     * we open and close the context and during this process we do not want the
     * Motion loop to start quite yet on this first image so we do
     * not set the status to connected
     */
    if (!netcam->first_image) netcam->status = NETCAM_CONNECTED;

    MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Camera (%s) connected")
        , netcam->cameratype,netcam->camera_name);

    return 0;
}

static void netcam_shutdown(struct ctx_netcam *netcam){

    if (netcam) {
        netcam_close_context(netcam);

        if (netcam->path != NULL) free(netcam->path);

        if (netcam->img_latest != NULL){
            free(netcam->img_latest->ptr);
            free(netcam->img_latest);
        }
        if (netcam->img_recv != NULL){
            free(netcam->img_recv->ptr);
            free(netcam->img_recv);
        }

        netcam->path    = NULL;
        netcam->img_latest = NULL;
        netcam->img_recv   = NULL;
    }

}

static void netcam_handler_wait(struct ctx_netcam *netcam){
    /* This function slows down the handler loop to try to
     * get in sync with the main motion loop in the capturing
     * of images while also trying to not go so slow that the
     * connection to the  network camera is lost and we end up
     * with lots of reconnects or fragmented images
     */

    int framerate;
    long usec_maxrate, usec_delay;

    framerate = netcam->conf->framerate;
    if (framerate < 2) framerate = 2;

    if (mystreq(netcam->service,"file")) {
        /* For file processing, we try to match exactly the motion loop rate */
        usec_maxrate = (1000000L / framerate);
    } else {
        /* We set the capture rate to be a bit faster than the frame rate.  This
         * should provide the motion loop with a picture whenever it wants one.
         */
        if (framerate < netcam->src_fps) framerate = netcam->src_fps;
        usec_maxrate = (1000000L / (framerate + 3));
    }

    clock_gettime(CLOCK_REALTIME, &netcam->frame_curr_tm);

    usec_delay = usec_maxrate -
        ((netcam->frame_curr_tm.tv_sec - netcam->frame_prev_tm.tv_sec) * 1000000L) -
        ((netcam->frame_curr_tm.tv_nsec - netcam->frame_prev_tm.tv_nsec)/1000);
    if ((usec_delay > 0) && (usec_delay < 1000000L)){
        SLEEP(0, usec_delay * 1000);
    }

}

static void netcam_handler_reconnect(struct ctx_netcam *netcam){

    int retcd;

    if ((netcam->status == NETCAM_CONNECTED) ||
        (netcam->status == NETCAM_READINGIMAGE)){
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Reconnecting with camera...."),netcam->cameratype);
    }
    netcam->status = NETCAM_RECONNECTING;

    /*
    * The retry count of 100 is arbritrary.
    * We want to try many times quickly to not lose too much information
    * before we go into the long wait phase
    */
    retcd = netcam_connect(netcam);
    if (retcd < 0){
        if (netcam->reconnect_count < 100){
            netcam->reconnect_count++;
        } else if (netcam->reconnect_count == 100){
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Camera did not reconnect."), netcam->cameratype);
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Checking for camera every 10 seconds."),netcam->cameratype);
            netcam->reconnect_count++;
            SLEEP(10,0);
        } else {
            SLEEP(10,0);
        }
    } else {
        netcam->reconnect_count = 0;
    }

}

static void *netcam_handler(void *arg){

    struct ctx_netcam *netcam =(struct ctx_netcam *) arg;

    netcam->handler_finished = FALSE;

    mythreadname_set("nc",netcam->threadnbr, netcam->camera_name);

    pthread_setspecific(tls_key_threadnr, (void *)((unsigned long)netcam->threadnbr));

    MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Camera handler thread [%d] started")
        ,netcam->cameratype, netcam->threadnbr);

    while (!netcam->finish) {
        if (!netcam->format_context) {      /* We must have disconnected.  Try to reconnect */
            clock_gettime(CLOCK_REALTIME, &netcam->frame_prev_tm);
            netcam_handler_reconnect(netcam);
            continue;
        } else {            /* We think we are connected...*/
            clock_gettime(CLOCK_REALTIME, &netcam->frame_prev_tm);
            if (netcam_read_image(netcam) < 0) {
                if (!netcam->finish) {   /* Nope.  We are not or got bad image.  Reconnect*/
                    netcam_handler_reconnect(netcam);
                }
                continue;
            }
            netcam_handler_wait(netcam);
        }
    }

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Handler loop finished."),netcam->cameratype);
    netcam_shutdown(netcam);

    /* Our thread is finished - decrement motion's thread count. */
    pthread_mutex_lock(&netcam->motapp->global_lock);
        netcam->motapp->threads_running--;
    pthread_mutex_unlock(&netcam->motapp->global_lock);

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("netcam camera handler: finish set, exiting"));
    netcam->handler_finished = TRUE;

    pthread_exit(NULL);
}

static int netcam_start_handler(struct ctx_netcam *netcam){

    int retcd;
    int wait_counter;
    pthread_attr_t handler_attribute;

    pthread_mutex_init(&netcam->mutex, NULL);
    pthread_mutex_init(&netcam->mutex_pktarray, NULL);
    pthread_mutex_init(&netcam->mutex_transfer, NULL);

    pthread_attr_init(&handler_attribute);
    pthread_attr_setdetachstate(&handler_attribute, PTHREAD_CREATE_DETACHED);

    pthread_mutex_lock(&netcam->motapp->global_lock);
        netcam->threadnbr = ++netcam->motapp->threads_running;
    pthread_mutex_unlock(&netcam->motapp->global_lock);

    retcd = pthread_create(&netcam->thread_id, &handler_attribute, &netcam_handler, netcam);
    if (retcd < 0) {
        MOTION_LOG(ALR, TYPE_NETCAM, SHOW_ERRNO
            ,_("%s: Error starting handler thread"),netcam->cameratype);
        pthread_attr_destroy(&handler_attribute);
        return -1;
    }
    pthread_attr_destroy(&handler_attribute);


    /* Now give a few tries to check that an image has been captured.
     * This ensures that by the time the setup routine exits, the
     * handler is completely set up and has images available
     */
    wait_counter = 60;
    while (wait_counter > 0) {
        pthread_mutex_lock(&netcam->mutex);
            if (netcam->img_latest->ptr != NULL ) wait_counter = -1;
        pthread_mutex_unlock(&netcam->mutex);

        if (wait_counter > 0 ){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Waiting for first image from the handler."),netcam->cameratype);
            SLEEP(0,5000000);
            wait_counter--;
        }
    }
    /* Warn the user about a mismatch of camera FPS vs handler capture rate*/
    if (netcam->conf->framerate < netcam->src_fps){
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            , "Requested frame rate %d FPS is less than camera frame rate %d FPS"
            , netcam->conf->framerate,netcam->src_fps);
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            , "Increasing capture rate to %d FPS to match camera."
            , netcam->src_fps);
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            , "To lower CPU, change camera FPS to lower rate and decrease I frame interval."
            , netcam->src_fps);

    }

    return 0;

}

int netcam_setup(struct ctx_cam *cam){

    int retcd;
    int indx_cam, indx_max;
    struct ctx_netcam *netcam;

    cam->netcam = NULL;
    cam->netcam_high = NULL;

    if (netcam_set_dimensions(cam) < 0 ) return -1;

    indx_cam = 1;
    indx_max = 1;
    if (cam->conf->netcam_highres != "") indx_max = 2;

    while (indx_cam <= indx_max){
        if (indx_cam == 1){
            cam->netcam = netcam_new_context();
            if (cam->netcam == NULL) {
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("unable to create rtsp context"));
                return -1;
            }
            netcam = cam->netcam;
            netcam->high_resolution = FALSE;           /* Set flag for this being the normal resolution camera */
        } else {
            cam->netcam_high = netcam_new_context();
            if (cam->netcam_high == NULL) {
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("unable to create rtsp high context"));
                return -1;
            }
            netcam = cam->netcam_high;
            netcam->high_resolution = TRUE;            /* Set flag for this being the high resolution camera */
        }

        netcam_null_context(netcam);

        netcam_set_parms(cam, netcam);

        if (netcam_connect(netcam) < 0) return -1;

        retcd = netcam_read_image(netcam);
        if (retcd < 0){
            MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("Failed trying to read first image - retval:%d"), retcd);
            netcam->status = NETCAM_NOTCONNECTED;
            return -1;
        }
        /* When running dual, there seems to be contamination across norm/high with codec functions. */
        netcam_close_context(netcam);       /* Close in this thread to open it again within handler thread */
        netcam->status = NETCAM_RECONNECTING;      /* Set as reconnecting to avoid excess messages when starting */
        netcam->first_image = FALSE;             /* Set flag that we are not processing our first image */

        /* For normal resolution, we resize the image to the config parms so we do not need
         * to set the dimension parameters here (it is done in the set_parms).  For high res
         * we must get the dimensions from the first image captured
         */
        if (netcam->high_resolution){
            cam->imgs.width_high = netcam->imgsize.width;
            cam->imgs.height_high = netcam->imgsize.height;
        }

        if (netcam_start_handler(netcam) < 0 ) return -1;

        indx_cam++;
    }

    return 0;

}

int netcam_next(struct ctx_cam *cam, struct ctx_image_data *img_data){

    /* This is called from the motion loop thread */

    if ((cam->netcam->status == NETCAM_RECONNECTING) ||
        (cam->netcam->status == NETCAM_NOTCONNECTED)){
            return 1;
        }
    pthread_mutex_lock(&cam->netcam->mutex);
        netcam_pktarray_resize(cam, FALSE);
        memcpy(img_data->image_norm
               , cam->netcam->img_latest->ptr
               , cam->netcam->img_latest->used);
        img_data->idnbr_norm = cam->netcam->idnbr;
    pthread_mutex_unlock(&cam->netcam->mutex);

    if (cam->netcam_high){
        if ((cam->netcam_high->status == NETCAM_RECONNECTING) ||
            (cam->netcam_high->status == NETCAM_NOTCONNECTED)) return 1;

        pthread_mutex_lock(&cam->netcam_high->mutex);
            netcam_pktarray_resize(cam, TRUE);
            if (!(cam->netcam_high->high_resolution && cam->netcam_high->passthrough)) {
                memcpy(img_data->image_high
                       ,cam->netcam_high->img_latest->ptr
                       ,cam->netcam_high->img_latest->used);
            }
            img_data->idnbr_high = cam->netcam_high->idnbr;
        pthread_mutex_unlock(&cam->netcam_high->mutex);
    }

    /* Rotate images if requested */
    rotate_map(cam, img_data);

    return 0;
}

void netcam_cleanup(struct ctx_cam *cam, int init_retry_flag){

     /*
     * If the init_retry_flag is not set this function was
     * called while retrying the initial connection and there is
     * no camera-handler started yet and thread_running must
     * not be decremented.
     */
    int wait_counter;
    int indx_cam, indx_max;
    struct ctx_netcam *netcam;

    indx_cam = 1;
    indx_max = 1;
    if (cam->netcam_high) indx_max = 2;

    while (indx_cam <= indx_max) {
        if (indx_cam == 1){
            netcam = cam->netcam;
        } else {
            netcam = cam->netcam_high;
        }

        if (netcam){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Shutting down network camera."),netcam->cameratype);

            /* Throw the finish flag in context and wait a bit for it to finish its work and close everything
             * This is shutting down the thread so for the moment, we are not worrying about the
             * cross threading and protecting these variables with mutex's
            */
            netcam->finish = TRUE;
            netcam->interruptduration = 0;
            wait_counter = 0;
            while ((!netcam->handler_finished) && (wait_counter < 10)) {
                SLEEP(1,0);
                wait_counter++;
            }
            if (!netcam->handler_finished) {
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("%s: No response from handler thread."),netcam->cameratype);
                /* Last resort.  Kill the thread. Not safe for posix but if no response, what to do...*/
                /* pthread_kill(netcam->thread_id); */
                pthread_cancel(netcam->thread_id);
                pthread_kill(netcam->thread_id, SIGVTALRM); /* This allows the cancel to be processed */
                if (!init_retry_flag){
                    pthread_mutex_lock(&netcam->motapp->global_lock);
                        netcam->motapp->threads_running--;
                    pthread_mutex_unlock(&netcam->motapp->global_lock);
                }
            }
            /* If we never connect we don't have a handler but we still need to clean up some */
            netcam_shutdown(netcam);

            pthread_mutex_destroy(&netcam->mutex);
            pthread_mutex_destroy(&netcam->mutex_pktarray);
            pthread_mutex_destroy(&netcam->mutex_transfer);

            free(netcam);
            netcam = NULL;
            if (indx_cam == 1){
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("Normal resolution: Shut down complete."));
            } else {
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("High resolution: Shut down complete."));
            }
        }
        indx_cam++;
    }
    cam->netcam = NULL;
    cam->netcam_high = NULL;

}

