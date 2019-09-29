/*
 * mmalcam.c
 *
 *    Raspberry Pi camera module using MMAL API.
 *
 *    Built upon functionality from the Raspberry Pi userland utility raspivid.
 *
 *    Copyright 2013 by Nicholas Tuckett
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */

#include "translate.h"
#include "motion.h"
#include "rotate.h"

#ifdef HAVE_MMAL

#include "interface/vcos/vcos.h"
#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/mmal_port.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"
#include "raspicam/RaspiCamControl.h"

#define MMALCAM_OK        0
#define MMALCAM_ERROR    -1

#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2
#define VIDEO_FRAME_RATE_NUM 30
#define VIDEO_FRAME_RATE_DEN 1
#define VIDEO_OUTPUT_BUFFERS_NUM 3

const int MAX_BITRATE = 30000000; // 30Mbits/s

static void parse_camera_control_params(const char *control_params_str, RASPICAM_CAMERA_PARAMETERS *camera_params)
{
    char *control_params_tok = alloca(strlen(control_params_str) + 1);
    strcpy(control_params_tok, control_params_str);

    char *next_param = strtok(control_params_tok, " ");

    while (next_param != NULL) {
        char *param_val = strtok(NULL, " ");
        if (raspicamcontrol_parse_cmdline(camera_params, next_param + 1, param_val) < 2) {
            next_param = param_val;
        } else {
            next_param = strtok(NULL, " ");
        }
    }
}

static void check_disable_port(MMAL_PORT_T *port)
{
    if (port && port->is_enabled) {
        mmal_port_disable(port);
    }
}

static void camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    if (buffer->cmd != MMAL_EVENT_PARAMETER_CHANGED) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("Received unexpected camera control callback event, 0x%08x"), buffer->cmd);
    }

    mmal_buffer_header_release(buffer);
}

static void camera_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    mmalcam_context_ptr mmalcam = (mmalcam_context_ptr) port->userdata;
    mmal_queue_put(mmalcam->camera_buffer_queue, buffer);
}

static void set_port_format(mmalcam_context_ptr mmalcam, MMAL_ES_FORMAT_T *format)
{
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;
    format->es->video.width = mmalcam->width;
    format->es->video.height = mmalcam->height;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = mmalcam->width;
    format->es->video.crop.height = mmalcam->height;
}

static void set_video_port_format(mmalcam_context_ptr mmalcam, MMAL_ES_FORMAT_T *format)
{
    set_port_format(mmalcam, format);
    format->es->video.frame_rate.num = mmalcam->framerate;
    format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;
    if (mmalcam->framerate > 30){
        /* The pi noir camera could not determine autoexpose at high frame rates */
        MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO, _("A high frame rate can cause problems with exposure of images"));
        MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO, _("If autoexposure is not working, try a lower frame rate."));
    }
}

static int create_camera_component(mmalcam_context_ptr mmalcam, const char *mmalcam_name)
{
    MMAL_STATUS_T status;
    MMAL_COMPONENT_T *camera_component;
    MMAL_PORT_T *video_port = NULL;

    status = mmal_component_create(mmalcam_name, &camera_component);

    if (status != MMAL_SUCCESS) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("Failed to create MMAL camera component %s"), mmalcam_name);
        goto error;
    }

    if (camera_component->output_num == 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("MMAL camera %s doesn't have output ports"), mmalcam_name);
        goto error;
    }

    video_port = camera_component->output[MMAL_CAMERA_VIDEO_PORT];

    status = mmal_port_enable(camera_component->control, camera_control_callback);

    if (status) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("Unable to enable control port : error %d"), status);
        goto error;
    }

    //  set up the camera configuration
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
                { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
                .max_stills_w = mmalcam->width,
                .max_stills_h = mmalcam->height,
                .stills_yuv422 = 0,
                .one_shot_stills = 0,
                .max_preview_video_w = mmalcam->width,
                .max_preview_video_h = mmalcam->height,
                .num_preview_video_frames = 3,
                .stills_capture_circular_buffer_height = 0,
                .fast_preview_resume = 0,
                .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC };
        mmal_port_parameter_set(camera_component->control, &cam_config.hdr);
    }

    set_video_port_format(mmalcam, video_port->format);
    video_port->format->encoding = MMAL_ENCODING_I420;
    // set buffer size for an aligned/padded frame
    video_port->buffer_size = VCOS_ALIGN_UP(mmalcam->width, 32) *
        VCOS_ALIGN_UP(mmalcam->height, 16) * 3 / 2;

    if (mmal_port_parameter_set_boolean(video_port, MMAL_PARAMETER_NO_IMAGE_PADDING, 1)
            != MMAL_SUCCESS) {
        MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO, _("MMAL no-padding setup failed"));
    }

    status = mmal_port_format_commit(video_port);

    if (status) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, _("camera video format couldn't be set"));
        goto error;
    }

    // Ensure there are enough buffers to avoid dropping frames
    if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM) {
        video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
    }

    status = mmal_component_enable(camera_component);

    if (status) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, _("camera component couldn't be enabled"));
        goto error;
    }

    raspicamcontrol_set_all_parameters(camera_component, mmalcam->camera_parameters);
    mmalcam->camera_component = camera_component;
    mmalcam->camera_capture_port = video_port;
    mmalcam->camera_capture_port->userdata = (struct MMAL_PORT_USERDATA_T*) mmalcam;
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, _("MMAL camera component created"));
    return MMALCAM_OK;

    error: if (mmalcam->camera_component != NULL ) {
        mmal_component_destroy(camera_component);
        mmalcam->camera_component = NULL;
    }

    return MMALCAM_ERROR;
}

static void destroy_camera_component(mmalcam_context_ptr mmalcam)
{
    if (mmalcam->camera_component) {
        mmal_component_destroy(mmalcam->camera_component);
        mmalcam->camera_component = NULL;
    }
}

static int create_camera_buffer_structures(mmalcam_context_ptr mmalcam)
{
    mmalcam->camera_buffer_pool = mmal_pool_create(mmalcam->camera_capture_port->buffer_num,
            mmalcam->camera_capture_port->buffer_size);
    if (mmalcam->camera_buffer_pool == NULL ) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, _("MMAL camera buffer pool creation failed"));
        return MMALCAM_ERROR;
    }

    mmalcam->camera_buffer_queue = mmal_queue_create();
    if (mmalcam->camera_buffer_queue == NULL ) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, _("MMAL camera buffer queue creation failed"));
        return MMALCAM_ERROR;
    }

    return MMALCAM_OK;
}

static int send_pooled_buffers_to_port(MMAL_POOL_T *pool, MMAL_PORT_T *port)
{
    int num = mmal_queue_length(pool->queue);

    int i;
    for (i = 0; i < num; i++) {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool->queue);

        if (!buffer) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                ,_("Unable to get a required buffer %d from pool queue"), i);
            return MMALCAM_ERROR;
        }

        if (mmal_port_send_buffer(port, buffer) != MMAL_SUCCESS) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, _("Unable to send a buffer to port (%d)"), i);
            return MMALCAM_ERROR;
        }
    }

    return MMALCAM_OK;
}

static void destroy_camera_buffer_structures(mmalcam_context_ptr mmalcam)
{
    if (mmalcam->camera_buffer_queue != NULL ) {
        mmal_queue_destroy(mmalcam->camera_buffer_queue);
        mmalcam->camera_buffer_queue = NULL;
    }

    if (mmalcam->camera_buffer_pool != NULL ) {
        mmal_pool_destroy(mmalcam->camera_buffer_pool);
        mmalcam->camera_buffer_pool = NULL;
    }
}

/**
 * mmalcam_start
 *
 *      This routine is called from the main motion thread.  It's job is
 *      to open up the requested camera device via MMAL and do any required
 *      initialization.
 *
 * Parameters:
 *
 *      cnt     Pointer to the motion context structure for this device.
 *
 * Returns:     0 on success
 *              -1 on any failure
 */

int mmalcam_start(struct context *cnt)
{
    mmalcam_context_ptr mmalcam;

    cnt->mmalcam = (mmalcam_context*) mymalloc(sizeof(struct mmalcam_context));
    memset(cnt->mmalcam, 0, sizeof(mmalcam_context));
    mmalcam = cnt->mmalcam;
    mmalcam->cnt = cnt;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
        ,_("MMAL Camera thread starting... for camera (%s) of %d x %d at %d fps")
        ,cnt->conf.mmalcam_name, cnt->conf.width, cnt->conf.height, cnt->conf.framerate);

    mmalcam->camera_parameters = (RASPICAM_CAMERA_PARAMETERS*)malloc(sizeof(RASPICAM_CAMERA_PARAMETERS));
    if (mmalcam->camera_parameters == NULL) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, _("camera params couldn't be allocated"));
        return MMALCAM_ERROR;
    }

    raspicamcontrol_set_defaults(mmalcam->camera_parameters);
    mmalcam->width = cnt->conf.width;
    mmalcam->height = cnt->conf.height;
    mmalcam->framerate = cnt->conf.framerate;

    if (cnt->conf.mmalcam_control_params) {
        parse_camera_control_params(cnt->conf.mmalcam_control_params, mmalcam->camera_parameters);
    }

    cnt->imgs.width = mmalcam->width;
    cnt->imgs.height = mmalcam->height;
    cnt->imgs.size_norm = (mmalcam->width * mmalcam->height * 3) / 2;
    cnt->imgs.motionsize = mmalcam->width * mmalcam->height;

    int retval = create_camera_component(mmalcam, cnt->conf.mmalcam_name);

    if (retval == 0) {
        retval = create_camera_buffer_structures(mmalcam);
    }

    if (retval == 0) {
        if (mmal_port_enable(mmalcam->camera_capture_port, camera_buffer_callback)) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, _("MMAL camera capture port enabling failed"));
            retval = MMALCAM_ERROR;
        }
    }

    if (retval == 0) {
        if (mmal_port_parameter_set_boolean(mmalcam->camera_capture_port, MMAL_PARAMETER_CAPTURE, 1)
                != MMAL_SUCCESS) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, _("MMAL camera capture start failed"));
            retval = MMALCAM_ERROR;
        }
    }

    if (retval == 0) {
        retval = send_pooled_buffers_to_port(mmalcam->camera_buffer_pool, mmalcam->camera_capture_port);
    }

    return retval;
}

/**
 * mmalcam_cleanup
 *
 *      This routine shuts down any MMAL resources, then releases any allocated data
 *      within the mmalcam context and frees the context itself.
 *      This function is also called from motion_init if first time connection
 *      fails and we start retrying until we get a valid first frame from the
 *      camera.
 *
 * Parameters:
 *
 *      mmalcam          Pointer to a mmalcam context
 *
 * Returns:              Nothing.
 *
 */
void mmalcam_cleanup(struct mmalcam_context *mmalcam)
{
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, _("MMAL Camera cleanup"));

    if (mmalcam != NULL ) {
        if (mmalcam->camera_component) {
            check_disable_port(mmalcam->camera_capture_port);
            mmal_component_disable(mmalcam->camera_component);
            destroy_camera_buffer_structures(mmalcam);
            destroy_camera_component(mmalcam);
        }

        if (mmalcam->camera_parameters) {
            free(mmalcam->camera_parameters);
        }

        free(mmalcam);
    }
}

/**
 * mmalcam_next
 *
 *      This routine is called when the main 'motion' thread wants a new
 *      frame of video.  It fetches the most recent frame available from
 *      the Pi camera already in YUV420P, and returns it to motion.
 *
 * Parameters:
 *      cnt             Pointer to the context for this thread
 *      image           Pointer to a buffer for the returned image
 *
 * Returns:             Error code
 */
int mmalcam_next(struct context *cnt,  struct image_data *img_data)
{
    mmalcam_context_ptr mmalcam;

    if ((!cnt) || (!cnt->mmalcam))
        return NETCAM_FATAL_ERROR;

    mmalcam = cnt->mmalcam;

    MMAL_BUFFER_HEADER_T *camera_buffer = mmal_queue_wait(mmalcam->camera_buffer_queue);

    if (camera_buffer->cmd == 0 && (camera_buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END)
            && camera_buffer->length >= cnt->imgs.size_norm) {
        mmal_buffer_header_mem_lock(camera_buffer);
        memcpy(img_data->image_norm, camera_buffer->data, cnt->imgs.size_norm);
        mmal_buffer_header_mem_unlock(camera_buffer);
    } else {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("cmd %d flags %08x size %d/%d at %08x, img_size=%d")
            ,camera_buffer->cmd, camera_buffer->flags, camera_buffer->length
            ,camera_buffer->alloc_size, camera_buffer->data, cnt->imgs.size_norm);
    }

    mmal_buffer_header_release(camera_buffer);

    if (mmalcam->camera_capture_port->is_enabled) {
        MMAL_STATUS_T status;
        MMAL_BUFFER_HEADER_T *new_buffer = mmal_queue_get(mmalcam->camera_buffer_pool->queue);

        if (new_buffer) {
            status = mmal_port_send_buffer(mmalcam->camera_capture_port, new_buffer);
        }

        if (!new_buffer || status != MMAL_SUCCESS)
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                ,_("Unable to return a buffer to the camera video port"));
    }

    rotate_map(cnt,img_data);

    return 0;
}

#endif
