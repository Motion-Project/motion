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
 *    Copyright 2020-2023 MotionMrDave@gmail.com
*/

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "picture.hpp"
#include "webu.hpp"
#include "webu_stream.hpp"
#include "alg_sec.hpp"


/* Allocate buffers if needed */
static void webu_stream_mjpeg_checkbuffers(ctx_webui *webui)
{
    if (webui->resp_size < (size_t)webui->cam->imgs.size_norm) {
        if (webui->resp_image   != NULL) {
            myfree(&webui->resp_image);
        }
        webui->resp_image   =(char*) mymalloc(webui->cam->imgs.size_norm);
        memset(webui->resp_image,'\0',webui->cam->imgs.size_norm);
        webui->resp_size = webui->cam->imgs.size_norm;
        webui->resp_used = 0;
    }

}

/* Sleep required time to get to the user requested framerate for the stream */
static void webu_stream_mjpeg_delay(ctx_webui *webui)
{
    long   stream_rate;
    struct timespec time_curr;
    long   stream_delay;

    if ((webui->motapp->webcontrol_finish) ||
        (webui->cam->finish_dev)) {
        webui->resp_used = 0;
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &time_curr);

    /* The stream rate MUST be less than 1000000000 otherwise undefined behaviour
     * will occur with the SLEEP function.
     */
    stream_delay = ((time_curr.tv_nsec - webui->time_last.tv_nsec)) +
        ((time_curr.tv_sec - webui->time_last.tv_sec)*1000000000);
    if (stream_delay < 0)  {
        stream_delay = 0;
    }
    if (stream_delay > 1000000000 ) {
        stream_delay = 1000000000;
    }

    if (webui->stream_fps >= 1) {
        stream_rate = ( (1000000000 / webui->stream_fps) - stream_delay);
        if ((stream_rate > 0) && (stream_rate < 1000000000)) {
            SLEEP(0,stream_rate);
        } else if (stream_rate == 1000000000) {
            SLEEP(1,0);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &webui->time_last);

}

static void webu_stream_mjpeg_getimg(ctx_webui *webui)
{
    long jpeg_size;
    char resp_head[80];
    int  header_len;
    ctx_stream_data *local_stream;

    if ((webui->motapp->webcontrol_finish) ||
        (webui->cam->finish_dev)) {
        webui->resp_used = 0;
        return;
    }

    memset(webui->resp_image, '\0', webui->resp_size);

    /* Assign to a local pointer the stream we want */
    if (webui->cnct_type == WEBUI_CNCT_FULL) {
        local_stream = &webui->cam->stream.norm;

    } else if (webui->cnct_type == WEBUI_CNCT_SUB) {
        local_stream = &webui->cam->stream.sub;

    } else if (webui->cnct_type == WEBUI_CNCT_MOTION) {
        local_stream = &webui->cam->stream.motion;

    } else if (webui->cnct_type == WEBUI_CNCT_SOURCE) {
        local_stream = &webui->cam->stream.source;

    } else if (webui->cnct_type == WEBUI_CNCT_SECONDARY) {
        local_stream = &webui->cam->stream.secondary;

    } else {
        return;
    }

    /* Copy jpg from the motion loop thread */
    pthread_mutex_lock(&webui->cam->stream.mutex);
        if ((!webui->cam->detecting_motion) &&
            (webui->motapp->cam_list[webui->threadnbr]->conf->stream_motion)) {
            webui->stream_fps = 1;
        } else {
            webui->stream_fps = webui->motapp->cam_list[webui->threadnbr]->conf->stream_maxrate;
        }
        if (local_stream->jpeg_data == NULL) {
            pthread_mutex_unlock(&webui->cam->stream.mutex);
            return;
        }
        jpeg_size = local_stream->jpeg_size;
        header_len = snprintf(resp_head, 80
            ,"--BoundaryString\r\n"
            "Content-type: image/jpeg\r\n"
            "Content-Length: %9ld\r\n\r\n"
            ,jpeg_size);
        memcpy(webui->resp_image, resp_head, header_len);
        memcpy(webui->resp_image + header_len
            ,local_stream->jpeg_data
            ,jpeg_size);
        /* Copy in the terminator after the jpg data at the end*/
        memcpy(webui->resp_image + header_len + jpeg_size,"\r\n",2);
        webui->resp_used = header_len + jpeg_size + 2;
        local_stream->consumed = true;
    pthread_mutex_unlock(&webui->cam->stream.mutex);

}

/* Callback function for mhd to get stream */
static ssize_t webu_stream_mjpeg_response (void *cls, uint64_t pos, char *buf, size_t max)
{
    /* This is the callback response function for MHD streams.  It is kept "open" and
     * in process during the entire time that the user has the stream open in the web
     * browser.  We sleep the requested amount of time between fetching images to match
     * the user configuration parameters.  This function may be called multiple times for
     * a single image so we can write what we can to the buffer and pick up remaining bytes
     * to send based upon the stream position
     */
    ctx_webui *webui =(ctx_webui *)cls;
    size_t sent_bytes;

    (void)pos;  /*Remove compiler warning */

    if ((webui->motapp->webcontrol_finish) ||
        (webui->cam->finish_dev)) {
        return -1;
    }

    if ((webui->stream_pos == 0) || (webui->resp_used == 0)) {

        webu_stream_mjpeg_delay(webui);

        webui->stream_pos = 0;
        webui->resp_used = 0;

        webu_stream_mjpeg_getimg(webui);

        if (webui->resp_used == 0) {
            return 0;
        }
    }

    if ((webui->resp_used - webui->stream_pos) > max) {
        sent_bytes = max;
    } else {
        sent_bytes = webui->resp_used - webui->stream_pos;
    }

    memcpy(buf, webui->resp_image + webui->stream_pos, sent_bytes);

    webui->stream_pos = webui->stream_pos + sent_bytes;
    if (webui->stream_pos >= webui->resp_used) {
        webui->stream_pos = 0;
    }

    return sent_bytes;

}

/* Obtain the current image, compress it to a JPG and put into webui->resp_image */
static void webu_stream_static_getimg(ctx_webui *webui)
{

    ctx_stream_data *local_stream;

    webui->resp_used = 0;
    memset(webui->resp_image, '\0', webui->resp_size);

    /* Assign to a local pointer the stream we want */
    if (webui->cnct_type == WEBUI_CNCT_FULL) {
        local_stream = &webui->cam->stream.norm;

    } else if (webui->cnct_type == WEBUI_CNCT_SUB) {
        local_stream = &webui->cam->stream.sub;

    } else if (webui->cnct_type == WEBUI_CNCT_MOTION) {
        local_stream = &webui->cam->stream.motion;

    } else if (webui->cnct_type == WEBUI_CNCT_SOURCE) {
        local_stream = &webui->cam->stream.source;

    } else if (webui->cnct_type == WEBUI_CNCT_SECONDARY) {
        local_stream = &webui->cam->stream.secondary;

    } else {
        return;
    }

    pthread_mutex_lock(&webui->cam->stream.mutex);
        if (local_stream->jpeg_data == NULL) {
            pthread_mutex_unlock(&webui->cam->stream.mutex);
            return;
        }
        memcpy(webui->resp_image
            ,local_stream->jpeg_data
            ,local_stream->jpeg_size);
        webui->resp_used =local_stream->jpeg_size;
        local_stream->consumed = true;
    pthread_mutex_unlock(&webui->cam->stream.mutex);

}

/* Determine whether the user specified a valid URL for the particular port */
static int webu_stream_checks(ctx_webui *webui)
{
    pthread_mutex_lock(&webui->motapp->mutex_camlst);
        if (webui->threadnbr == -1) {
            MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
                , _("Invalid thread specified: %s"),webui->url.c_str());
            pthread_mutex_unlock(&webui->motapp->mutex_camlst);
            return -1;
        }

        if (webui->threadnbr < 0) {
            MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
                , _("Invalid thread specified: %s"),webui->url.c_str());
            pthread_mutex_unlock(&webui->motapp->mutex_camlst);
            return -1;
        }

        if ((webui->motapp->webcontrol_finish) ||
            (webui->cam->finish_dev)) {
            return -1;
        }

    pthread_mutex_unlock(&webui->motapp->mutex_camlst);

    return 0;
}

/* Increment the counters for the connections to the streams */
static void webu_stream_cnct_count(ctx_webui *webui)
{
    int cnct_count;

    cnct_count = 0;
    if (webui->cnct_type == WEBUI_CNCT_SUB) {
        pthread_mutex_lock(&webui->cam->stream.mutex);
            webui->cam->stream.sub.cnct_count++;
            cnct_count = webui->cam->stream.sub.cnct_count;
        pthread_mutex_unlock(&webui->cam->stream.mutex);

    } else if (webui->cnct_type == WEBUI_CNCT_MOTION) {
        pthread_mutex_lock(&webui->cam->stream.mutex);
            webui->cam->stream.motion.cnct_count++;
            cnct_count = webui->cam->stream.motion.cnct_count;
        pthread_mutex_unlock(&webui->cam->stream.mutex);

    } else if (webui->cnct_type == WEBUI_CNCT_SOURCE) {
        pthread_mutex_lock(&webui->cam->stream.mutex);
            webui->cam->stream.source.cnct_count++;
            cnct_count = webui->cam->stream.source.cnct_count;
        pthread_mutex_unlock(&webui->cam->stream.mutex);

    } else if (webui->cnct_type == WEBUI_CNCT_SECONDARY) {
        pthread_mutex_lock(&webui->cam->stream.mutex);
            webui->cam->stream.secondary.cnct_count++;
            cnct_count = webui->cam->stream.secondary.cnct_count;
        pthread_mutex_unlock(&webui->cam->stream.mutex);

    } else {
        /* Stream */
        pthread_mutex_lock(&webui->cam->stream.mutex);
            webui->cam->stream.norm.cnct_count++;
            cnct_count = webui->cam->stream.norm.cnct_count;
        pthread_mutex_unlock(&webui->cam->stream.mutex);
    }

    if (cnct_count == 1) {
        /* This is the first connection so we need to wait half a sec
         * so that the motion loop on the other thread can update image
         */
        SLEEP(0,500000000L);
    }

}

/* Assign the type of stream that is being answered*/
static void webu_stream_type(ctx_webui *webui)
{
    if (webui->uri_cmd2 == "stream") {
        webui->cnct_type = WEBUI_CNCT_FULL;

    } else if (webui->uri_cmd2 == "substream") {
        webui->cnct_type = WEBUI_CNCT_SUB;

    } else if (webui->uri_cmd2 == "motion") {
        webui->cnct_type = WEBUI_CNCT_MOTION;

    } else if (webui->uri_cmd2 == "source") {
        webui->cnct_type = WEBUI_CNCT_SOURCE;

    } else if (webui->uri_cmd2 == "secondary") {
        if (webui->cam->algsec_inuse) {
            webui->cnct_type = WEBUI_CNCT_SECONDARY;
        } else {
            webui->cnct_type = WEBUI_CNCT_UNKNOWN;
        }

    } else if (webui->uri_cmd2 == "") {
        webui->cnct_type = WEBUI_CNCT_FULL;

    } else {
        webui->cnct_type = WEBUI_CNCT_UNKNOWN;
    }

}

static mhdrslt webu_stream_mjpeg(ctx_webui *webui)
{
    /* Create the stream for the motion jpeg */
    mhdrslt retcd;
    struct MHD_Response *response;
    int indx;

    if (webu_stream_checks(webui) == -1) {
        return MHD_NO;
    }

    webu_stream_cnct_count(webui);

    webu_stream_mjpeg_checkbuffers(webui);

    clock_gettime(CLOCK_MONOTONIC, &webui->time_last);

    response = MHD_create_response_from_callback (MHD_SIZE_UNKNOWN, 1024
        ,&webu_stream_mjpeg_response, webui, NULL);
    if (!response) {
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return MHD_NO;
    }

    if (webui->motapp->webcontrol_headers->params_count > 0) {
        for (indx = 0; indx < webui->motapp->webcontrol_headers->params_count; indx++) {
            MHD_add_response_header (response
                , webui->motapp->webcontrol_headers->params_array[indx].param_name
                , webui->motapp->webcontrol_headers->params_array[indx].param_value
            );
        }
    }

    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE
        , "multipart/x-mixed-replace; boundary=BoundaryString");


    retcd = MHD_queue_response (webui->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;
}

/* Create the response for the static image request*/
static mhdrslt webu_stream_static(ctx_webui *webui)
{
    mhdrslt retcd;
    struct MHD_Response *response;
    char resp_used[20];
    int indx;

    if (webu_stream_checks(webui) == -1) {
        return MHD_NO;
    }

    webu_stream_cnct_count(webui);

    webu_stream_mjpeg_checkbuffers(webui);

    webu_stream_static_getimg(webui);

    if (webui->resp_used == 0) {
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Could not get image to stream."));
        return MHD_NO;
    }

    response = MHD_create_response_from_buffer (webui->resp_size
        ,(void *)webui->resp_image, MHD_RESPMEM_MUST_COPY);
    if (!response) {
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return MHD_NO;
    }

    if (webui->motapp->webcontrol_headers->params_count > 0) {
        for (indx = 0; indx < webui->motapp->webcontrol_headers->params_count; indx++) {
            MHD_add_response_header (response
                , webui->motapp->webcontrol_headers->params_array[indx].param_name
                , webui->motapp->webcontrol_headers->params_array[indx].param_value
            );
        }
    }

    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "image/jpeg");
    snprintf(resp_used, 20, "%9ld\r\n\r\n",(long)webui->resp_used);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_LENGTH, resp_used);

    retcd = MHD_queue_response (webui->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;
}

/* Entry point for answering stream*/
mhdrslt webu_stream_main(ctx_webui *webui)
{
    mhdrslt retcd;

    if (webui->cam == NULL) {
        return MHD_NO;
    }

    if ((webui->cam->passflag == 0) || (webui->cam->finish_dev)) {
        return MHD_NO;
    }

    webu_stream_type(webui);

    if (webui->uri_cmd1 == "static") {
        retcd = webu_stream_static(webui);
    } else {
        retcd = webu_stream_mjpeg(webui);
    }

    return retcd;
}

/* Initial the stream context items for the camera */
void webu_stream_init(ctx_dev *cam)
{
    /* NOTE:  This runs on the motion_loop thread. */

    pthread_mutex_init(&cam->stream.mutex, NULL);

    cam->imgs.image_substream = NULL;

    cam->stream.norm.jpeg_size = 0;
    cam->stream.norm.jpeg_data = NULL;
    cam->stream.norm.cnct_count = 0;
    cam->stream.norm.consumed = true;

    cam->stream.sub.jpeg_size = 0;
    cam->stream.sub.jpeg_data = NULL;
    cam->stream.sub.cnct_count = 0;
    cam->stream.sub.consumed = true;

    cam->stream.motion.jpeg_size = 0;
    cam->stream.motion.jpeg_data = NULL;
    cam->stream.motion.cnct_count = 0;
    cam->stream.motion.consumed = true;

    cam->stream.source.jpeg_size = 0;
    cam->stream.source.jpeg_data = NULL;
    cam->stream.source.cnct_count = 0;
    cam->stream.source.consumed = true;

    cam->stream.secondary.jpeg_size = 0;
    cam->stream.secondary.jpeg_data = NULL;
    cam->stream.secondary.cnct_count = 0;
    cam->stream.source.consumed = true;

}

/* Free the stream buffers and mutex for shutdown */
void webu_stream_deinit(ctx_dev *cam)
{
    /* NOTE:  This runs on the motion_loop thread. */

    pthread_mutex_destroy(&cam->stream.mutex);

    myfree(&cam->imgs.image_substream);
    myfree(&cam->stream.norm.jpeg_data);
    myfree(&cam->stream.sub.jpeg_data);
    myfree(&cam->stream.motion.jpeg_data);
    myfree(&cam->stream.source.jpeg_data);
    myfree(&cam->stream.secondary.jpeg_data);

}

/* Get a normal image from the motion loop and compress it*/
static void webu_stream_getimg_norm(ctx_dev *cam, ctx_image_data *img_data)
{
    /*This is on the motion_loop thread */
    if (cam->stream.norm.jpeg_data == NULL) {
        cam->stream.norm.jpeg_data =(unsigned char*)mymalloc(cam->imgs.size_norm);
    }
    if (img_data->image_norm != NULL && cam->stream.norm.consumed) {
        cam->stream.norm.jpeg_size = pic_put_memory(cam
            ,cam->stream.norm.jpeg_data
            ,cam->imgs.size_norm
            ,img_data->image_norm
            ,cam->conf->stream_quality
            ,cam->imgs.width
            ,cam->imgs.height);
        cam->stream.norm.consumed = false;
    }

}

/* Get a substream image from the motion loop and compress it*/
static void webu_stream_getimg_sub(ctx_dev *cam, ctx_image_data *img_data)
{
    /*This is on the motion_loop thread */

    int subsize;

    if (cam->stream.sub.jpeg_data == NULL) {
        cam->stream.sub.jpeg_data =(unsigned char*)mymalloc(cam->imgs.size_norm);
    }
    if (img_data->image_norm != NULL && cam->stream.sub.consumed) {
        /* Resulting substream image must be multiple of 8 */
        if (((cam->imgs.width  % 16) == 0)  &&
            ((cam->imgs.height % 16) == 0)) {

            subsize = ((cam->imgs.width / 2) * (cam->imgs.height / 2) * 3 / 2);
            if (cam->imgs.image_substream == NULL) {
                cam->imgs.image_substream =(unsigned char*)mymalloc(subsize);
            }
            pic_scale_img(cam->imgs.width
                ,cam->imgs.height
                ,img_data->image_norm
                ,cam->imgs.image_substream);
            cam->stream.sub.jpeg_size = pic_put_memory(cam
                ,cam->stream.sub.jpeg_data
                ,subsize
                ,cam->imgs.image_substream
                ,cam->conf->stream_quality
                ,(cam->imgs.width / 2)
                ,(cam->imgs.height / 2));
        } else {
            /* Substream was not multiple of 8 so send full image*/
            cam->stream.sub.jpeg_size = pic_put_memory(cam
                ,cam->stream.sub.jpeg_data
                ,cam->imgs.size_norm
                ,img_data->image_norm
                ,cam->conf->stream_quality
                ,cam->imgs.width
                ,cam->imgs.height);
        }
        cam->stream.sub.consumed = false;
    }

}

/* Get a motion image from the motion loop and compress it*/
static void webu_stream_getimg_motion(ctx_dev *cam)
{
    /*This is on the motion_loop thread */

    if (cam->stream.motion.jpeg_data == NULL) {
        cam->stream.motion.jpeg_data =(unsigned char*)mymalloc(cam->imgs.size_norm);
    }
    if (cam->imgs.image_motion.image_norm != NULL  && cam->stream.motion.consumed) {
        cam->stream.motion.jpeg_size = pic_put_memory(cam
            ,cam->stream.motion.jpeg_data
            ,cam->imgs.size_norm
            ,cam->imgs.image_motion.image_norm
            ,cam->conf->stream_quality
            ,cam->imgs.width
            ,cam->imgs.height);
        cam->stream.motion.consumed = false;
    }

}

/* Get a source image from the motion loop and compress it*/
static void webu_stream_getimg_source(ctx_dev *cam)
{
    /*This is on the motion_loop thread */

    if (cam->stream.source.jpeg_data == NULL) {
        cam->stream.source.jpeg_data =(unsigned char*)mymalloc(cam->imgs.size_norm);
    }
    if (cam->imgs.image_virgin != NULL && cam->stream.source.consumed) {
        cam->stream.source.jpeg_size = pic_put_memory(cam
            ,cam->stream.source.jpeg_data
            ,cam->imgs.size_norm
            ,cam->imgs.image_virgin
            ,cam->conf->stream_quality
            ,cam->imgs.width
            ,cam->imgs.height);
        cam->stream.source.consumed = false;
    }

}

/* Get a secondary image from the motion loop and compress it*/
static void webu_stream_getimg_secondary(ctx_dev *cam)
{
    /*This is on the motion_loop thread */

    if (cam->imgs.size_secondary>0) {
        pthread_mutex_lock(&cam->algsec->mutex);
            if (cam->stream.secondary.jpeg_data == NULL) {
                cam->stream.secondary.jpeg_data =(unsigned char*)mymalloc(cam->imgs.size_norm);
            }
            memcpy(cam->stream.secondary.jpeg_data,cam->imgs.image_secondary,cam->imgs.size_secondary);
            cam->stream.secondary.jpeg_size = cam->imgs.size_secondary;
        pthread_mutex_unlock(&cam->algsec->mutex);
    } else {
        myfree(&cam->stream.secondary.jpeg_data);
    }

}

/* Get image from the motion loop and compress it*/
void webu_stream_getimg(ctx_dev *cam, ctx_image_data *img_data)
{

    /*This is on the motion_loop thread */

    pthread_mutex_lock(&cam->stream.mutex);
        if (cam->stream.norm.cnct_count > 0)        webu_stream_getimg_norm(cam, img_data);
        if (cam->stream.sub.cnct_count > 0)         webu_stream_getimg_sub(cam, img_data);
        if (cam->stream.motion.cnct_count > 0)      webu_stream_getimg_motion(cam);
        if (cam->stream.source.cnct_count > 0)      webu_stream_getimg_source(cam);
        if (cam->stream.secondary.cnct_count > 0)   webu_stream_getimg_secondary(cam);
    pthread_mutex_unlock(&cam->stream.mutex);
}
