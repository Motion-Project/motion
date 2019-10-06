/*    motion_loop.cpp
 *
 *    This file is part of the Motion application
 *    Copyright (C) 2019  Motion-Project Developers(motion-project.github.io)
 *
 *    This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Library General Public
 *    License as published by the Free Software Foundation; either
 *    version 2 of the License, or (at your option) any later version.
 *
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Library General Public License for more details.
 *
 *    You should have received a copy of the GNU Library General Public
 *    License along with this library; if not, write to the
 *    Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *    Boston, MA  02110-1301, USA.
*/

#include "motion.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "motion_loop.hpp"
#include "rotate.hpp"
#include "movie.hpp"
#include "video_common.hpp"
#include "video_v4l2.hpp"
#include "video_loopback.hpp"
#include "netcam.hpp"
#include "conf.hpp"
#include "alg.hpp"
#include "track.hpp"
#include "event.hpp"
#include "picture.hpp"
#include "rotate.hpp"
#include "webu.hpp"
#include "dbse.hpp"
#include "draw.hpp"
#include "webu_stream.hpp"

#define IMAGE_BUFFER_FLUSH ((unsigned int)-1)


static void mlp_ring_resize(struct ctx_cam *cam, int new_size) {

    int smallest, i;
    struct ctx_image_data *tmp;

    if (cam->event_nr != cam->prev_event) {

        if (new_size < cam->imgs.ring_size){
            smallest = new_size;
        } else {
            smallest = cam->imgs.ring_size;
        }

        if (cam->imgs.ring_in == smallest - 1 || smallest == 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Resizing pre_capture buffer to %d items"), new_size);

            tmp =(struct ctx_image_data*) mymalloc(new_size * sizeof(struct ctx_image_data));

            if (smallest > 0){
                memcpy(tmp, cam->imgs.image_ring, sizeof(struct ctx_image_data) * smallest);
            }

            for(i = smallest; i < new_size; i++) {
                tmp[i].image_norm =(unsigned char*) mymalloc(cam->imgs.size_norm);
                memset(tmp[i].image_norm, 0x80, cam->imgs.size_norm);  /* initialize to grey */
                if (cam->imgs.size_high > 0){
                    tmp[i].image_high =(unsigned char*) mymalloc(cam->imgs.size_high);
                    memset(tmp[i].image_high, 0x80, cam->imgs.size_high);
                }
            }

            free(cam->imgs.image_ring);

            cam->imgs.image_ring = tmp;
            cam->current_image = NULL;

            cam->imgs.ring_size = new_size;

            cam->imgs.ring_in = 0;
            cam->imgs.ring_out = 0;
        }
    }
}

static void mlp_ring_destroy(struct ctx_cam *cam) {
    int i;

    if (cam->imgs.image_ring == NULL) return;

    for (i = 0; i < cam->imgs.ring_size; i++){
        free(cam->imgs.image_ring[i].image_norm);
        if (cam->imgs.size_high >0 ) free(cam->imgs.image_ring[i].image_high);
    }
    free(cam->imgs.image_ring);

    cam->imgs.image_ring = NULL;
    cam->current_image = NULL;
    cam->imgs.ring_size = 0;
}

static void mlp_ring_process_debug(struct ctx_cam *cam){
    char tmp[32];
    const char *t;

    if (cam->imgs.image_ring[cam->imgs.ring_out].flags & IMAGE_TRIGGER){
        t = "Trigger";
    } else if (cam->imgs.image_ring[cam->imgs.ring_out].flags & IMAGE_MOTION){
        t = "Motion";
    } else if (cam->imgs.image_ring[cam->imgs.ring_out].flags & IMAGE_PRECAP){
        t = "Precap";
    } else if (cam->imgs.image_ring[cam->imgs.ring_out].flags & IMAGE_POSTCAP){
        t = "Postcap";
    } else {
        t = "Other";
    }

    mystrftime(cam, tmp, sizeof(tmp), "%H%M%S-%q",
                &cam->imgs.image_ring[cam->imgs.ring_out].imgts, NULL, 0);
    draw_text(cam->imgs.image_ring[cam->imgs.ring_out].image_norm,
                cam->imgs.width, cam->imgs.height, 10, 20, tmp, cam->text_scale);
    draw_text(cam->imgs.image_ring[cam->imgs.ring_out].image_norm,
                cam->imgs.width, cam->imgs.height, 10, 30, t, cam->text_scale);

}

static void mlp_ring_process(struct ctx_cam *cam, unsigned int max_images) {

    struct ctx_image_data *saved_current_image = cam->current_image;

    do {
        if ((cam->imgs.image_ring[cam->imgs.ring_out].flags & (IMAGE_SAVE | IMAGE_SAVED)) != IMAGE_SAVE){
            break;
        }

        cam->current_image = &cam->imgs.image_ring[cam->imgs.ring_out];

        if (cam->imgs.image_ring[cam->imgs.ring_out].shot < cam->conf.framerate) {
            if (cam->log_level >= DBG) mlp_ring_process_debug(cam);

            event(cam, EVENT_IMAGE_DETECTED,
              &cam->imgs.image_ring[cam->imgs.ring_out], NULL, NULL,
              &cam->imgs.image_ring[cam->imgs.ring_out].imgts);

            if (cam->movie_last_shot >= 0){
                cam->movie_last_shot = cam->imgs.image_ring[cam->imgs.ring_out].shot;
            }
        }

        cam->imgs.image_ring[cam->imgs.ring_out].flags |= IMAGE_SAVED;

        if (cam->imgs.image_ring[cam->imgs.ring_out].flags & IMAGE_MOTION) {
            if (cam->new_img & NEWIMG_BEST) {
                if (cam->imgs.image_ring[cam->imgs.ring_out].diffs > cam->imgs.image_preview.diffs) {
                    pic_save_preview(cam, &cam->imgs.image_ring[cam->imgs.ring_out]);
                }
            }
            if (cam->new_img & NEWIMG_CENTER) {
                if (cam->imgs.image_ring[cam->imgs.ring_out].cent_dist < cam->imgs.image_preview.cent_dist) {
                    pic_save_preview(cam, &cam->imgs.image_ring[cam->imgs.ring_out]);
                }
            }
        }

        if (++cam->imgs.ring_out >= cam->imgs.ring_size) {
            cam->imgs.ring_out = 0;
        }

        /* TODO mlp_action requests this....only wants two images...but why?*/
        if (max_images != IMAGE_BUFFER_FLUSH) {
            max_images--;
            if (max_images == 0) break;
        }
    } while (cam->imgs.ring_out != cam->imgs.ring_in);

    cam->current_image = saved_current_image;
}

static void mlp_detected_trigger(struct ctx_cam *cam, struct ctx_image_data *img) {

    if (img->flags & IMAGE_TRIGGER) {
        if (cam->event_nr != cam->prev_event) {

            cam->prev_event = cam->event_nr;
            cam->eventtime = img->imgts.tv_sec;

            mystrftime(cam, cam->text_event_string, sizeof(cam->text_event_string),
                       cam->conf.text_event, &img->imgts, NULL, 0);

            event(cam, EVENT_FIRSTMOTION, img, NULL, NULL,
                &cam->imgs.image_ring[cam->imgs.ring_out].imgts);

            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motion detected - starting event %d"),
                       cam->event_nr);

            if (cam->new_img & (NEWIMG_FIRST | NEWIMG_BEST | NEWIMG_CENTER)){
                pic_save_preview(cam, img);
            }

        }

        event(cam, EVENT_MOTION, NULL, NULL, NULL, &img->imgts);
    }

}

static void mlp_detected(struct ctx_cam *cam, int dev, struct ctx_image_data *img) {
    struct ctx_config *conf = &cam->conf;
    unsigned int distX, distY;

    draw_locate(cam, img);

    /* Calculate how centric motion is if configured preview center*/
    if (cam->new_img & NEWIMG_CENTER) {
        distX = abs((cam->imgs.width / 2) - img->location.x );
        distY = abs((cam->imgs.height / 2) - img->location.y);
        img->cent_dist = distX * distX + distY * distY;
    }

    mlp_detected_trigger(cam, img);

    if (img->shot < conf->framerate) {
        if (conf->stream_motion && !conf->setup_mode && img->shot != 1){
            event(cam, EVENT_STREAM, img, NULL, NULL, &img->imgts);
        }
        if (conf->picture_output_motion){
            event(cam, EVENT_IMAGEM_DETECTED, NULL, NULL, NULL, &img->imgts);
        }
    }

    if (cam->track.type && cam->track.active){
        cam->frame_skip = track_move(cam, dev, &img->location, &cam->imgs, 0);
    }

}


static int init_camera_type(struct ctx_cam *cam){

    cam->camera_type = CAMERA_TYPE_UNKNOWN;

    if (cam->conf.mmalcam_name) {
        cam->camera_type = CAMERA_TYPE_MMAL;
        return 0;
    }

    if (cam->conf.netcam_url) {
        if ((strncmp(cam->conf.netcam_url,"mjpeg",5) == 0) ||
            (strncmp(cam->conf.netcam_url,"http" ,4) == 0) ||
            (strncmp(cam->conf.netcam_url,"v4l2" ,4) == 0) ||
            (strncmp(cam->conf.netcam_url,"file" ,4) == 0) ||
            (strncmp(cam->conf.netcam_url,"rtmp" ,4) == 0) ||
            (strncmp(cam->conf.netcam_url,"rtsp" ,4) == 0)) {
            cam->camera_type = CAMERA_TYPE_NETCAM;
        }
        return 0;
    }

    if (cam->conf.video_device) {
        cam->camera_type = CAMERA_TYPE_V4L2;
        return 0;
    }

    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
        , _("Unable to determine camera type (MMAL, Netcam, V4L2)"));
    return -1;

}

/** Get first images from camera at startup */
static void mlp_init_firstimage(struct ctx_cam *cam) {

    int indx;

    /* Capture first image, or we will get an alarm on start */
    if (cam->video_dev >= 0) {
        for (indx = 0; indx < 5; indx++) {
            if (vid_next(cam, cam->current_image) == 0)
                break;
            SLEEP(2, 0);
        }

        if (indx >= 5) {
            memset(cam->imgs.image_virgin, 0x80, cam->imgs.size_norm);       /* initialize to grey */
            draw_text(cam->imgs.image_virgin, cam->imgs.width, cam->imgs.height,
                      10, 20, "Error capturing first image", cam->text_scale);
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error capturing first image"));
        }
    }
    cam->current_image = &cam->imgs.image_ring[cam->imgs.ring_in];

    alg_update_reference_frame(cam, RESET_REF_FRAME);

}

/** Validate the config parms for height and width */
static void mlp_init_szconf(struct ctx_cam *cam){

    if ((cam->conf.height == 0) || (cam->conf.width == 0)) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Invalid configuration dimensions %dx%d"),cam->conf.height,cam->conf.width);
        cam->conf.height = DEF_HEIGHT;
        cam->conf.width = DEF_WIDTH;
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Using default dimensions %dx%d"),cam->conf.height,cam->conf.width);
    }
    if (cam->conf.width % 8) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image width (%d) requested is not modulo 8."), cam->conf.width);
        cam->conf.width = cam->conf.width - (cam->conf.width % 8) + 8;
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Adjusting width to next higher multiple of 8 (%d)."), cam->conf.width);
    }
    if (cam->conf.height % 8) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image height (%d) requested is not modulo 8."), cam->conf.height);
        cam->conf.height = cam->conf.height - (cam->conf.height % 8) + 8;
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Adjusting height to next higher multiple of 8 (%d)."), cam->conf.height);
    }
    if (cam->conf.width  < 64){
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Adjusting to minimum picture width of 64."));
        cam->conf.width  = 64;
    }
    if (cam->conf.height < 64){
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Adjusting to minimum picture height of 64."));
        cam->conf.height = 64;
    }

}

/** Check the image size to determine if modulo 8 and over 64 */
static int mlp_check_szimg(struct ctx_cam *cam){

    /* Revalidate we got a valid image size */
    if ((cam->imgs.width % 8) || (cam->imgs.height % 8)) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image width (%d) or height(%d) requested is not modulo 8.")
            ,cam->imgs.width, cam->imgs.height);
        return -3;
    }
    if ((cam->imgs.width  < 64) || (cam->imgs.height < 64)){
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Motion only supports width and height greater than or equal to 64 %dx%d")
            ,cam->imgs.width, cam->imgs.height);
            return -3;
    }
    /* Substream size notification*/
    if ((cam->imgs.width % 16) || (cam->imgs.height % 16)) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Substream not available.  Image sizes not modulo 16."));
    }

    return 0;

}

/** Set the items required for the area detect */
static void mlp_init_areadetect(struct ctx_cam *cam){

    /* Initialize area detection */
    cam->area_minx[0] = cam->area_minx[3] = cam->area_minx[6] = 0;
    cam->area_miny[0] = cam->area_miny[1] = cam->area_miny[2] = 0;

    cam->area_minx[1] = cam->area_minx[4] = cam->area_minx[7] = cam->imgs.width / 3;
    cam->area_maxx[0] = cam->area_maxx[3] = cam->area_maxx[6] = cam->imgs.width / 3;

    cam->area_minx[2] = cam->area_minx[5] = cam->area_minx[8] = cam->imgs.width / 3 * 2;
    cam->area_maxx[1] = cam->area_maxx[4] = cam->area_maxx[7] = cam->imgs.width / 3 * 2;

    cam->area_miny[3] = cam->area_miny[4] = cam->area_miny[5] = cam->imgs.height / 3;
    cam->area_maxy[0] = cam->area_maxy[1] = cam->area_maxy[2] = cam->imgs.height / 3;

    cam->area_miny[6] = cam->area_miny[7] = cam->area_miny[8] = cam->imgs.height / 3 * 2;
    cam->area_maxy[3] = cam->area_maxy[4] = cam->area_maxy[5] = cam->imgs.height / 3 * 2;

    cam->area_maxx[2] = cam->area_maxx[5] = cam->area_maxx[8] = cam->imgs.width;
    cam->area_maxy[6] = cam->area_maxy[7] = cam->area_maxy[8] = cam->imgs.height;

    cam->areadetect_eventnbr = 0;

}

/** Allocate the required buffers */
static void mlp_init_buffers(struct ctx_cam *cam){

    cam->imgs.ref =(unsigned char*) mymalloc(cam->imgs.size_norm);
    cam->imgs.image_motion.image_norm = (unsigned char*)mymalloc(cam->imgs.size_norm);
    cam->imgs.ref_dyn =(int*) mymalloc(cam->imgs.motionsize * sizeof(*cam->imgs.ref_dyn));
    cam->imgs.image_virgin =(unsigned char*) mymalloc(cam->imgs.size_norm);
    cam->imgs.image_vprvcy = (unsigned char*)mymalloc(cam->imgs.size_norm);
    cam->imgs.smartmask =(unsigned char*) mymalloc(cam->imgs.motionsize);
    cam->imgs.smartmask_final =(unsigned char*) mymalloc(cam->imgs.motionsize);
    cam->imgs.smartmask_buffer =(int*) mymalloc(cam->imgs.motionsize * sizeof(*cam->imgs.smartmask_buffer));
    cam->imgs.labels =(int*)mymalloc(cam->imgs.motionsize * sizeof(*cam->imgs.labels));
    cam->imgs.labelsize =(int*) mymalloc((cam->imgs.motionsize/2+1) * sizeof(*cam->imgs.labelsize));
    cam->imgs.image_preview.image_norm =(unsigned char*) mymalloc(cam->imgs.size_norm);
    cam->imgs.common_buffer =(unsigned char*) mymalloc(3 * cam->imgs.width * cam->imgs.height);
    if (cam->imgs.size_high > 0){
        cam->imgs.image_preview.image_high =(unsigned char*) mymalloc(cam->imgs.size_high);
    }

}

/** mlp_init */
static int mlp_init(struct ctx_cam *cam) {

    mythreadname_set("ml",cam->threadnr,cam->conf.camera_name);

    pthread_setspecific(tls_key_threadnr, (void *)((unsigned long)cam->threadnr));

    clock_gettime(CLOCK_REALTIME, &cam->frame_last_ts);
    clock_gettime(CLOCK_REALTIME, &cam->frame_curr_ts);

    cam->smartmask_speed = 0;

    /*
     * We initialize cam->event_nr to 1 and cam->prev_event to 0 (not really needed) so
     * that certain code below does not run until motion has been detected the first time
     */
    cam->event_nr = 1;
    cam->prev_event = 0;
    cam->lightswitch_framecounter = 0;
    cam->detecting_motion = 0;
    cam->event_user = FALSE;
    cam->event_stop = FALSE;

    /* Make sure to default the high res to zero */
    cam->imgs.width_high = 0;
    cam->imgs.height_high = 0;
    cam->imgs.size_high = 0;
    cam->movie_passthrough = cam->conf.movie_passthrough;

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
        ,_("Camera %d started: motion detection %s"),
        cam->camera_id, cam->pause ? _("Disabled"):_("Enabled"));

    if (!cam->conf.target_dir)
        cam->conf.target_dir = mystrdup(".");

    if (init_camera_type(cam) != 0 ) return -3;

    if ((cam->camera_type != CAMERA_TYPE_NETCAM) &&
        (cam->movie_passthrough)) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Pass-through processing disabled."));
        cam->movie_passthrough = FALSE;
    }

    mlp_init_szconf(cam);

    cam->video_dev = vid_start(cam);

    if (cam->video_dev == -1) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Could not fetch initial image from camera "));
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Motion continues using width and height from config file(s)"));
        cam->imgs.width = cam->conf.width;
        cam->imgs.height = cam->conf.height;
        cam->imgs.size_norm = cam->conf.width * cam->conf.height * 3 / 2;
        cam->imgs.motionsize = cam->conf.width * cam->conf.height;
    } else if (cam->video_dev == -2) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Could not fetch initial image from camera "));
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Motion only supports width and height modulo 8"));
        return -3;
    } else {
        cam->imgs.motionsize = (cam->imgs.width * cam->imgs.height);
        cam->imgs.size_norm  = (cam->imgs.width * cam->imgs.height * 3) / 2;
        cam->imgs.size_high  = (cam->imgs.width_high * cam->imgs.height_high * 3) / 2;
    }

    if (mlp_check_szimg(cam) != 0) return -1;

    mlp_ring_resize(cam, 1); /* Create a initial precapture ring buffer with 1 frame */

    mlp_init_buffers(cam);

    webu_stream_init(cam);

    rotate_init(cam);

    draw_init_scale(cam);

    mlp_init_firstimage(cam);

    vlp_init(cam);

    dbse_init(cam);

    pic_init_mask(cam);

    pic_init_privacy(cam);

    /* Always initialize smart_mask - someone could turn it on later... */
    memset(cam->imgs.smartmask, 0, cam->imgs.motionsize);
    memset(cam->imgs.smartmask_final, 255, cam->imgs.motionsize);
    memset(cam->imgs.smartmask_buffer, 0, cam->imgs.motionsize * sizeof(*cam->imgs.smartmask_buffer));

    /* Set noise level */
    cam->noise = cam->conf.noise_level;

    /* Set threshold value */
    cam->threshold = cam->conf.threshold;
    if (cam->conf.threshold_maximum > cam->conf.threshold ){
        cam->threshold_maximum = cam->conf.threshold_maximum;
    } else {
        cam->threshold_maximum = (cam->imgs.height * cam->imgs.width * 3) / 2;
    }

    track_init(cam);
    cam->frame_skip = 8;

    /* Work out expected frame rate based on config setting */
    if (cam->conf.framerate < 2)
        cam->conf.framerate = 2;

    /* 2 sec startup delay so FPS is calculated correct */
    cam->startup_frames = (cam->conf.framerate * 2) + cam->conf.pre_capture + cam->conf.minimum_motion_frames;

    cam->required_frame_time = 1000000L / cam->conf.framerate;

    cam->frame_delay = cam->required_frame_time;

    cam->track_posx = 0;
    cam->track_posy = 0;
    if (cam->track.type)
        cam->frame_skip = track_center(cam, cam->video_dev, 0, 0, 0);

    mlp_init_areadetect(cam);

    cam->timenow = 0;
    cam->timebefore = 0;
    cam->rate_limit = 0;
    cam->lastframetime = 0;
    cam->minimum_frame_time_downcounter = cam->conf.minimum_frame_time;
    cam->get_image = 1;

    cam->olddiffs = 0;
    cam->smartmask_ratio = 0;
    cam->smartmask_count = 20;

    cam->previous_diffs = 0;
    cam->previous_location_x = 0;
    cam->previous_location_y = 0;

    cam->time_last_frame = 1;
    cam->time_current_frame = 0;

    cam->smartmask_lastrate = 0;

    cam->passflag = 0;  //only purpose to flag first frame
    cam->rolling_frame = 0;

    if (cam->conf.emulate_motion) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("Emulating motion"));
    }

    return 0;
}

/** clean up all memory etc. from motion init */
void mlp_cleanup(struct ctx_cam *cam) {

    event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, NULL);
    event(cam, EVENT_ENDMOTION, NULL, NULL, NULL, NULL);

    webu_stream_deinit(cam);

    if (cam->video_dev >= 0) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("Calling vid_close() from mlp_cleanup"));
        vid_close(cam);
    }

    free(cam->imgs.image_motion.image_norm);
    cam->imgs.image_motion.image_norm = NULL;

    free(cam->imgs.ref);
    cam->imgs.ref = NULL;

    free(cam->imgs.ref_dyn);
    cam->imgs.ref_dyn = NULL;

    free(cam->imgs.image_virgin);
    cam->imgs.image_virgin = NULL;

    free(cam->imgs.image_vprvcy);
    cam->imgs.image_vprvcy = NULL;

    free(cam->imgs.labels);
    cam->imgs.labels = NULL;

    free(cam->imgs.labelsize);
    cam->imgs.labelsize = NULL;

    free(cam->imgs.smartmask);
    cam->imgs.smartmask = NULL;

    free(cam->imgs.smartmask_final);
    cam->imgs.smartmask_final = NULL;

    free(cam->imgs.smartmask_buffer);
    cam->imgs.smartmask_buffer = NULL;

    if (cam->imgs.mask) free(cam->imgs.mask);
    cam->imgs.mask = NULL;

    if (cam->imgs.mask_privacy) free(cam->imgs.mask_privacy);
    cam->imgs.mask_privacy = NULL;

    if (cam->imgs.mask_privacy_uv) free(cam->imgs.mask_privacy_uv);
    cam->imgs.mask_privacy_uv = NULL;

    if (cam->imgs.mask_privacy_high) free(cam->imgs.mask_privacy_high);
    cam->imgs.mask_privacy_high = NULL;

    if (cam->imgs.mask_privacy_high_uv) free(cam->imgs.mask_privacy_high_uv);
    cam->imgs.mask_privacy_high_uv = NULL;

    free(cam->imgs.common_buffer);
    cam->imgs.common_buffer = NULL;

    free(cam->imgs.image_preview.image_norm);
    cam->imgs.image_preview.image_norm = NULL;

    if (cam->imgs.size_high > 0){
        free(cam->imgs.image_preview.image_high);
        cam->imgs.image_preview.image_high = NULL;
    }

    mlp_ring_destroy(cam); /* Cleanup the precapture ring buffer */

    rotate_deinit(cam); /* cleanup image rotation data */

    if (cam->pipe != -1) {
        close(cam->pipe);
        cam->pipe = -1;
    }

    if (cam->mpipe != -1) {
        close(cam->mpipe);
        cam->mpipe = -1;
    }

    dbse_deinit(cam);

}

static void mlp_mask_privacy(struct ctx_cam *cam){

    if (cam->imgs.mask_privacy == NULL) return;

    /*
    * This function uses long operations to process 4 (32 bit) or 8 (64 bit)
    * bytes at a time, providing a significant boost in performance.
    * Then a trailer loop takes care of any remaining bytes.
    */
    unsigned char *image;
    const unsigned char *mask;
    const unsigned char *maskuv;

    int index_y;
    int index_crcb;
    int increment;
    int indx_img;                /* Counter for how many images we need to apply the mask to */
    int indx_max;                /* 1 if we are only doing norm, 2 if we are doing both norm and high */

    indx_img = 1;
    indx_max = 1;
    if (cam->imgs.size_high > 0) indx_max = 2;
    increment = sizeof(unsigned long);

    while (indx_img <= indx_max){
        if (indx_img == 1) {
            /* Normal Resolution */
            index_y = cam->imgs.height * cam->imgs.width;
            image = cam->current_image->image_norm;
            mask = cam->imgs.mask_privacy;
            index_crcb = cam->imgs.size_norm - index_y;
            maskuv = cam->imgs.mask_privacy_uv;
        } else {
            /* High Resolution */
            index_y = cam->imgs.height_high * cam->imgs.width_high;
            image = cam->current_image->image_high;
            mask = cam->imgs.mask_privacy_high;
            index_crcb = cam->imgs.size_high - index_y;
            maskuv = cam->imgs.mask_privacy_high_uv;
        }

        while (index_y >= increment) {
            *((unsigned long *)image) &= *((unsigned long *)mask);
            image += increment;
            mask += increment;
            index_y -= increment;
        }
        while (--index_y >= 0) {
            *(image++) &= *(mask++);
        }

        /* Mask chrominance. */
        while (index_crcb >= increment) {
            index_crcb -= increment;
            /*
            * Replace the masked bytes with 0x080. This is done using two masks:
            * the normal privacy mask is used to clear the masked bits, the
            * "or" privacy mask is used to write 0x80. The benefit of that method
            * is that we process 4 or 8 bytes in just two operations.
            */
            *((unsigned long *)image) &= *((unsigned long *)mask);
            mask += increment;
            *((unsigned long *)image) |= *((unsigned long *)maskuv);
            maskuv += increment;
            image += increment;
        }

        while (--index_crcb >= 0) {
            if (*(mask++) == 0x00) *image = 0x80; // Mask last remaining bytes.
            image += 1;
        }

        indx_img++;
    }
}

static void mlp_areadetect(struct ctx_cam *cam){
    int i, j, z = 0;
    /*
     * Simple hack to recognize motion in a specific area
     * Do we need a new coversion specifier as well??
     */
    if ((cam->conf.area_detect) &&
        (cam->event_nr != cam->areadetect_eventnbr) &&
        (cam->current_image->flags & IMAGE_TRIGGER)) {
        j = strlen(cam->conf.area_detect);
        for (i = 0; i < j; i++) {
            z = cam->conf.area_detect[i] - 49; /* characters are stored as ascii 48-57 (0-9) */
            if ((z >= 0) && (z < 9)) {
                if (cam->current_image->location.x > cam->area_minx[z] &&
                    cam->current_image->location.x < cam->area_maxx[z] &&
                    cam->current_image->location.y > cam->area_miny[z] &&
                    cam->current_image->location.y < cam->area_maxy[z]) {
                    event(cam, EVENT_AREA_DETECTED, NULL, NULL, NULL, &cam->current_image->imgts);
                    cam->areadetect_eventnbr = cam->event_nr; /* Fire script only once per event */
                    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO
                        ,_("Motion in area %d detected."), z + 1);
                    break;
                }
            }
        }
    }

}

static void mlp_prepare(struct ctx_cam *cam){

    int frame_buffer_size;

    cam->watchdog = WATCHDOG_TMO;

    cam->timebefore = cam->timenow;

    cam->frame_last_ts.tv_sec = cam->frame_curr_ts.tv_sec;
    cam->frame_last_ts.tv_nsec = cam->frame_curr_ts.tv_nsec;
    clock_gettime(CLOCK_REALTIME, &cam->frame_curr_ts);

    /*
     * Calculate detection rate limit. Above 5fps we limit the detection
     * rate to 3fps to reduce load at higher framerates.
     */
    cam->process_thisframe = 0;
    cam->rate_limit++;
    if (cam->rate_limit >= (cam->lastrate / 3)) {
        cam->rate_limit = 0;
        cam->process_thisframe = 1;
    }

    if (cam->conf.minimum_motion_frames < 1)
        cam->conf.minimum_motion_frames = 1;

    if (cam->conf.pre_capture < 0)
        cam->conf.pre_capture = 0;

    frame_buffer_size = cam->conf.pre_capture + cam->conf.minimum_motion_frames;

    if (cam->imgs.ring_size != frame_buffer_size)
        mlp_ring_resize(cam, frame_buffer_size);

    /*
     * If we have started on a new second we reset the shots variable
     * lastrate is updated to be the number of the last frame. last rate
     * is used as the ffmpeg framerate when motion is detected.
     */
    if (cam->frame_last_ts.tv_sec != cam->frame_curr_ts.tv_sec) {
        cam->lastrate = cam->shots + 1;
        cam->shots = -1;
        cam->lastframetime = cam->currenttime;

        if (cam->conf.minimum_frame_time) {
            cam->minimum_frame_time_downcounter--;
            if (cam->minimum_frame_time_downcounter == 0)
                cam->get_image = 1;
        } else {
            cam->get_image = 1;
        }
    }


    /* Increase the shots variable for each frame captured within this second */
    cam->shots++;

    if (cam->startup_frames > 0)
        cam->startup_frames--;


}

static void mlp_resetimages(struct ctx_cam *cam){

    struct ctx_image_data *old_image;

    if (cam->conf.minimum_frame_time) {
        cam->minimum_frame_time_downcounter = cam->conf.minimum_frame_time;
        cam->get_image = 0;
    }

    /* ring_buffer_in is pointing to current pos, update before put in a new image */
    if (++cam->imgs.ring_in >= cam->imgs.ring_size)
        cam->imgs.ring_in = 0;

    /* Check if we have filled the ring buffer, throw away last image */
    if (cam->imgs.ring_in == cam->imgs.ring_out) {
        if (++cam->imgs.ring_out >= cam->imgs.ring_size)
            cam->imgs.ring_out = 0;
    }

    /* cam->current_image points to position in ring where to store image, diffs etc. */
    old_image = cam->current_image;
    cam->current_image = &cam->imgs.image_ring[cam->imgs.ring_in];

    /* Init/clear current_image */
    if (cam->process_thisframe) {
        /* set diffs to 0 now, will be written after we calculated diffs in new image */
        cam->current_image->diffs = 0;

        /* Set flags to 0 */
        cam->current_image->flags = 0;
        cam->current_image->cent_dist = 0;

        /* Clear location data */
        memset(&cam->current_image->location, 0, sizeof(cam->current_image->location));
        cam->current_image->total_labels = 0;
    } else if (cam->current_image && old_image) {
        /* not processing this frame: save some important values for next image */
        cam->current_image->diffs = old_image->diffs;
        cam->current_image->imgts = old_image->imgts;
        cam->current_image->shot = old_image->shot;
        cam->current_image->cent_dist = old_image->cent_dist;
        cam->current_image->flags = old_image->flags & (~IMAGE_SAVED);
        cam->current_image->location = old_image->location;
        cam->current_image->total_labels = old_image->total_labels;
    }

    clock_gettime(CLOCK_REALTIME, &cam->current_image->imgts);

    /* Store shot number with pre_captured image */
    cam->current_image->shot = cam->shots;

}

static int mlp_retry(struct ctx_cam *cam){

    /*
     * If a camera is not available we keep on retrying every 10 seconds
     * until it shows up.
     */
    int size_high;

    if (cam->video_dev < 0 &&
        cam->currenttime % 10 == 0 && cam->shots == 0) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Retrying until successful connection with camera"));
        cam->video_dev = vid_start(cam);

        if (cam->video_dev < 0) {
            return 1;
        }

        if (mlp_check_szimg(cam) != 0) return 1;

        /*
         * If the netcam has different dimensions than in the config file
         * we need to restart Motion to re-allocate all the buffers
         */
        if (cam->imgs.width != cam->conf.width || cam->imgs.height != cam->conf.height) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Camera has finally become available\n"
                       "Camera image has different width and height"
                       "from what is in the config file. You should fix that\n"
                       "Restarting Motion thread to reinitialize all "
                       "image buffers to new picture dimensions"));
            cam->conf.width = cam->imgs.width;
            cam->conf.height = cam->imgs.height;
            /*
             * Break out of main loop terminating thread
             * watchdog will start us again
             */
            return 1;
        }
        /*
         * For high res, we check the size of buffer to determine whether to break out
         * the init_motion function allocated the buffer for high using the cam->imgs.size_high
         * and the vid_start ONLY re-populates the height/width so we can check the size here.
         */
        size_high = (cam->imgs.width_high * cam->imgs.height_high * 3) / 2;
        if (cam->imgs.size_high != size_high) return 1;
    }
    return 0;
}

static int mlp_capture(struct ctx_cam *cam){

    const char *tmpin;
    char tmpout[80];
    int vid_return_code = 0;        /* Return code used when calling vid_next */
    struct timespec ts1;

    /***** MOTION LOOP - IMAGE CAPTURE SECTION *****/
    /*
     * Fetch next frame from camera
     * If vid_next returns 0 all is well and we got a new picture
     * Any non zero value is an error.
     * 0 = OK, valid picture
     * <0 = fatal error - leave the thread by breaking out of the main loop
     * >0 = non fatal error - copy last image or show grey image with message
     */
    if (cam->video_dev >= 0)
        vid_return_code = vid_next(cam, cam->current_image);
    else
        vid_return_code = 1; /* Non fatal error */

    // VALID PICTURE
    if (vid_return_code == 0) {
        cam->lost_connection = 0;
        cam->connectionlosttime = 0;

        /* If all is well reset missing_frame_counter */
        if (cam->missing_frame_counter >= MISSING_FRAMES_TIMEOUT * cam->conf.framerate) {
            /* If we previously logged starting a grey image, now log video re-start */
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Video signal re-acquired"));
            // event for re-acquired video signal can be called here
            event(cam, EVENT_CAMERA_FOUND, NULL, NULL, NULL, NULL);
        }
        cam->missing_frame_counter = 0;

        /*
         * Save the newly captured still virgin image to a buffer
         * which we will not alter with text and location graphics
         */
        memcpy(cam->imgs.image_virgin, cam->current_image->image_norm, cam->imgs.size_norm);

        mlp_mask_privacy(cam);

        memcpy(cam->imgs.image_vprvcy, cam->current_image->image_norm, cam->imgs.size_norm);

    // FATAL ERROR - leave the thread by breaking out of the main loop
    } else if (vid_return_code < 0) {
        /* Fatal error - Close video device */
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Video device fatal error - Closing video device"));
        vid_close(cam);
        /*
         * Use virgin image, if we are not able to open it again next loop
         * a gray image with message is applied
         * flag lost_connection
         */
        memcpy(cam->current_image->image_norm, cam->imgs.image_virgin, cam->imgs.size_norm);
        cam->lost_connection = 1;
    /* NO FATAL ERROR -
    *        copy last image or show grey image with message
    *        flag on lost_connection if :
    *               vid_return_code == NETCAM_RESTART_ERROR
    *        cam->video_dev < 0
    *        cam->missing_frame_counter > (MISSING_FRAMES_TIMEOUT * cam->conf.framerate)
    */
    } else {

        //MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "vid_return_code %d",vid_return_code);

        /*
         * Netcams that change dimensions while Motion is running will
         * require that Motion restarts to reinitialize all the many
         * buffers inside Motion. It will be a mess to try and recover any
         * other way
         */
        if (vid_return_code == NETCAM_RESTART_ERROR) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Restarting Motion thread to reinitialize all "
                "image buffers"));
            /*
             * Break out of main loop terminating thread
             * watchdog will start us again
             * Set lost_connection flag on
             */
            cam->lost_connection = 1;
            return 1;
        }

        /*
         * First missed frame - store timestamp
         * Don't reset time when thread restarts
         */
        if (cam->connectionlosttime == 0){
            cam->connectionlosttime = cam->currenttime;
        }


        /*
         * Increase missing_frame_counter
         * The first MISSING_FRAMES_TIMEOUT seconds we copy previous virgin image
         * After MISSING_FRAMES_TIMEOUT seconds we put a grey error image in the buffer
         * If we still have not yet received the initial image from a camera
         * we go straight for the grey error image.
         */
        ++cam->missing_frame_counter;

        if (cam->video_dev >= 0 &&
            cam->missing_frame_counter < (MISSING_FRAMES_TIMEOUT * cam->conf.framerate)) {
            memcpy(cam->current_image->image_norm, cam->imgs.image_vprvcy, cam->imgs.size_norm);
        } else {
            cam->lost_connection = 1;

            if (cam->video_dev >= 0)
                tmpin = "CONNECTION TO CAMERA LOST\\nSINCE %Y-%m-%d %T";
            else
                tmpin = "UNABLE TO OPEN VIDEO DEVICE\\nSINCE %Y-%m-%d %T";

            ts1.tv_sec=cam->connectionlosttime;
            ts1.tv_nsec = 0;
            memset(cam->current_image->image_norm, 0x80, cam->imgs.size_norm);
            mystrftime(cam, tmpout, sizeof(tmpout), tmpin, &ts1, NULL, 0);
            draw_text(cam->current_image->image_norm, cam->imgs.width, cam->imgs.height,
                      10, 20 * cam->text_scale, tmpout, cam->text_scale);

            /* Write error message only once */
            if (cam->missing_frame_counter == MISSING_FRAMES_TIMEOUT * cam->conf.framerate) {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                    ,_("Video signal lost - Adding grey image"));
                // Event for lost video signal can be called from here
                event(cam, EVENT_CAMERA_LOST, NULL, NULL, NULL, &ts1);
            }

            /*
             * If we don't get a valid frame for a long time, try to close/reopen device
             * Only try this when a device is open
             */
            if ((cam->video_dev > 0) &&
                (cam->missing_frame_counter == (MISSING_FRAMES_TIMEOUT * 4) * cam->conf.framerate)) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Video signal still lost - "
                    "Trying to close video device"));
                vid_close(cam);
            }
        }
    }
    return 0;

}

static void mlp_detection(struct ctx_cam *cam){

    if (cam->process_thisframe) {
        if (cam->threshold && !cam->pause) {
            /*
             * If we've already detected motion and we want to see if there's
             * still motion, don't bother trying the fast one first. IF there's
             * motion, the alg_diff will trigger alg_diff_standard
             * anyway
             */
            if (cam->detecting_motion || cam->conf.setup_mode)
                cam->current_image->diffs = alg_diff_standard(cam, cam->imgs.image_vprvcy);
            else
                cam->current_image->diffs = alg_diff(cam, cam->imgs.image_vprvcy);

            /* Lightswitch feature - has light intensity changed?
             * This can happen due to change of light conditions or due to a sudden change of the camera
             * sensitivity. If alg_lightswitch detects lightswitch we suspend motion detection the next
             * 'lightswitch_frames' frames to allow the camera to settle.
             * Don't check if we have lost connection, we detect "Lost signal" frame as lightswitch
             */
            if (cam->conf.lightswitch_percent > 1 && !cam->lost_connection) {
                if (alg_lightswitch(cam, cam->current_image->diffs)) {
                    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("Lightswitch detected"));

                    if (cam->conf.lightswitch_frames < 1)
                        cam->conf.lightswitch_frames = 1;
                    else if (cam->conf.lightswitch_frames > 1000)
                        cam->conf.lightswitch_frames = 1000;

                    if (cam->frame_skip < (unsigned int)cam->conf.lightswitch_frames)
                        cam->frame_skip = (unsigned int)cam->conf.lightswitch_frames;

                    cam->current_image->diffs = 0;
                    alg_update_reference_frame(cam, RESET_REF_FRAME);
                }
            }

            /*
             * Switchfilter feature tries to detect a change in the video signal
             * from one camera to the next. This is normally used in the Round
             * Robin feature. The algorithm is not very safe.
             * The algorithm takes a little time so we only call it when needed
             * ie. when feature is enabled and diffs>threshold.
             * We do not suspend motion detection like we did for lightswitch
             * because with Round Robin this is controlled by roundrobin_skip.
             */
            if (cam->conf.roundrobin_switchfilter && cam->current_image->diffs > cam->threshold) {
                cam->current_image->diffs = alg_switchfilter(cam, cam->current_image->diffs,
                                                             cam->current_image->image_norm);

                if ((cam->current_image->diffs <= cam->threshold) ||
                    (cam->current_image->diffs > cam->threshold_maximum)) {

                    cam->current_image->diffs = 0;
                    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("Switchfilter detected"));
                }
            }

            /*
             * Despeckle feature
             * First we run (as given by the despeckle_filter option iterations
             * of erode and dilate algorithms.
             * Finally we run the labelling feature.
             * All this is done in the alg_despeckle code.
             */
            cam->current_image->total_labels = 0;
            cam->imgs.largest_label = 0;
            cam->olddiffs = 0;

            if (cam->conf.despeckle_filter && cam->current_image->diffs > 0) {
                cam->olddiffs = cam->current_image->diffs;
                cam->current_image->diffs = alg_despeckle(cam, cam->olddiffs);
            } else if (cam->imgs.labelsize_max) {
                cam->imgs.labelsize_max = 0; /* Disable labeling if enabled */
            }

        } else if (!cam->conf.setup_mode) {
            cam->current_image->diffs = 0;
        }
    }

    //TODO:  This section needs investigation for purpose, cause and effect
    /* Manipulate smart_mask sensitivity (only every smartmask_ratio seconds) */
    if ((cam->smartmask_speed && (cam->event_nr != cam->prev_event)) &&
        (!--cam->smartmask_count)) {
        alg_tune_smartmask(cam);
        cam->smartmask_count = cam->smartmask_ratio;
    }

    /*
     * cam->frame_skip is set by the tracking code when camera has been asked to move.
     * When camera is moving we do not want motion to detect motion or we will
     * get our camera chasing itself like crazy and we will get motion detected
     * which is not really motion. So we pretend there is no motion by setting
     * cam->diffs = 0.
     * We also pretend to have a moving camera when we start Motion and when light
     * switch has been detected to allow camera to settle.
     */
    if (cam->frame_skip) {
        cam->frame_skip--;
        cam->current_image->diffs = 0;
    }

}

static void mlp_tuning(struct ctx_cam *cam){

    if ((cam->conf.noise_tune && cam->shots == 0) &&
         (!cam->detecting_motion && (cam->current_image->diffs <= cam->threshold)))
        alg_noise_tune(cam, cam->imgs.image_vprvcy);


    /*
     * If we are not noise tuning lets make sure that remote controlled
     * changes of noise_level are used.
     */
    if (cam->process_thisframe) {
        /*
         * threshold tuning if enabled
         * if we are not threshold tuning lets make sure that remote controlled
         * changes of threshold are used.
         */
        if (cam->conf.threshold_tune){
            alg_threshold_tune(cam, cam->current_image->diffs, cam->detecting_motion);
        }

        /*
         * If motion is detected (cam->current_image->diffs > cam->threshold) and before we add text to the pictures
         * we find the center and size coordinates of the motion to be used for text overlays and later
         * for adding the locate rectangle
         */
        if ((cam->current_image->diffs > cam->threshold) &&
            (cam->current_image->diffs < cam->threshold_maximum)){

            alg_locate_center_size(&cam->imgs
                , cam->imgs.width
                , cam->imgs.height
                , &cam->current_image->location);
            }

        /*
         * Update reference frame.
         * micro-lighswitch: trying to auto-detect lightswitch events.
         * frontdoor illumination. Updates are rate-limited to 3 per second at
         * framerates above 5fps to save CPU resources and to keep sensitivity
         * at a constant level.
         */

        if ((cam->current_image->diffs > cam->threshold) &&
            (cam->current_image->diffs < cam->threshold_maximum) &&
            (cam->conf.lightswitch_percent >= 1) &&
            (cam->lightswitch_framecounter < (cam->lastrate * 2)) && /* two seconds window only */
            /* number of changed pixels almost the same in two consecutive frames and */
            ((abs(cam->previous_diffs - cam->current_image->diffs)) < (cam->previous_diffs / 15)) &&
            /* center of motion in about the same place ? */
            ((abs(cam->current_image->location.x - cam->previous_location_x)) <= (cam->imgs.width / 150)) &&
            ((abs(cam->current_image->location.y - cam->previous_location_y)) <= (cam->imgs.height / 150))) {
            alg_update_reference_frame(cam, RESET_REF_FRAME);
            cam->current_image->diffs = 0;
            cam->lightswitch_framecounter = 0;

            MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("micro-lightswitch!"));
        } else {
            alg_update_reference_frame(cam, UPDATE_REF_FRAME);
        }
        cam->previous_diffs = cam->current_image->diffs;
        cam->previous_location_x = cam->current_image->location.x;
        cam->previous_location_y = cam->current_image->location.y;
    }


}

static void mlp_overlay(struct ctx_cam *cam){

    char tmp[PATH_MAX];

    /* Smartmask overlay */
    if (cam->smartmask_speed &&
        (cam->conf.picture_output_motion || cam->conf.movie_output_motion ||
         cam->conf.setup_mode || (cam->stream.motion.cnct_count > 0)))
        draw_smartmask(cam, cam->imgs.image_motion.image_norm);

    /* Largest labels overlay */
    if (cam->imgs.largest_label && (cam->conf.picture_output_motion || cam->conf.movie_output_motion ||
        cam->conf.setup_mode || (cam->stream.motion.cnct_count > 0)))
        draw_largest_label(cam, cam->imgs.image_motion.image_norm);

    /* Fixed mask overlay */
    if (cam->imgs.mask && (cam->conf.picture_output_motion || cam->conf.movie_output_motion ||
        cam->conf.setup_mode || (cam->stream.motion.cnct_count > 0)))
        draw_fixed_mask(cam, cam->imgs.image_motion.image_norm);

    /* Add changed pixels in upper right corner of the pictures */
    if (cam->conf.text_changes) {
        if (!cam->pause)
            sprintf(tmp, "%d", cam->current_image->diffs);
        else
            sprintf(tmp, "-");

        draw_text(cam->current_image->image_norm, cam->imgs.width, cam->imgs.height,
                  cam->imgs.width - 10, 10, tmp, cam->text_scale);
    }

    /*
     * Add changed pixels to motion-images (for stream) in setup_mode
     * and always overlay smartmask (not only when motion is detected)
     */
    if (cam->conf.setup_mode || (cam->stream.motion.cnct_count > 0)) {
        sprintf(tmp, "D:%5d L:%3d N:%3d", cam->current_image->diffs,
                cam->current_image->total_labels, cam->noise);
        draw_text(cam->imgs.image_motion.image_norm, cam->imgs.width, cam->imgs.height,
                  cam->imgs.width - 10, cam->imgs.height - (30 * cam->text_scale),
                  tmp, cam->text_scale);
        sprintf(tmp, "THREAD %d SETUP", cam->threadnr);
        draw_text(cam->imgs.image_motion.image_norm, cam->imgs.width, cam->imgs.height,
                  cam->imgs.width - 10, cam->imgs.height - (10 * cam->text_scale),
                  tmp, cam->text_scale);
    }

    /* Add text in lower left corner of the pictures */
    if (cam->conf.text_left) {
        mystrftime(cam, tmp, sizeof(tmp), cam->conf.text_left,
                   &cam->current_image->imgts, NULL, 0);
        draw_text(cam->current_image->image_norm, cam->imgs.width, cam->imgs.height,
                  10, cam->imgs.height - (10 * cam->text_scale), tmp, cam->text_scale);
    }

    /* Add text in lower right corner of the pictures */
    if (cam->conf.text_right) {
        mystrftime(cam, tmp, sizeof(tmp), cam->conf.text_right,
                   &cam->current_image->imgts, NULL, 0);
        draw_text(cam->current_image->image_norm, cam->imgs.width, cam->imgs.height,
                  cam->imgs.width - 10, cam->imgs.height - (10 * cam->text_scale),
                  tmp, cam->text_scale);
    }

}

static void mlp_actions(struct ctx_cam *cam){

    int indx;

    /***** MOTION LOOP - ACTIONS AND EVENT CONTROL SECTION *****/

    if ((cam->current_image->diffs > cam->threshold) &&
        (cam->current_image->diffs < cam->threshold_maximum)) {
        /* flag this image, it have motion */
        cam->current_image->flags |= IMAGE_MOTION;
        cam->lightswitch_framecounter++; /* micro lightswitch */
    } else {
        cam->lightswitch_framecounter = 0;
    }

    /*
     * If motion has been detected we take action and start saving
     * pictures and movies etc by calling mlp_detected().
     * Is emulate_motion enabled we always call mlp_detected()
     * If post_capture is enabled we also take care of this in the this
     * code section.
     */
    if ((cam->conf.emulate_motion || cam->event_user) && (cam->startup_frames == 0)) {
        /*  If we were previously detecting motion, started a movie, then got
         *  no motion then we reset the start movie time so that we do not
         *  get a pause in the movie.
        */
        if ( (cam->detecting_motion == 0) && (cam->movie_norm != NULL) )
            movie_reset_start_time(cam->movie_norm, &cam->current_image->imgts);
        cam->detecting_motion = 1;
        if (cam->conf.post_capture > 0) {
            /* Setup the postcap counter */
            cam->postcap = cam->conf.post_capture;
            // MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "(Em) Init post capture %d", cam->postcap);
        }

        cam->current_image->flags |= (IMAGE_TRIGGER | IMAGE_SAVE);
        /* Mark all images in image_ring to be saved */
        for (indx = 0; indx < cam->imgs.ring_size; indx++){
            cam->imgs.image_ring[indx].flags |= IMAGE_SAVE;
        }

        mlp_detected(cam, cam->video_dev, cam->current_image);
    } else if ((cam->current_image->flags & IMAGE_MOTION) && (cam->startup_frames == 0)) {
        /*
         * Did we detect motion (like the cat just walked in :) )?
         * If so, ensure the motion is sustained if minimum_motion_frames
         */

        /* Count how many frames with motion there is in the last minimum_motion_frames in precap buffer */
        int frame_count = 0;
        int pos = cam->imgs.ring_in;

        for (indx = 0; indx < cam->conf.minimum_motion_frames; indx++) {
            if (cam->imgs.image_ring[pos].flags & IMAGE_MOTION)
                frame_count++;

            if (pos == 0)
                pos = cam->imgs.ring_size-1;
            else
                pos--;
        }

        if (frame_count >= cam->conf.minimum_motion_frames) {

            cam->current_image->flags |= (IMAGE_TRIGGER | IMAGE_SAVE);
            /*  If we were previously detecting motion, started a movie, then got
             *  no motion then we reset the start movie time so that we do not
             *  get a pause in the movie.
            */
            if ( (cam->detecting_motion == 0) && (cam->movie_norm != NULL) )
                movie_reset_start_time(cam->movie_norm, &cam->current_image->imgts);

            cam->detecting_motion = 1;

            /* Setup the postcap counter */
            cam->postcap = cam->conf.post_capture;
            //MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "Setup post capture %d", cam->postcap);

            /* Mark all images in image_ring to be saved */
            for (indx = 0; indx < cam->imgs.ring_size; indx++)
                cam->imgs.image_ring[indx].flags |= IMAGE_SAVE;

        } else if (cam->postcap > 0) {
           /* we have motion in this frame, but not enought frames for trigger. Check postcap */
            cam->current_image->flags |= (IMAGE_POSTCAP | IMAGE_SAVE);
            cam->postcap--;
            //MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "post capture %d", cam->postcap);
        } else {
            cam->current_image->flags |= IMAGE_PRECAP;
        }

        /* Always call mlp_detected when we have a motion image */
        mlp_detected(cam, cam->video_dev, cam->current_image);
    } else if (cam->postcap > 0) {
        /* No motion, doing postcap */
        cam->current_image->flags |= (IMAGE_POSTCAP | IMAGE_SAVE);
        cam->postcap--;
        //MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "post capture %d", cam->postcap);
    } else {
        /* Done with postcap, so just have the image in the precap buffer */
        cam->current_image->flags |= IMAGE_PRECAP;
        /* gapless movie feature */
        if ((cam->conf.event_gap == 0) && (cam->detecting_motion == 1))
            cam->event_stop = TRUE;
        cam->detecting_motion = 0;
    }

    /* Update last frame saved time, so we can end event after gap time */
    if (cam->current_image->flags & IMAGE_SAVE)
        cam->lasttime = cam->current_image->imgts.tv_sec;


    mlp_areadetect(cam);

    /*
     * Is the movie too long? Then make movies
     * First test for movie_max_time
     */
    if ((cam->conf.movie_max_time && cam->event_nr == cam->prev_event) &&
        (cam->currenttime - cam->eventtime >= cam->conf.movie_max_time))
        cam->event_stop = TRUE;

    /*
     * Now test for quiet longer than 'gap' OR make movie as decided in
     * previous statement.
     */
    if (((cam->currenttime - cam->lasttime >= cam->conf.event_gap) && cam->conf.event_gap > 0) ||
          cam->event_stop) {
        if (cam->event_nr == cam->prev_event || cam->event_stop) {

            /* Flush image buffer */
            mlp_ring_process(cam, IMAGE_BUFFER_FLUSH);

            /* Save preview_shot here at the end of event */
            if (cam->imgs.image_preview.diffs) {
                event(cam, EVENT_IMAGE_PREVIEW, NULL, NULL, NULL, &cam->current_image->imgts);
                cam->imgs.image_preview.diffs = 0;
            }

            event(cam, EVENT_ENDMOTION, NULL, NULL, NULL, &cam->current_image->imgts);

            /*
             * If tracking is enabled we center our camera so it does not
             * point to a place where it will miss the next action
             */
            if (cam->track.type)
                cam->frame_skip = track_center(cam, cam->video_dev, 0, 0, 0);

            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("End of event %d"), cam->event_nr);

            cam->event_stop = FALSE;
            cam->event_user = FALSE;

            /* Reset post capture */
            cam->postcap = 0;

            /* Finally we increase the event number */
            cam->event_nr++;
            cam->lightswitch_framecounter = 0;

            /*
             * And we unset the text_event_string to avoid that buffered
             * images get a timestamp from previous event.
             */
            cam->text_event_string[0] = '\0';
        }
    }

    /* Save/send to movie some images */
    /* But why?  And why just two images from the ring? Didn't other functions flush already?*/
    mlp_ring_process(cam, 2);


}

static void mlp_setupmode(struct ctx_cam *cam){

    /* If CAMERA_VERBOSE enabled output some numbers to console */
    if (cam->conf.setup_mode) {
        char msg[1024] = "\0";
        char part[100];

        if (cam->conf.despeckle_filter) {
            snprintf(part, 99, _("Raw changes: %5d - changes after '%s': %5d"),
                     cam->olddiffs, cam->conf.despeckle_filter, cam->current_image->diffs);
            strcat(msg, part);
            if (strchr(cam->conf.despeckle_filter, 'l')) {
                snprintf(part, 99,_(" - labels: %3d"), cam->current_image->total_labels);
                strcat(msg, part);
            }
        } else {
            snprintf(part, 99,_("Changes: %5d"), cam->current_image->diffs);
            strcat(msg, part);
        }

        if (cam->conf.noise_tune) {
            snprintf(part, 99,_(" - noise level: %2d"), cam->noise);
            strcat(msg, part);
        }

        if (cam->conf.threshold_tune) {
            snprintf(part, 99, _(" - threshold: %d"), cam->threshold);
            strcat(msg, part);
        }

        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "%s", msg);
    }

}

static void mlp_snapshot(struct ctx_cam *cam){

    /* time_current_frame is used both for snapshot and timelapse features */
    cam->time_current_frame = cam->currenttime;

    if ((cam->conf.snapshot_interval > 0 && cam->shots == 0 &&
         cam->frame_curr_ts.tv_sec % cam->conf.snapshot_interval <=
         cam->frame_last_ts.tv_sec % cam->conf.snapshot_interval) ||
         cam->snapshot) {
        event(cam, EVENT_IMAGE_SNAPSHOT, cam->current_image, NULL, NULL, &cam->current_image->imgts);
        cam->snapshot = 0;
    }

}

static void mlp_timelapse(struct ctx_cam *cam){

    struct tm timestamp_tm;

    if (cam->conf.timelapse_interval) {
        localtime_r(&cam->current_image->imgts.tv_sec, &timestamp_tm);

        /*
         * Check to see if we should start a new timelapse file. We start one when
         * we are on the first shot, and and the seconds are zero. We must use the seconds
         * to prevent the timelapse file from getting reset multiple times during the minute.
         */
        if (timestamp_tm.tm_min == 0 &&
            (cam->frame_curr_ts.tv_sec % 60 < cam->frame_last_ts.tv_sec % 60) &&
            cam->shots == 0) {

            if (mystrceq(cam->conf.timelapse_mode, "manual")) {
                ;/* No action */

            /* If we are daily, raise timelapseend event at midnight */
            } else if (mystrceq(cam->conf.timelapse_mode, "daily")) {
                if (timestamp_tm.tm_hour == 0)
                    event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cam->current_image->imgts);

            /* handle the hourly case */
            } else if (mystreq(cam->conf.timelapse_mode, "hourly")) {
                event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cam->current_image->imgts);

            /* If we are weekly-sunday, raise timelapseend event at midnight on sunday */
            } else if (mystrceq(cam->conf.timelapse_mode, "weekly-sunday")) {
                if (timestamp_tm.tm_wday == 0 &&
                    timestamp_tm.tm_hour == 0)
                    event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cam->current_image->imgts);
            /* If we are weekly-monday, raise timelapseend event at midnight on monday */
            } else if (mystrceq(cam->conf.timelapse_mode, "weekly-monday") == 0) {
                if (timestamp_tm.tm_wday == 1 &&
                    timestamp_tm.tm_hour == 0)
                    event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cam->current_image->imgts);
            /* If we are monthly, raise timelapseend event at midnight on first day of month */
            } else if (mystrceq(cam->conf.timelapse_mode, "monthly")) {
                if (timestamp_tm.tm_mday == 1 &&
                    timestamp_tm.tm_hour == 0)
                    event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cam->current_image->imgts);
            /* If invalid we report in syslog once and continue in manual mode */
            } else {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Invalid timelapse_mode argument '%s'"), cam->conf.timelapse_mode);
                MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
                    ,_("%:s Defaulting to manual timelapse mode"));
                conf_parm_set(cam, (char *)"movie_timelapse_mode",(char *)"manual");
            }
        }

        if (cam->shots == 0 &&
            cam->frame_curr_ts.tv_sec % cam->conf.timelapse_interval <=
            cam->frame_last_ts.tv_sec % cam->conf.timelapse_interval) {
                event(cam, EVENT_TIMELAPSE, cam->current_image, NULL
                    , NULL, &cam->current_image->imgts);
        }

    } else if (cam->movie_timelapse) {
    /*
     * If timelapse movie is in progress but conf.timelapse_interval is zero then close timelapse file
     * This is an important feature that allows manual roll-over of timelapse file using the http
     * remote control via a cron job.
     */
        event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cam->current_image->imgts);
    }

    cam->time_last_frame = cam->time_current_frame;


}

static void mlp_loopback(struct ctx_cam *cam){
    /*
     * Feed last image and motion image to video device pipes and the stream clients
     * In setup mode we send the special setup mode image to both stream and vloopback pipe
     * In normal mode we feed the latest image to vloopback device and we send
     * the image to the stream. We always send the first image in a second to the stream.
     * Other image are sent only when the config option stream_motion is off
     * The result is that with stream_motion on the stream stream is normally at the minimal
     * 1 frame per second but the minute motion is detected the mlp_detected() function
     * sends all detected pictures to the stream except the 1st per second which is already sent.
     */
    if (cam->conf.setup_mode) {

        event(cam, EVENT_IMAGE, &cam->imgs.image_motion, NULL, &cam->pipe, &cam->current_image->imgts);
        event(cam, EVENT_STREAM, &cam->imgs.image_motion, NULL, NULL, &cam->current_image->imgts);
    } else {
        event(cam, EVENT_IMAGE, cam->current_image, NULL,
              &cam->pipe, &cam->current_image->imgts);

        if (!cam->conf.stream_motion || cam->shots == 1)
            event(cam, EVENT_STREAM, cam->current_image, NULL, NULL,
                  &cam->current_image->imgts);
    }

    event(cam, EVENT_IMAGEM, &cam->imgs.image_motion, NULL, &cam->mpipe, &cam->current_image->imgts);

}

static void mlp_parmsupdate(struct ctx_cam *cam){

    /* Check for some config parameter changes but only every second */
    if (cam->shots != 0) return;

    draw_init_scale(cam);  /* Initialize and validate text_scale */

    if (mystrceq(cam->conf.picture_output, "on"))
        cam->new_img = NEWIMG_ON;
    else if (mystrceq(cam->conf.picture_output, "first"))
        cam->new_img = NEWIMG_FIRST;
    else if (mystrceq(cam->conf.picture_output, "best"))
        cam->new_img = NEWIMG_BEST;
    else if (mystrceq(cam->conf.picture_output, "center"))
        cam->new_img = NEWIMG_CENTER;
    else
        cam->new_img = NEWIMG_OFF;

    if (mystrceq(cam->conf.locate_motion_mode, "on"))
        cam->locate_motion_mode = LOCATE_ON;
    else if (mystrceq(cam->conf.locate_motion_mode, "preview"))
        cam->locate_motion_mode = LOCATE_PREVIEW;
    else
        cam->locate_motion_mode = LOCATE_OFF;

    if (mystrceq(cam->conf.locate_motion_style, "box"))
        cam->locate_motion_style = LOCATE_BOX;
    else if (mystrceq(cam->conf.locate_motion_style, "redbox"))
        cam->locate_motion_style = LOCATE_REDBOX;
    else if (mystrceq(cam->conf.locate_motion_style, "cross"))
        cam->locate_motion_style = LOCATE_CROSS;
    else if (mystrceq(cam->conf.locate_motion_style, "redcross"))
        cam->locate_motion_style = LOCATE_REDCROSS;
    else
        cam->locate_motion_style = LOCATE_BOX;

    /* Sanity check for smart_mask_speed, silly value disables smart mask */
    if (cam->conf.smart_mask_speed < 0 || cam->conf.smart_mask_speed > 10)
        cam->conf.smart_mask_speed = 0;

    /* Has someone changed smart_mask_speed or framerate? */
    if (cam->conf.smart_mask_speed != cam->smartmask_speed ||
        cam->smartmask_lastrate != cam->lastrate) {
        if (cam->conf.smart_mask_speed == 0) {
            memset(cam->imgs.smartmask, 0, cam->imgs.motionsize);
            memset(cam->imgs.smartmask_final, 255, cam->imgs.motionsize);
        }

        cam->smartmask_lastrate = cam->lastrate;
        cam->smartmask_speed = cam->conf.smart_mask_speed;
        /*
            * Decay delay - based on smart_mask_speed (framerate independent)
            * This is always 5*smartmask_speed seconds
            */
        cam->smartmask_ratio = 5 * cam->lastrate * (11 - cam->smartmask_speed);
    }

    dbse_sqlmask_update(cam);

    cam->threshold = cam->conf.threshold;
    if (cam->conf.threshold_maximum > cam->conf.threshold ){
        cam->threshold_maximum = cam->conf.threshold_maximum;
    } else {
        cam->threshold_maximum = (cam->imgs.height * cam->imgs.width * 3) / 2;
    }

    if (!cam->conf.noise_tune){
        cam->noise = cam->conf.noise_level;
    }

}

static void mlp_frametiming(struct ctx_cam *cam){

    int indx;
    struct timespec ts2;
    int64_t avgtime;

    /* Shuffle the last wait times*/
    for (indx=0; indx<AVGCNT-1; indx++){
        cam->frame_wait[indx]=cam->frame_wait[indx+1];
    }

    if (cam->conf.framerate) {
        cam->frame_wait[AVGCNT-1] = 1000000L / cam->conf.framerate;
    } else {
        cam->frame_wait[AVGCNT-1] = 0;
    }

    clock_gettime(CLOCK_REALTIME, &ts2);

    cam->frame_wait[AVGCNT-1] = cam->frame_wait[AVGCNT-1] -
            (1000000L * (ts2.tv_sec - cam->frame_curr_ts.tv_sec)) -
            ((ts2.tv_nsec - cam->frame_curr_ts.tv_nsec)/1000);

    avgtime = 0;
    for (indx=0; indx<AVGCNT; indx++){
        avgtime = avgtime + cam->frame_wait[indx];
    }
    avgtime = (avgtime/AVGCNT);

    if (avgtime > 0) {
        avgtime = avgtime * 1000;
        /* If over 1 second, just do one*/
        if (avgtime > 999999999) {
            SLEEP(1, 0);
        } else {
            SLEEP(0, avgtime);
        }
    }

}

/** Thread function for each camera */
void *motion_loop(void *arg) {

    struct ctx_cam *cam =(struct ctx_cam *) arg;

    if (mlp_init(cam) == 0){
        while (!cam->finish_cam || cam->event_stop) {
            mlp_prepare(cam);
            if (cam->get_image) {
                mlp_resetimages(cam);
                if (mlp_retry(cam) == 1)  break;
                if (mlp_capture(cam) == 1)  break;
                mlp_detection(cam);
                mlp_tuning(cam);
                mlp_overlay(cam);
                mlp_actions(cam);
                mlp_setupmode(cam);
            }
            mlp_snapshot(cam);
            mlp_timelapse(cam);
            mlp_loopback(cam);
            mlp_parmsupdate(cam);
            mlp_frametiming(cam);
        }
    }

    cam->lost_connection = 1;
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Thread exiting"));

    mlp_cleanup(cam);

    pthread_mutex_lock(&cam->motapp->global_lock);
        cam->motapp->threads_running--;
    pthread_mutex_unlock(&cam->motapp->global_lock);

    cam->running_cam = 0;
    cam->finish_cam = 0;

    pthread_exit(NULL);
}

