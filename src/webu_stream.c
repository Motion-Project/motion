/*
 *    webu_stream.c
 *
 *    Create the web streams for Motion
 *
 *    This software is distributed under the GNU Public License Version 2
 *    See also the file 'COPYING'.
 *
 *    Functional naming scheme
 *    webu_stream*      - All functions in this module
 *    webu_stream_mjpeg*    - Create the motion-jpeg stream for the user
 *    webu_stream_static*   - Create the static jpg image for the user.
 *    webu_stream_checks    - Edit/validate request from user
 */

#include "motion.h"
#include "webu.h"
#include "webu_stream.h"
#include "translate.h"

static void webu_stream_mjpeg_checkbuffers(struct webui_ctx *webui) {
    /* Allocate buffers if needed */
    if (webui->resp_size < (size_t)webui->cnt->imgs.size_norm){
        if (webui->resp_page   != NULL) free(webui->resp_page);
        webui->resp_page   = mymalloc(webui->cnt->imgs.size_norm);
        memset(webui->resp_page,'\0',webui->cnt->imgs.size_norm);
        webui->resp_size = webui->cnt->imgs.size_norm;
        webui->resp_used = 0;
    }

}

static void webu_stream_mjpeg_delay(struct webui_ctx *webui) {
    /* Sleep required time to get to the user requested frame
     * rate for the stream
     */

    long   stream_rate;
    struct timeval time_curr;
    long   stream_delay;

    gettimeofday(&time_curr, NULL);

    /* The stream rate MUST be less than 1000000000 otherwise undefined behaviour
     * will occur with the SLEEP function.
     */
    stream_delay = ((time_curr.tv_usec - webui->time_last.tv_usec)*1000) +
        ((time_curr.tv_sec - webui->time_last.tv_sec)*1000000000);
    if (stream_delay < 0)  stream_delay = 0;
    if (stream_delay > 1000000000 ) stream_delay = 1000000000;

    if (webui->stream_fps >= 1){
        stream_rate = ( (1000000000 / webui->stream_fps) - stream_delay);
        if ((stream_rate > 0) && (stream_rate < 1000000000)){
            SLEEP(0,stream_rate);
        } else if (stream_rate == 1000000000) {
            SLEEP(1,0);
        }
    }
    gettimeofday(&webui->time_last, NULL);

}

static void webu_stream_mjpeg_getimg(struct webui_ctx *webui) {
    long jpeg_size;
    char resp_head[80];
    int  header_len;
    struct stream_data *local_stream;

    memset(webui->resp_page, '\0', webui->resp_size);

    /* Assign to a local pointer the stream we want */
    if (webui->cnct_type == WEBUI_CNCT_FULL){
        local_stream = &webui->cnt->stream_norm;

    } else if (webui->cnct_type == WEBUI_CNCT_SUB){
        local_stream = &webui->cnt->stream_sub;

    } else if (webui->cnct_type == WEBUI_CNCT_MOTION){
        local_stream = &webui->cnt->stream_motion;

    } else if (webui->cnct_type == WEBUI_CNCT_SOURCE){
        local_stream = &webui->cnt->stream_source;

    } else {
        return;
    }

    /* Copy jpg from the motion loop thread */
    pthread_mutex_lock(&webui->cnt->mutex_stream);
        if ((!webui->cnt->detecting_motion) && (webui->cnt->conf.stream_motion)){
            webui->stream_fps = 1;
        } else {
            webui->stream_fps = webui->cnt->conf.stream_maxrate;
        }
        if (local_stream->jpeg_data == NULL) {
            pthread_mutex_unlock(&webui->cnt->mutex_stream);
            return;
        }
        jpeg_size = local_stream->jpeg_size;
        header_len = snprintf(resp_head, 80
            ,"--BoundaryString\r\n"
            "Content-type: image/jpeg\r\n"
            "Content-Length: %9ld\r\n\r\n"
            ,jpeg_size);
        memcpy(webui->resp_page, resp_head, header_len);
        memcpy(webui->resp_page + header_len
            ,local_stream->jpeg_data
            ,jpeg_size);
        /* Copy in the terminator after the jpg data at the end*/
        memcpy(webui->resp_page + header_len + jpeg_size,"\r\n",2);
        webui->resp_used = header_len + jpeg_size + 2;
    pthread_mutex_unlock(&webui->cnt->mutex_stream);

}

static ssize_t webu_stream_mjpeg_response (void *cls, uint64_t pos, char *buf, size_t max){
    /* This is the callback response function for MHD streams.  It is kept "open" and
     * in process during the entire time that the user has the stream open in the web
     * browser.  We sleep the requested amount of time between fetching images to match
     * the user configuration parameters.  This function may be called multiple times for
     * a single image so we can write what we can to the buffer and pick up remaining bytes
     * to send based upon the stream position
     */
    struct webui_ctx *webui = cls;
    size_t sent_bytes;

    (void)pos;  /*Remove compiler warning */

    if (webui->cnt->webcontrol_finish) return -1;

    if ((webui->stream_pos == 0) || (webui->resp_used == 0)){

        webu_stream_mjpeg_delay(webui);

        webui->stream_pos = 0;
        webui->resp_used = 0;

        webu_stream_mjpeg_getimg(webui);

        if (webui->resp_used == 0) return 0;
    }

    if ((webui->resp_used - webui->stream_pos) > max) {
        sent_bytes = max;
    } else {
        sent_bytes = webui->resp_used - webui->stream_pos;
    }

    memcpy(buf, webui->resp_page + webui->stream_pos, sent_bytes);

    webui->stream_pos = webui->stream_pos + sent_bytes;
    if (webui->stream_pos >= webui->resp_used){
        webui->stream_pos = 0;
    }

    return sent_bytes;

}

static void webu_stream_static_getimg(struct webui_ctx *webui) {
    /* Obtain the current image, compress it to a JPG and put into webui->resp_page
     * for MHD to send back to user
     */
    webui->resp_used = 0;

    memset(webui->resp_page, '\0', webui->resp_size);

    pthread_mutex_lock(&webui->cnt->mutex_stream);
        if (webui->cnt->stream_norm.jpeg_data == NULL){
            pthread_mutex_unlock(&webui->cnt->mutex_stream);
            return;
        }
        memcpy(webui->resp_page
            ,webui->cnt->stream_norm.jpeg_data
            ,webui->cnt->stream_norm.jpeg_size);
        webui->resp_used = webui->cnt->stream_norm.jpeg_size;
    pthread_mutex_unlock(&webui->cnt->mutex_stream);

}

static int webu_stream_checks(struct webui_ctx *webui) {
    /* Perform edits to determine whether the user specified a valid URL
     * for the particular port
     */
    if ((webui->cntlst != NULL) && (webui->thread_nbr >= webui->cam_threads)){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , _("Invalid thread specified: %s"),webui->url);
        return -1;
    }

    if ((webui->cntlst != NULL) && (webui->thread_nbr < 0) && (webui->cam_threads > 1)){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , _("Invalid thread specified: %s"),webui->url);
        return -1;
    }

    /* Thread numbers are not used for context specific ports. */
    if ((webui->cntlst == NULL) && (webui->thread_nbr >= 0)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , _("Invalid URL for a camera specific port: %s"),webui->url);
        return -1;
    }

    /* If multiple threads then thread zero is invalid. */
    if ((webui->cam_threads > 1) && (webui->thread_nbr == 0)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , _("URL for thread 0 is not valid when using camera specific files.: %s")
            ,webui->url);
        return -1;
    }

    /* Thread numbers are not used for context specific ports. */
    if ((webui->cntlst == NULL) && (strlen(webui->uri_cmd1) > 0)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , _("Bad URL for a camera specific port: %s"),webui->url);
        return -1;
    }

    return 0;
}

static void webu_stream_cnct_count(struct webui_ctx *webui) {
    /* Increment the counters for the connections to the streams */
    int cnct_count;

    cnct_count = 0;
    if (webui->cnct_type == WEBUI_CNCT_SUB) {
        pthread_mutex_lock(&webui->cnt->mutex_stream);
            webui->cnt->stream_sub.cnct_count++;
            cnct_count = webui->cnt->stream_sub.cnct_count;
        pthread_mutex_unlock(&webui->cnt->mutex_stream);

    } else if (webui->cnct_type == WEBUI_CNCT_MOTION) {
        pthread_mutex_lock(&webui->cnt->mutex_stream);
            webui->cnt->stream_motion.cnct_count++;
            cnct_count = webui->cnt->stream_motion.cnct_count;
        pthread_mutex_unlock(&webui->cnt->mutex_stream);

    } else if (webui->cnct_type == WEBUI_CNCT_SOURCE) {
        pthread_mutex_lock(&webui->cnt->mutex_stream);
            webui->cnt->stream_source.cnct_count++;
            cnct_count = webui->cnt->stream_source.cnct_count;
        pthread_mutex_unlock(&webui->cnt->mutex_stream);

    } else {
        /* Stream, Static */
        pthread_mutex_lock(&webui->cnt->mutex_stream);
            webui->cnt->stream_norm.cnct_count++;
            cnct_count = webui->cnt->stream_norm.cnct_count;
        pthread_mutex_unlock(&webui->cnt->mutex_stream);
    }

    if (cnct_count == 1){
        /* This is the first connection so we need to wait half a sec
         * so that the motion loop on the other thread can update image
         */
        SLEEP(0,500000000L);
    }

}

int webu_stream_mjpeg(struct webui_ctx *webui) {
    /* Create the stream for the motion jpeg */
    int retcd;
    struct MHD_Response *response;

    if (webu_stream_checks(webui) == -1) return MHD_NO;

    webu_stream_cnct_count(webui);

    webu_stream_mjpeg_checkbuffers(webui);

    gettimeofday(&webui->time_last, NULL);

    response = MHD_create_response_from_callback (MHD_SIZE_UNKNOWN, 1024
        ,&webu_stream_mjpeg_response, webui, NULL);
    if (!response){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return MHD_NO;
    }

    if (webui->cnt->conf.stream_cors_header != NULL){
        MHD_add_response_header (response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN
            , webui->cnt->conf.stream_cors_header);
    }

    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE
        , "multipart/x-mixed-replace; boundary=BoundaryString");

    retcd = MHD_queue_response (webui->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;
}

int webu_stream_static(struct webui_ctx *webui) {
    /* Create the response for the static image request*/
    int retcd;
    struct MHD_Response *response;
    char resp_used[20];

    if (webu_stream_checks(webui) == -1) return MHD_NO;

    webu_stream_cnct_count(webui);

    webu_stream_mjpeg_checkbuffers(webui);

    webu_stream_static_getimg(webui);

    if (webui->resp_used == 0) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Could not get image to stream."));
        return MHD_NO;
    }

    response = MHD_create_response_from_buffer (webui->resp_size
        ,(void *)webui->resp_page, MHD_RESPMEM_MUST_COPY);
    if (!response){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return MHD_NO;
    }

    if (webui->cnt->conf.stream_cors_header != NULL){
        MHD_add_response_header (response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN
            , webui->cnt->conf.stream_cors_header);
    }

    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "image/jpeg;");
    snprintf(resp_used, 20, "%9ld\r\n\r\n",(long)webui->resp_used);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_LENGTH, resp_used);

    retcd = MHD_queue_response (webui->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;
}
