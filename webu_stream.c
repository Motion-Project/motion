/*
 *    webu_stream.c
 *
 *    Create the web streams for Motion
 *
 *    This software is distributed under the GNU Public License Version 2
 *    See also the file 'COPYING'.
 *
 *
 */

#include "motion.h"
#include "webu.h"
#include "webu_stream.h"
#include "translate.h"
#include "picture.h"

static void webu_stream_checksize(struct webui_ctx *webui) {
    /* Determine whether a substream size if valid */
    if ((webui->cnt->imgs.width / 2) % 8 == 0  &&
        (webui->cnt->imgs.height / 2) % 8 == 0){
        webui->valid_subsize = TRUE;
    } else{
        webui->valid_subsize = FALSE;
    }
}

static void webu_stream_checkbuffers(struct webui_ctx *webui) {
    /* Allocate buffers for the images if needed */
    int subsize;

    subsize=(((webui->cnt->imgs.width/2) * (webui->cnt->imgs.height/2) * 3)/2);

    if (webui->resp_size < (size_t)webui->cnt->imgs.size_norm){
        if (webui->resp_page   != NULL) free(webui->resp_page);
        webui->resp_page   = mymalloc(webui->cnt->imgs.size_norm);
        memset(webui->resp_page,'\0',webui->cnt->imgs.size_norm);
        webui->resp_size = webui->cnt->imgs.size_norm;
        webui->resp_used = 0;
    }

    if (webui->stream_img_size < (size_t)webui->cnt->imgs.size_norm) {
        if (webui->stream_img    != NULL) free(webui->stream_img);
        webui->stream_img    = mymalloc(webui->cnt->imgs.size_norm);
        memset(webui->stream_img,'\0',webui->cnt->imgs.size_norm);
        webui->stream_img_size = (size_t)webui->cnt->imgs.size_norm;

        if (webui->stream_imgsub != NULL) free(webui->stream_imgsub);
        webui->stream_imgsub = mymalloc(subsize);
        memset(webui->stream_imgsub,'\0',subsize);
    }

}

static void webu_stream_mjpeg_getimg(struct webui_ctx *webui) {
    /* Get the image from the motion context and compress it into a JPG*/
    int jpeg_size;
    char resp_size[20];
    int  resp_len, height, width, subsize;
    const char resp_head[] = "\r\n--BoundaryString\r\n"
                             "Content-type: image/jpeg\r\n"
                             "Content-Length:                 ";

    webui->resp_size = 0;

    if (webui->cnt->imgs.size_norm == 0) return;

    width = webui->cnt->imgs.width;
    height = webui->cnt->imgs.height;
    subsize=(((width/2) * (height/2) * 3)/2);

    webu_stream_checkbuffers(webui);

    webu_stream_checksize(webui);

    pthread_mutex_lock(&webui->cnt->mutex_stream);
        if (webui->cnt->imgs.image_stream == NULL) {
            pthread_mutex_unlock(&webui->cnt->mutex_stream);
            return;
        }
        memcpy(webui->stream_img
            ,webui->cnt->imgs.image_stream
            ,webui->cnt->imgs.size_norm);
    pthread_mutex_unlock(&webui->cnt->mutex_stream);

    memcpy(webui->resp_page, resp_head, strlen(resp_head));

    if (((strcmp(webui->uri_cmd1,"substream") == 0) ||
        (strcmp(webui->uri_thread,"substream") == 0)) &&
        (webui->valid_subsize)){
        pic_scale_img(width, height, webui->stream_img, webui->stream_imgsub);
        jpeg_size = put_picture_memory(webui->cnt
            ,(unsigned char *)(webui->resp_page + strlen(resp_head))
            ,subsize
            ,webui->stream_imgsub
            ,webui->cnt->conf.stream_quality
            ,(width/2),(height/2));
    } else {
        jpeg_size = put_picture_memory(webui->cnt
            ,(unsigned char *)(webui->resp_page + strlen(resp_head))
            ,webui->cnt->imgs.size_norm
            ,webui->stream_img
            ,webui->cnt->conf.stream_quality
            ,width,height);
    }

    resp_len = snprintf(resp_size, 20, "%9ld\r\n\r\n", (long)jpeg_size);
    memcpy(webui->resp_page + strlen(resp_head) - resp_len, resp_size, resp_len);
    memcpy(webui->resp_page + strlen(resp_head) + jpeg_size + 2,"\r\n",2);
    webui->resp_used = strlen(resp_head) + jpeg_size + 2;

}

static ssize_t webu_stream_response (void *cls, uint64_t pos, char *buf, size_t max){
    /* This is the callback response function for MHD streams.  It is kept "open" and
     * in process during the entire time that the user has the stream open in the web
     * browser.  We sleep the requested amount of time between fetching images to match
     * the user configuration parameters.  This function may be called multiple times for
     * a single image so we write what we can to the buffer and pick up remaining bytes
     * to send based upon the static variable of stream position
     */
    struct webui_ctx *webui = cls;
    static uint64_t stream_pos;
    size_t sent_bytes;
    long   stream_rate;
    static int indx_start;

    (void)pos;  /*Remove compiler warning */
    /* We use indx_start to send a few extra images when the stream starts*/

    if ((stream_pos == 0) || (webui->resp_used == 0)){
        if (webui->resp_used == 0) indx_start = 1;
        if (indx_start > 3) {
            indx_start = 3;
            if (webui->cnt->conf.stream_maxrate > 1){
                stream_rate =  (1000000000 / webui->cnt->conf.stream_maxrate);
                SLEEP(0,stream_rate);
            } else {
                SLEEP(1,0);
            }
        }
        indx_start++;
        stream_pos = 0;
        webu_stream_mjpeg_getimg(webui);

        if (webui->resp_used == 0) return 0;
    }

    if ((webui->resp_used - stream_pos) > max) {
        sent_bytes = max;
    } else {
        sent_bytes = webui->resp_used - stream_pos;
    }

    memcpy(buf, webui->resp_page + stream_pos, sent_bytes);

    stream_pos = stream_pos + sent_bytes + 1 ;
    if (stream_pos >= webui->resp_used){
        stream_pos = 0;
    }

    return sent_bytes;

}

static void webu_stream_static_getimg(struct webui_ctx *webui) {
    /* Obtain the current image, compress it to a JPG and put into webui->resp_page
     * for MHD to send back to user
     */
    webui->resp_used = 0;

    if (webui->resp_size < (size_t)webui->cnt->imgs.size_norm){
        if (webui->resp_page  != NULL) free(webui->resp_page);
        webui->resp_page  = mymalloc(webui->cnt->imgs.size_norm);
        memset(webui->resp_page,'\0',webui->cnt->imgs.size_norm);
        webui->resp_size = webui->cnt->imgs.size_norm;
    }
    if (webui->stream_img_size < (size_t)webui->cnt->imgs.size_norm) {
        if (webui->stream_img != NULL) free(webui->stream_img);
        webui->stream_img = mymalloc(webui->cnt->imgs.size_norm);
        memset(webui->stream_img,'\0',webui->cnt->imgs.size_norm);
        webui->stream_img_size = (size_t)webui->cnt->imgs.size_norm;
    }

    pthread_mutex_lock(&webui->cnt->mutex_stream);
        memcpy(webui->stream_img
            ,webui->cnt->imgs.image_stream
            ,webui->cnt->imgs.size_norm);
    pthread_mutex_unlock(&webui->cnt->mutex_stream);

    webui->resp_used = put_picture_memory(webui->cnt
        ,(unsigned char *)webui->resp_page
        ,webui->cnt->imgs.size_norm
        ,webui->stream_img
        ,webui->cnt->conf.stream_quality
        ,webui->cnt->imgs.width
        ,webui->cnt->imgs.height);
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

int webu_stream_mjpeg(struct webui_ctx *webui) {
    /* Create the stream for the motion jpeg */
    int retcd;
    struct MHD_Response *response;

    if (webu_stream_checks(webui) == -1) return MHD_NO;

    response = MHD_create_response_from_callback (MHD_SIZE_UNKNOWN, 1024
        ,&webu_stream_response, webui, NULL);
    if (!response){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid response");
        return MHD_NO;
    }

    if (webui->cnt->conf.webcontrol_cors_header != NULL){
        MHD_add_response_header (response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN
            , webui->cnt->conf.webcontrol_cors_header);
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
    char resp_size[20];

    if (webu_stream_checks(webui) == -1) return MHD_NO;

    webu_stream_static_getimg(webui);

    response = MHD_create_response_from_buffer (webui->resp_size
        ,(void *)webui->resp_page, MHD_RESPMEM_MUST_COPY);
    if (!response){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid response");
        return MHD_NO;
    }

    if (webui->cnt->conf.stream_cors_header != NULL){
        MHD_add_response_header (response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN
            , webui->cnt->conf.stream_cors_header);
    }

    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "image/jpeg;");
    snprintf(resp_size, 20, "%9ld\r\n\r\n",(long)webui->resp_size);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_LENGTH, resp_size);

    retcd = MHD_queue_response (webui->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;
}