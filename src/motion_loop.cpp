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
 *
*/

#include "motionplus.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "motion_loop.hpp"
#include "rotate.hpp"
#include "movie.hpp"
#include "libcam.hpp"
#include "video_v4l2.hpp"
#include "video_loopback.hpp"
#include "netcam.hpp"
#include "conf.hpp"
#include "alg.hpp"
#include "alg_sec.hpp"
#include "picture.hpp"
#include "webu.hpp"
#include "dbse.hpp"
#include "draw.hpp"
#include "webu_getimg.hpp"

/* Resize the image ring */
static void mlp_ring_resize(ctx_dev *cam)
{
    int i, new_size;
    ctx_image_data *tmp;

    new_size = cam->conf->pre_capture + cam->conf->minimum_motion_frames;
    if (new_size < 1) {
        new_size = 1;
    }

    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
        ,_("Resizing buffer to %d items"), new_size);

    tmp =(ctx_image_data*) mymalloc((uint)new_size * sizeof(ctx_image_data));

    for(i = 0; i < new_size; i++) {
        tmp[i].image_norm =(unsigned char*) mymalloc((uint)cam->imgs.size_norm);
        memset(tmp[i].image_norm, 0x80, (uint)cam->imgs.size_norm);
        if (cam->imgs.size_high > 0) {
            tmp[i].image_high =(unsigned char*) mymalloc((uint)cam->imgs.size_high);
            memset(tmp[i].image_high, 0x80, (uint)cam->imgs.size_high);
        }
    }

    cam->imgs.image_ring = tmp;
    cam->current_image = NULL;
    cam->imgs.ring_size = new_size;
    cam->imgs.ring_in = 0;
    cam->imgs.ring_out = 0;

}

/* Clean image ring */
static void mlp_ring_destroy(ctx_dev *cam)
{
    int i;

    if (cam->imgs.image_ring == NULL) {
        return;
    }

    for (i = 0; i < cam->imgs.ring_size; i++) {
        myfree(cam->imgs.image_ring[i].image_norm);
        myfree(cam->imgs.image_ring[i].image_high);
    }
    myfree(cam->imgs.image_ring);

    /*
     * current_image is an alias from the pointers above which have
     * already been freed so we just set it equal to NULL here
    */
    cam->current_image = NULL;

    cam->imgs.ring_size = 0;
}

/* Add debug messsage to image */
static void mlp_ring_process_debug(ctx_dev *cam)
{
    char tmp[32];
    const char *t;

    if (cam->current_image->flags & IMAGE_TRIGGER) {
        t = "Trigger";
    } else if (cam->current_image->flags & IMAGE_MOTION) {
        t = "Motion";
    } else if (cam->current_image->flags & IMAGE_PRECAP) {
        t = "Precap";
    } else if (cam->current_image->flags & IMAGE_POSTCAP) {
        t = "Postcap";
    } else {
        t = "Other";
    }

    mystrftime(cam, tmp, sizeof(tmp), "%H%M%S-%q", NULL);
    cam->draw->text(cam->imgs.image_ring[cam->imgs.ring_out].image_norm
            , cam->imgs.width, cam->imgs.height
            , 10, 20, tmp, cam->text_scale);
    cam->draw->text(cam->imgs.image_ring[cam->imgs.ring_out].image_norm
            , cam->imgs.width, cam->imgs.height
            , 10, 30, t, cam->text_scale);
}

static void mlp_ring_process_image(ctx_dev *cam)
{
    cam->picture->process_norm();
    if (cam->movie_norm->put_image(cam->current_image
            , &cam->current_image->imgts) == -1) {
        MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
    }
    if (cam->movie_motion->put_image(&cam->imgs.image_motion
            , &cam->imgs.image_motion.imgts) == -1) {
        MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
    }
    if (cam->movie_extpipe->put_image(cam->current_image
            , &cam->current_image->imgts) == -1) {
        MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
    }
}

/* Process the entire image ring */
static void mlp_ring_process(ctx_dev *cam)
{
    ctx_image_data *saved_current_image = cam->current_image;

    do {
        if ((cam->imgs.image_ring[cam->imgs.ring_out].flags & (IMAGE_SAVE | IMAGE_SAVED)) != IMAGE_SAVE) {
            break;
        }

        cam->current_image = &cam->imgs.image_ring[cam->imgs.ring_out];

        if (cam->current_image->shot <= cam->conf->framerate) {
            if (cam->motapp->conf->log_level >= DBG) {
                mlp_ring_process_debug(cam);
            }
            mlp_ring_process_image(cam);
        }

        cam->current_image->flags |= IMAGE_SAVED;

        if (cam->current_image->flags & IMAGE_MOTION) {
            if (cam->new_img & NEWIMG_BEST) {
                if (cam->current_image->diffs > cam->imgs.image_preview.diffs) {
                    cam->picture->save_preview();
                }
            }
            if (cam->new_img & NEWIMG_CENTER) {
                if (cam->current_image->cent_dist < cam->imgs.image_preview.cent_dist) {
                    cam->picture->save_preview();
                }
            }
        }

        if (++cam->imgs.ring_out >= cam->imgs.ring_size) {
            cam->imgs.ring_out = 0;
        }

    } while (cam->imgs.ring_out != cam->imgs.ring_in);

    cam->current_image = saved_current_image;
}

/* Reset the image info variables*/
static void mlp_info_reset(ctx_dev *cam)
{
    cam->info_diff_cnt = 0;
    cam->info_diff_tot = 0;
    cam->info_sdev_min = 99999999;
    cam->info_sdev_max = 0;
    cam->info_sdev_tot = 0;
}

static void mlp_movie_start(ctx_dev *cam)
{
    cam->movie_start_time = cam->frame_curr_ts.tv_sec;
    if (cam->lastrate < 2) {
        cam->movie_fps = 2;
    } else {
        cam->movie_fps = cam->lastrate;
    }
    cam->movie_norm->start();
    cam->movie_motion->start();
    cam->movie_extpipe->start();
}

static void mlp_movie_end(ctx_dev *cam)
{
    cam->movie_norm->stop();
    cam->movie_motion->stop();
    cam->movie_extpipe->stop();
}

/* Process the motion detected items*/
static void mlp_detected_trigger(ctx_dev *cam)
{
    time_t raw_time;
    struct tm evt_tm;

    if (cam->current_image->flags & IMAGE_TRIGGER) {
        if (cam->event_curr_nbr != cam->event_prev_nbr) {
            mlp_info_reset(cam);
            cam->event_prev_nbr = cam->event_curr_nbr;

            cam->algsec->detected = false;

            time(&raw_time);
            localtime_r(&raw_time, &evt_tm);
            sprintf(cam->eventid, "%05d", cam->device_id);
            strftime(cam->eventid+5, 15, "%Y%m%d%H%M%S", &evt_tm);

            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motion detected - starting event %d"),
                       cam->event_curr_nbr);

            mystrftime(cam, cam->text_event_string
                , sizeof(cam->text_event_string)
                , cam->conf->text_event.c_str(), NULL);

            if (cam->conf->on_event_start != "") {
                util_exec_command(cam, cam->conf->on_event_start.c_str(), NULL);
            }
            mlp_movie_start(cam);
            cam->motapp->dbse->exec(cam, "", "event_start");

            if (cam->new_img & (NEWIMG_FIRST | NEWIMG_BEST | NEWIMG_CENTER)) {
                cam->picture->save_preview();
            }

        }
        if (cam->conf->on_motion_detected != "") {
            util_exec_command(cam, cam->conf->on_motion_detected.c_str(), NULL);
        }
    }
}

/* call ptz camera center */
static void mlp_track_center(ctx_dev *cam)
{
    if ((cam->conf->ptz_auto_track) && (cam->conf->ptz_move_track != "")) {
        cam->track_posx = 0;
        cam->track_posy = 0;
        util_exec_command(cam, cam->conf->ptz_move_track.c_str(), NULL);
        cam->frame_skip = cam->conf->ptz_wait;
    }
}

/* call ptz camera move */
static void mlp_track_move(ctx_dev *cam)
{
    if ((cam->conf->ptz_auto_track) && (cam->conf->ptz_move_track != "")) {
            cam->track_posx += cam->current_image->location.x;
            cam->track_posy += cam->current_image->location.y;
            util_exec_command(cam, cam->conf->ptz_move_track.c_str(), NULL);
            cam->frame_skip = cam->conf->ptz_wait;
    }
}

/* motion detected */
static void mlp_detected(ctx_dev *cam)
{
    unsigned int distX, distY;

    cam->draw->locate();

    /* Calculate how centric motion is if configured preview center*/
    if (cam->new_img & NEWIMG_CENTER) {
        distX = (uint)abs((cam->imgs.width / 2) - cam->current_image->location.x );
        distY = (uint)abs((cam->imgs.height / 2) - cam->current_image->location.y);
        cam->current_image->cent_dist = distX * distX + distY * distY;
    }

    mlp_detected_trigger(cam);

    if (cam->current_image->shot <= cam->conf->framerate) {
        if ((cam->conf->stream_motion == true) &&
            (cam->current_image->shot != 1)) {
            webu_getimg_main(cam);
        }
        cam->picture->process_motion();
    }

    mlp_track_move(cam);
}

/* Apply the privacy mask to image*/
static void mlp_mask_privacy(ctx_dev *cam)
{
    if (cam->imgs.mask_privacy == NULL) {
        return;
    }

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
    if (cam->imgs.size_high > 0) {
        indx_max = 2;
    } else {
        indx_max = 1;
    }
    increment = sizeof(unsigned long);

    while (indx_img <= indx_max) {
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
            if (*(mask++) == 0x00) {
                *image = 0x80; // Mask last remaining bytes.
            }
            image += 1;
        }

        indx_img++;
    }
}

/* Close and clean up camera*/
void mlp_cam_close(ctx_dev *cam)
{
    mydelete(cam->libcam);
    mydelete(cam->v4l2cam);
    mydelete(cam->netcam);
    mydelete(cam->netcam_high);
}

/* Start camera */
void mlp_cam_start(ctx_dev *cam)
{
    if (cam->camera_type == CAMERA_TYPE_LIBCAM) {
        cam->libcam = new cls_libcam(cam);
    } else if (cam->camera_type == CAMERA_TYPE_NETCAM) {
        cam->netcam = new cls_netcam(cam, false);
        if (cam->conf->netcam_high_url != "") {
            cam->netcam_high = new cls_netcam(cam, true);
        }
    } else if (cam->camera_type == CAMERA_TYPE_V4L2) {
        cam->v4l2cam = new cls_v4l2cam(cam);
    } else {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("No Camera device specified"));
        cam->device_status = STATUS_CLOSED;
    }
}

/* Get next image from camera */
int mlp_cam_next(ctx_dev *cam, ctx_image_data *img_data)
{
    int retcd;

    if (cam->camera_type == CAMERA_TYPE_LIBCAM) {
        retcd = cam->libcam->next(img_data);
    } else if (cam->camera_type == CAMERA_TYPE_NETCAM) {
        retcd = cam->netcam->next(img_data);
        if ((retcd == CAPTURE_SUCCESS) &&
            (cam->netcam_high != nullptr)) {
            retcd = cam->netcam_high->next(img_data);
        }
        cam->rotate->process(img_data);
    } else if (cam->camera_type == CAMERA_TYPE_V4L2) {
        retcd = cam->v4l2cam->next(img_data);
    } else {
        retcd = -1;
    }

    return retcd;

}

/* Assign the camera type */
static void mlp_init_camera_type(ctx_dev *cam)
{
    if (cam->conf->libcam_device != "") {
        cam->camera_type = CAMERA_TYPE_LIBCAM;
    } else if (cam->conf->netcam_url != "") {
        cam->camera_type = CAMERA_TYPE_NETCAM;
    } else if (cam->conf->v4l2_device != "") {
        cam->camera_type = CAMERA_TYPE_V4L2;
    } else {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("Unable to determine camera type"));
        cam->camera_type = CAMERA_TYPE_UNKNOWN;
        cam->finish_dev = true;
        cam->restart_dev = false;
    }
}

/** Get first images from camera at startup */
static void mlp_init_firstimage(ctx_dev *cam)
{
    int indx;
    const char *msg;

    cam->current_image = &cam->imgs.image_ring[cam->imgs.ring_in];
    if (cam->device_status == STATUS_OPENED) {
        for (indx = 0; indx < 5; indx++) {
            if (mlp_cam_next(cam, cam->current_image) == CAPTURE_SUCCESS) {
                break;
            }
            SLEEP(2, 0);
        }
    } else {
        indx = 0;
    }

    if ((indx >= 5) || (cam->device_status != STATUS_OPENED)) {
        if (cam->device_status != STATUS_OPENED) {
            msg = "Unable to open camera";
        } else {
            msg = "Error capturing first image";
        }
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO, "%s", msg);
        for (indx = 0; indx<cam->imgs.ring_size; indx++) {
            memset(cam->imgs.image_ring[indx].image_norm
                , 0x80, (uint)cam->imgs.size_norm);
            cam->draw->text(cam->imgs.image_ring[indx].image_norm
                , cam->imgs.width, cam->imgs.height
                , 10, 20 * cam->text_scale
                , msg, cam->text_scale);
        }
    }

    cam->noise = cam->conf->noise_level;
    cam->threshold = cam->conf->threshold;
    if (cam->conf->threshold_maximum > cam->conf->threshold ) {
        cam->threshold_maximum = cam->conf->threshold_maximum;
    } else {
        cam->threshold_maximum = (cam->imgs.height * cam->imgs.width * 3) / 2;
    }

}

/** Check the image size to determine if modulo 8 and over 64 */
static void mlp_check_szimg(ctx_dev *cam)
{
    if ((cam->imgs.width % 8) || (cam->imgs.height % 8)) {
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("Image width (%d) or height(%d) requested is not modulo 8.")
            ,cam->imgs.width, cam->imgs.height);
        cam->device_status = STATUS_RESET;
    }
    if ((cam->imgs.width  < 64) || (cam->imgs.height < 64)) {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Motion only supports width and height greater than or equal to 64 %dx%d")
            ,cam->imgs.width, cam->imgs.height);
        cam->device_status = STATUS_RESET;
    }
    /* Substream size notification*/
    if ((cam->imgs.width % 16) || (cam->imgs.height % 16)) {
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("Substream not available.  Image sizes not modulo 16."));
    }

}

/** Set the items required for the area detect */
static void mlp_init_areadetect(ctx_dev *cam)
{
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
static void mlp_init_buffers(ctx_dev *cam)
{
    cam->imgs.ref =(unsigned char*) mymalloc((uint)cam->imgs.size_norm);
    cam->imgs.image_motion.image_norm = (unsigned char*)mymalloc((uint)cam->imgs.size_norm);
    cam->imgs.ref_dyn =(int*) mymalloc((uint)cam->imgs.motionsize * sizeof(*cam->imgs.ref_dyn));
    cam->imgs.image_virgin =(unsigned char*) mymalloc((uint)cam->imgs.size_norm);
    cam->imgs.image_vprvcy = (unsigned char*)mymalloc((uint)cam->imgs.size_norm);
    cam->imgs.labels =(int*)mymalloc((uint)cam->imgs.motionsize * sizeof(*cam->imgs.labels));
    cam->imgs.labelsize =(int*) mymalloc((uint)(cam->imgs.motionsize/2+1) * sizeof(*cam->imgs.labelsize));
    cam->imgs.image_preview.image_norm =(unsigned char*) mymalloc((uint)cam->imgs.size_norm);
    cam->imgs.common_buffer =(unsigned char*) mymalloc((uint)(3 * cam->imgs.width * cam->imgs.height));
    cam->imgs.image_secondary =(unsigned char*) mymalloc((uint)(3 * cam->imgs.width * cam->imgs.height));
    if (cam->imgs.size_high > 0) {
        cam->imgs.image_preview.image_high =(unsigned char*) mymalloc((uint)cam->imgs.size_high);
    } else {
        cam->imgs.image_preview.image_high = NULL;
    }

}

/* Initialize loop values */
static void mlp_init_values(ctx_dev *cam)
{
    cam->event_curr_nbr = 1;
    cam->event_prev_nbr = 0;

    cam->watchdog = cam->conf->watchdog_tmo;

    clock_gettime(CLOCK_MONOTONIC, &cam->frame_curr_ts);
    clock_gettime(CLOCK_MONOTONIC, &cam->frame_last_ts);

    cam->noise = cam->conf->noise_level;
    cam->passflag = false;
    cam->motapp->all_sizes->reset= true;
    cam->threshold = cam->conf->threshold;
    cam->device_status = STATUS_CLOSED;
    cam->startup_frames = (cam->conf->framerate * 2) + cam->conf->pre_capture + cam->conf->minimum_motion_frames;

    cam->movie_passthrough = cam->conf->movie_passthrough;
    if ((cam->camera_type != CAMERA_TYPE_NETCAM) &&
        (cam->movie_passthrough)) {
        MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Pass-through processing disabled."));
        cam->movie_passthrough = false;
    }
    if (cam->motapp->pause) {
        cam->pause = true;
    } else {
        cam->pause = cam->conf->pause;
    }
    cam->v4l2cam = nullptr;
    cam->netcam = nullptr;
    cam->netcam_high = nullptr;
    cam->rotate = nullptr;
    cam->picture = nullptr;
    cam->movie_norm = nullptr;
    cam->movie_norm = nullptr;
    cam->movie_motion = nullptr;
    cam->movie_timelapse = nullptr;
    cam->movie_extpipe = nullptr;
    cam->draw = nullptr;

    gethostname (cam->hostname, PATH_MAX);
    cam->hostname[PATH_MAX-1] = '\0';

}

/* start the camera */
static void mlp_init_cam_start(ctx_dev *cam)
{
    mlp_cam_start(cam);

    if (cam->device_status == STATUS_CLOSED) {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Failed to start camera."));
        cam->imgs.width = cam->conf->width;
        cam->imgs.height = cam->conf->height;
    }

    cam->imgs.motionsize = (cam->imgs.width * cam->imgs.height);
    cam->imgs.size_norm  = (cam->imgs.width * cam->imgs.height * 3) / 2;
    cam->imgs.size_high  = (cam->imgs.width_high * cam->imgs.height_high * 3) / 2;

}

/* initialize reference images*/
static void mlp_init_ref(ctx_dev *cam)
{
    memcpy(cam->imgs.image_virgin, cam->current_image->image_norm
        , (uint)cam->imgs.size_norm);

    mlp_mask_privacy(cam);

    memcpy(cam->imgs.image_vprvcy, cam->current_image->image_norm
        , (uint)cam->imgs.size_norm);

    cam->alg->ref_frame_reset();
}

/** clean up all memory etc. from motion init */
void mlp_cleanup(ctx_dev *cam)
{
    cam->movie_timelapse->stop();
    if (cam->event_curr_nbr == cam->event_prev_nbr) {
        mlp_ring_process(cam);
        if (cam->imgs.image_preview.diffs) {
            cam->picture->process_preview();
            cam->imgs.image_preview.diffs = 0;
        }
        if (cam->conf->on_event_end != "") {
            util_exec_command(cam, cam->conf->on_event_end.c_str(), NULL);
        }
        mlp_movie_end(cam);
        cam->motapp->dbse->exec(cam, "", "event_end");
    }

    webu_getimg_deinit(cam);

    if (cam->device_status == STATUS_OPENED) {
        mlp_cam_close(cam);
    }

    myfree(cam->imgs.image_motion.image_norm);
    myfree(cam->imgs.ref);
    myfree(cam->imgs.ref_dyn);
    myfree(cam->imgs.image_virgin);
    myfree(cam->imgs.image_vprvcy);
    myfree(cam->imgs.labels);
    myfree(cam->imgs.labelsize);
    myfree(cam->imgs.mask);
    myfree(cam->imgs.mask_privacy);
    myfree(cam->imgs.mask_privacy_uv);
    myfree(cam->imgs.mask_privacy_high);
    myfree(cam->imgs.mask_privacy_high_uv);
    myfree(cam->imgs.common_buffer);
    myfree(cam->imgs.image_secondary);
    myfree(cam->imgs.image_preview.image_norm);
    myfree(cam->imgs.image_preview.image_high);

    mlp_ring_destroy(cam); /* Cleanup the precapture ring buffer */

    mydelete(cam->alg);
    mydelete(cam->algsec);
    mydelete(cam->rotate);
    mydelete(cam->picture);
    mydelete(cam->movie_norm);
    mydelete(cam->movie_motion);
    mydelete(cam->movie_timelapse);
    mydelete(cam->movie_extpipe);
    mydelete(cam->draw);

    if (cam->pipe != -1) {
        close(cam->pipe);
        cam->pipe = -1;
    }

    if (cam->mpipe != -1) {
        close(cam->mpipe);
        cam->mpipe = -1;
    }

}

/* initialize everything for the loop */
static void mlp_init(ctx_dev *cam)
{
    if ((cam->device_status != STATUS_INIT) &&
        (cam->device_status != STATUS_RESET)) {
        return;
    }

    if (cam->device_status == STATUS_RESET) {
        mlp_cleanup(cam);
    }

    MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO,_("Initialize"));

    mlp_init_camera_type(cam);

    mlp_init_values(cam);

    mlp_init_cam_start(cam);

    mlp_check_szimg(cam);

    mlp_ring_resize(cam);

    mlp_init_buffers(cam);

    webu_getimg_init(cam);

    cam->rotate = new cls_rotate(cam);

    mlp_init_firstimage(cam);

    vlp_init(cam);
    cam->alg = new cls_alg(cam);
    cam->algsec = new cls_algsec(cam);
    cam->picture = new cls_picture(cam);
    cam->draw = new cls_draw(cam);
    cam->movie_norm = new cls_movie(cam, "norm");
    cam->movie_motion = new cls_movie(cam, "motion");
    cam->movie_timelapse = new cls_movie(cam, "timelapse");
    cam->movie_extpipe = new cls_movie(cam, "extpipe");

    mlp_init_areadetect(cam);

    mlp_init_ref(cam);

    if (cam->device_status == STATUS_OPENED) {
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Camera %d started: motion detection %s"),
            cam->device_id, cam->pause ? _("Disabled"):_("Enabled"));

        if (cam->conf->emulate_motion) {
            MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, _("Emulating motion"));
        }
    }
}



/* check the area detect */
static void mlp_areadetect(ctx_dev *cam)
{
    int i, j, z = 0;

    if ((cam->conf->area_detect != "" ) &&
        (cam->event_curr_nbr != cam->areadetect_eventnbr) &&
        (cam->current_image->flags & IMAGE_TRIGGER)) {
        j = (int)cam->conf->area_detect.length();
        for (i = 0; i < j; i++) {
            z = cam->conf->area_detect[(uint)i] - 49; /* characters are stored as ascii 48-57 (0-9) */
            if ((z >= 0) && (z < 9)) {
                if (cam->current_image->location.x > cam->area_minx[z] &&
                    cam->current_image->location.x < cam->area_maxx[z] &&
                    cam->current_image->location.y > cam->area_miny[z] &&
                    cam->current_image->location.y < cam->area_maxy[z]) {
                    if (cam->conf->on_area_detected != "") {
                        util_exec_command(cam, cam->conf->on_area_detected.c_str(), NULL);
                    }
                    cam->areadetect_eventnbr = cam->event_curr_nbr; /* Fire script only once per event */
                    MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO
                        ,_("Motion in area %d detected."), z + 1);
                    break;
                }
            }
        }
    }
}

/* Prepare for the next iteration of loop*/
static void mlp_prepare(ctx_dev *cam)
{
    cam->watchdog = cam->conf->watchdog_tmo;

    cam->frame_last_ts.tv_sec = cam->frame_curr_ts.tv_sec;
    cam->frame_last_ts.tv_nsec = cam->frame_curr_ts.tv_nsec;
    clock_gettime(CLOCK_MONOTONIC, &cam->frame_curr_ts);

    if (cam->frame_last_ts.tv_sec != cam->frame_curr_ts.tv_sec) {
        cam->lastrate = cam->shots_mt + 1;
        cam->shots_mt = -1;
    }
    cam->shots_mt++;

    if (cam->conf->pre_capture < 0) {
        cam->conf->pre_capture = 0;
    }

    if (cam->startup_frames > 0) {
        cam->startup_frames--;
    }
}

/* reset the images */
static void mlp_resetimages(ctx_dev *cam)
{
    int64_t tmpsec;

    /* ring_buffer_in is pointing to current pos, update before put in a new image */
    tmpsec =cam->current_image->imgts.tv_sec;
    if (++cam->imgs.ring_in >= cam->imgs.ring_size) {
        cam->imgs.ring_in = 0;
    }

    /* Check if we have filled the ring buffer, throw away last image */
    if (cam->imgs.ring_in == cam->imgs.ring_out) {
        if (++cam->imgs.ring_out >= cam->imgs.ring_size) {
            cam->imgs.ring_out = 0;
        }
    }

    cam->current_image = &cam->imgs.image_ring[cam->imgs.ring_in];
    cam->current_image->diffs = 0;
    cam->current_image->flags = 0;
    cam->current_image->cent_dist = 0;
    memset(&cam->current_image->location, 0, sizeof(cam->current_image->location));
    cam->current_image->total_labels = 0;

    clock_gettime(CLOCK_REALTIME, &cam->current_image->imgts);
    clock_gettime(CLOCK_MONOTONIC, &cam->current_image->monots);

    if (tmpsec != cam->current_image->imgts.tv_sec) {
        cam->shots_rt = 1;
    }  else {
        cam->shots_rt++;
    }
    /* Store shot number with pre_captured image */
    cam->current_image->shot = cam->shots_rt;

}

/* Try to reconnect to camera */
static void mlp_retry(ctx_dev *cam)
{
    int size_high;

    if ((cam->device_status == STATUS_CLOSED) &&
        (cam->frame_curr_ts.tv_sec % 10 == 0) &&
        (cam->shots_mt == 0)) {
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Retrying until successful connection with camera"));

        mlp_cam_start(cam);

        mlp_check_szimg(cam);

        if (cam->imgs.width != cam->conf->width || cam->imgs.height != cam->conf->height) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Resetting image buffers"));
            cam->device_status = STATUS_RESET;
        }
        /*
         * For high res, we check the size of buffer to determine whether to break out
         * the init_motion function allocated the buffer for high using the cam->imgs.size_high
         * and the mlp_cam_start ONLY re-populates the height/width so we can check the size here.
         */
        size_high = (cam->imgs.width_high * cam->imgs.height_high * 3) / 2;
        if (cam->imgs.size_high != size_high) {
            cam->device_status = STATUS_RESET;
        }
    }

}

/* Get next image from camera */
static int mlp_capture(ctx_dev *cam)
{
    const char *tmpin;
    char tmpout[80];
    int retcd;

    if (cam->device_status != STATUS_OPENED) {
        return 0;
    }

    retcd = mlp_cam_next(cam, cam->current_image);

    if (retcd == CAPTURE_SUCCESS) {
        cam->lost_connection = 0;
        cam->connectionlosttime.tv_sec = 0;

        if (cam->missing_frame_counter >= (cam->conf->device_tmo * cam->conf->framerate)) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Video signal re-acquired"));
            if (cam->conf->on_camera_found != "") {
                util_exec_command(cam, cam->conf->on_camera_found.c_str(), NULL);
            }
        }
        cam->missing_frame_counter = 0;
        memcpy(cam->imgs.image_virgin, cam->current_image->image_norm
            , (uint)cam->imgs.size_norm);
        mlp_mask_privacy(cam);
        memcpy(cam->imgs.image_vprvcy, cam->current_image->image_norm
            , (uint)cam->imgs.size_norm);

    } else {
        if (cam->connectionlosttime.tv_sec == 0) {
            clock_gettime(CLOCK_REALTIME, &cam->connectionlosttime);
        }

        cam->missing_frame_counter++;

        if ((cam->device_status == STATUS_OPENED) &&
            (cam->missing_frame_counter <
                (cam->conf->device_tmo * cam->conf->framerate))) {
            memcpy(cam->current_image->image_norm, cam->imgs.image_vprvcy
                , (uint)cam->imgs.size_norm);
        } else {
            cam->lost_connection = 1;
            if (cam->device_status == STATUS_OPENED) {
                tmpin = "CONNECTION TO CAMERA LOST\\nSINCE %Y-%m-%d %T";
            } else {
                tmpin = "UNABLE TO OPEN VIDEO DEVICE\\nSINCE %Y-%m-%d %T";
            }

            memset(cam->current_image->image_norm, 0x80, (uint)cam->imgs.size_norm);
            cam->current_image->imgts =cam->connectionlosttime;
            mystrftime(cam, tmpout, sizeof(tmpout), tmpin, NULL);
            cam->draw->text(cam->current_image->image_norm
                    , cam->imgs.width, cam->imgs.height
                    , 10, 20 * cam->text_scale
                    , tmpout, cam->text_scale);

            /* Write error message only once */
            if (cam->missing_frame_counter == (cam->conf->device_tmo * cam->conf->framerate)) {
                MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                    ,_("Video signal lost - Adding grey image"));
                if (cam->conf->on_camera_lost != "") {
                    util_exec_command(cam, cam->conf->on_camera_lost.c_str(), NULL);
                }
            }

            if ((cam->device_status == STATUS_OPENED) &&
                (cam->missing_frame_counter == ((cam->conf->device_tmo * 4) * cam->conf->framerate))) {
                MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Video signal still lost - Trying to close video device"));
                mlp_cam_close(cam);
            }
        }
    }
    return 0;

}

/* call detection */
static void mlp_detection(ctx_dev *cam)
{
    if (cam->frame_skip) {
        cam->frame_skip--;
        cam->current_image->diffs = 0;
        return;
    }

    if (cam->pause == false) {
        cam->alg->diff();
    } else {
        cam->current_image->diffs = 0;
        cam->current_image->diffs_raw = 0;
        cam->current_image->diffs_ratio = 100;
    }
}

/* tune the detection parameters*/
static void mlp_tuning(ctx_dev *cam)
{
    if ((cam->conf->noise_tune && cam->shots_mt == 0) &&
          (!cam->detecting_motion && (cam->current_image->diffs <= cam->threshold))) {
        cam->alg->noise_tune();
    }

    if (cam->conf->threshold_tune) {
        cam->alg->threshold_tune();
    }

    if ((cam->current_image->diffs > cam->threshold) &&
        (cam->current_image->diffs < cam->threshold_maximum)) {
        cam->alg->location();
        cam->alg->stddev();

    }

    if (cam->current_image->diffs_ratio < cam->conf->threshold_ratio) {
        cam->current_image->diffs = 0;
    }

    cam->alg->tune_smartmask();

    cam->alg->ref_frame_update();

    cam->previous_diffs = cam->current_image->diffs;
    cam->previous_location_x = cam->current_image->location.x;
    cam->previous_location_y = cam->current_image->location.y;
}

/* apply image overlays */
static void mlp_overlay(ctx_dev *cam)
{
    char tmp[PATH_MAX];

    if ((cam->conf->smart_mask_speed >0) &&
        ((cam->conf->picture_output_motion != "off") ||
        cam->conf->movie_output_motion ||
        (cam->stream.motion.jpg_cnct > 0) ||
        (cam->stream.motion.ts_cnct > 0))) {
        cam->draw->smartmask();
    }

    if (cam->imgs.largest_label &&
        ((cam->conf->picture_output_motion != "off") ||
        cam->conf->movie_output_motion ||
        (cam->stream.motion.jpg_cnct > 0) ||
        (cam->stream.motion.ts_cnct > 0))) {
        cam->draw->largest_label();
    }

    if (cam->imgs.mask &&
        ((cam->conf->picture_output_motion != "off") ||
        cam->conf->movie_output_motion ||
        (cam->stream.motion.jpg_cnct > 0) ||
        (cam->stream.motion.ts_cnct > 0))) {
        cam->draw->fixed_mask();
    }

    if (cam->conf->text_changes) {
        if (cam->pause == false) {
            sprintf(tmp, "%d", cam->current_image->diffs);
        } else {
            sprintf(tmp, "-");
        }
        cam->draw->text(cam->current_image->image_norm
                , cam->imgs.width, cam->imgs.height
                , cam->imgs.width - 10, 10
                , tmp, cam->text_scale);
    }

    if ((cam->stream.motion.jpg_cnct > 0) ||
        (cam->stream.motion.ts_cnct > 0)) {
        sprintf(tmp, "D:%5d L:%3d N:%3d", cam->current_image->diffs,
            cam->current_image->total_labels, cam->noise);
        cam->draw->text(cam->imgs.image_motion.image_norm
                , cam->imgs.width, cam->imgs.height
                , cam->imgs.width - 10
                , cam->imgs.height - (30 * cam->text_scale)
                , tmp, cam->text_scale);
        sprintf(tmp, "THREAD %d SETUP", cam->threadnr);
        cam->draw->text(cam->imgs.image_motion.image_norm
                , cam->imgs.width, cam->imgs.height
                , cam->imgs.width - 10
                , cam->imgs.height - (10 * cam->text_scale)
                , tmp, cam->text_scale);
    }

    /* Add text in lower left corner of the pictures */
    if (cam->conf->text_left != "") {
        mystrftime(cam, tmp, sizeof(tmp), cam->conf->text_left.c_str(), NULL);
        cam->draw->text(cam->current_image->image_norm
                , cam->imgs.width, cam->imgs.height
                , 10, cam->imgs.height - (10 * cam->text_scale)
                , tmp, cam->text_scale);
    }

    /* Add text in lower right corner of the pictures */
    if (cam->conf->text_right != "") {
        mystrftime(cam, tmp, sizeof(tmp), cam->conf->text_right.c_str(), NULL);
        cam->draw->text(cam->current_image->image_norm
                , cam->imgs.width, cam->imgs.height
                , cam->imgs.width - 10, cam->imgs.height - (10 * cam->text_scale)
                , tmp, cam->text_scale);
    }
}

/* emulate motion */
static void mlp_actions_emulate(ctx_dev *cam)
{
    int indx;

    if ((cam->detecting_motion == false) &&
        (cam->movie_norm->is_running == true)) {
        cam->movie_norm->reset_start_time(&cam->current_image->imgts);
    }

    if ((cam->detecting_motion == false) &&
        (cam->movie_motion->is_running == true)) {
        cam->movie_motion->reset_start_time(&cam->imgs.image_motion.imgts);
    }

    cam->detecting_motion = true;
    if (cam->conf->post_capture > 0) {
        cam->postcap = cam->conf->post_capture;
    }

    cam->current_image->flags |= (IMAGE_TRIGGER | IMAGE_SAVE);
    /* Mark all images in image_ring to be saved */
    for (indx = 0; indx < cam->imgs.ring_size; indx++) {
        cam->imgs.image_ring[indx].flags |= IMAGE_SAVE;
    }

    mlp_detected(cam);
}

/* call the actions */
static void mlp_actions_motion(ctx_dev *cam)
{
    int indx, frame_count = 0;
    int pos = cam->imgs.ring_in;

    for (indx = 0; indx < cam->conf->minimum_motion_frames; indx++) {
        if (cam->imgs.image_ring[pos].flags & IMAGE_MOTION) {
            frame_count++;
        }
        if (pos == 0) {
            pos = cam->imgs.ring_size-1;
        } else {
            pos--;
        }
    }

    if (frame_count >= cam->conf->minimum_motion_frames) {

        cam->current_image->flags |= (IMAGE_TRIGGER | IMAGE_SAVE);

        if ((cam->detecting_motion == false) &&
            (cam->movie_norm->is_running == true)) {
            cam->movie_norm->reset_start_time(&cam->current_image->imgts);
        }
        if ((cam->detecting_motion == false) &&
            (cam->movie_motion->is_running == true)) {
            cam->movie_motion->reset_start_time(&cam->imgs.image_motion.imgts);
        }
        cam->detecting_motion = true;
        cam->postcap = cam->conf->post_capture;

        for (indx = 0; indx < cam->imgs.ring_size; indx++) {
            cam->imgs.image_ring[indx].flags |= IMAGE_SAVE;
        }

    } else if (cam->postcap > 0) {
        /* we have motion in this frame, but not enough frames for trigger. Check postcap */
        cam->current_image->flags |= (IMAGE_POSTCAP | IMAGE_SAVE);
        cam->postcap--;
    } else {
        cam->current_image->flags |= IMAGE_PRECAP;
    }

    mlp_detected(cam);
}

/* call the event actions*/
static void mlp_actions_event(ctx_dev *cam)
{
    if ((cam->conf->event_gap > 0) &&
        ((cam->frame_curr_ts.tv_sec - cam->lasttime ) >= cam->conf->event_gap)) {
        cam->event_stop = true;
    }

    if (cam->event_stop) {
        if (cam->event_curr_nbr == cam->event_prev_nbr) {

            mlp_ring_process(cam);

            if (cam->imgs.image_preview.diffs) {
                cam->picture->process_preview();
                cam->imgs.image_preview.diffs = 0;
            }
            if (cam->conf->on_event_end != "") {
                util_exec_command(cam, cam->conf->on_event_end.c_str(), NULL);
            }
            mlp_movie_end(cam);
            cam->motapp->dbse->exec(cam, "", "event_end");

            mlp_track_center(cam);

            if (cam->algsec->detected) {
                MOTPLS_LOG(NTC, TYPE_EVENTS
                    , NO_ERRNO, _("Secondary detect"));
                if (cam->conf->on_secondary_detect != "") {
                    util_exec_command(cam
                        , cam->conf->on_secondary_detect.c_str()
                        , NULL);
                }
            }
            cam->algsec->detected = false;

            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("End of event %d"), cam->event_curr_nbr);

            cam->postcap = 0;
            cam->event_curr_nbr++;
            cam->text_event_string[0] = '\0';
        }
        cam->event_stop = false;
        cam->event_user = false;
    }

    if ((cam->conf->movie_max_time > 0) &&
        (cam->event_curr_nbr == cam->event_prev_nbr) &&
        ((cam->frame_curr_ts.tv_sec - cam->movie_start_time) >=
            cam->conf->movie_max_time) &&
        ( !(cam->current_image->flags & IMAGE_POSTCAP)) &&
        ( !(cam->current_image->flags & IMAGE_PRECAP))) {
        mlp_movie_end(cam);
        mlp_movie_start(cam);
    }

}

static void mlp_actions(ctx_dev *cam)
{
     if ((cam->current_image->diffs > cam->threshold) &&
        (cam->current_image->diffs < cam->threshold_maximum)) {
        cam->current_image->flags |= IMAGE_MOTION;
        cam->info_diff_cnt++;
        cam->info_diff_tot += (uint)cam->current_image->diffs;
        cam->info_sdev_tot += (uint)cam->current_image->location.stddev_xy;
        if (cam->info_sdev_min > cam->current_image->location.stddev_xy ) {
            cam->info_sdev_min = cam->current_image->location.stddev_xy;
        }
        if (cam->info_sdev_max < cam->current_image->location.stddev_xy ) {
            cam->info_sdev_max = cam->current_image->location.stddev_xy;
        }
        /*
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO
        , "dev_x %d dev_y %d dev_xy %d, diff %d ratio %d"
        , cam->current_image->location.stddev_x
        , cam->current_image->location.stddev_y
        , cam->current_image->location.stddev_xy
        , cam->current_image->diffs
        , cam->current_image->diffs_ratio);
        */
    }

    if ((cam->conf->emulate_motion || cam->event_user) && (cam->startup_frames == 0)) {
        mlp_actions_emulate(cam);
    } else if ((cam->current_image->flags & IMAGE_MOTION) && (cam->startup_frames == 0)) {
        mlp_actions_motion(cam);
    } else if (cam->postcap > 0) {
        cam->current_image->flags |= (IMAGE_POSTCAP | IMAGE_SAVE);
        cam->postcap--;
    } else {
        cam->current_image->flags |= IMAGE_PRECAP;
        if ((cam->conf->event_gap == 0) && cam->detecting_motion) {
            cam->event_stop = true;
        }
        cam->detecting_motion = false;
    }

    if (cam->current_image->flags & IMAGE_SAVE) {
        cam->lasttime = cam->current_image->monots.tv_sec;
    }

    if (cam->detecting_motion) {
        cam->algsec->detect();
    }

    mlp_areadetect(cam);

    mlp_ring_process(cam);

    mlp_actions_event(cam);

}

/* Snapshot interval*/
static void mlp_snapshot(ctx_dev *cam)
{
    if ((cam->conf->snapshot_interval > 0 && cam->shots_mt == 0 &&
         cam->frame_curr_ts.tv_sec % cam->conf->snapshot_interval <=
         cam->frame_last_ts.tv_sec % cam->conf->snapshot_interval) ||
         cam->snapshot) {
        cam->picture->process_snapshot();
        cam->snapshot = 0;
    }
}

/* Create timelapse video*/
static void mlp_timelapse(ctx_dev *cam)
{
    struct tm timestamp_tm;

    if (cam->conf->timelapse_interval) {
        localtime_r(&cam->current_image->imgts.tv_sec, &timestamp_tm);

        if (timestamp_tm.tm_min == 0 &&
            (cam->frame_curr_ts.tv_sec % 60 < cam->frame_last_ts.tv_sec % 60) &&
            cam->shots_mt == 0) {

            if (cam->conf->timelapse_mode == "daily") {
                if (timestamp_tm.tm_hour == 0) {
                    cam->movie_timelapse->stop();
                }
            } else if (cam->conf->timelapse_mode == "hourly") {
                cam->movie_timelapse->stop();
            } else if (cam->conf->timelapse_mode == "weekly-sunday") {
                if (timestamp_tm.tm_wday == 0 && timestamp_tm.tm_hour == 0) {
                    cam->movie_timelapse->stop();
                }
            } else if (cam->conf->timelapse_mode == "weekly-monday") {
                if (timestamp_tm.tm_wday == 1 && timestamp_tm.tm_hour == 0) {
                    cam->movie_timelapse->stop();
                }
            } else if (cam->conf->timelapse_mode == "monthly") {
                if (timestamp_tm.tm_mday == 1 && timestamp_tm.tm_hour == 0) {
                    cam->movie_timelapse->stop();
                }
            }
        }

        if (cam->shots_mt == 0 &&
            cam->frame_curr_ts.tv_sec % cam->conf->timelapse_interval <=
            cam->frame_last_ts.tv_sec % cam->conf->timelapse_interval) {
            cam->movie_timelapse->start();
            if (cam->movie_timelapse->put_image(
                cam->current_image, &cam->current_image->imgts) == -1) {
                MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
            }
        }

    } else if (cam->movie_timelapse->is_running) {
    /*
     * If timelapse movie is in progress but conf.timelapse_interval is zero then close timelapse file
     * This is an important feature that allows manual roll-over of timelapse file using the http
     * remote control via a cron job.
     */
        cam->movie_timelapse->stop();
    }
}

/* send images to loopback device*/
static void mlp_loopback(ctx_dev *cam)
{

    vlp_putpipe(cam);

    if (!cam->conf->stream_motion || cam->shots_mt == 0) {
        webu_getimg_main(cam);
    }

}

/* Update parameters from web interface*/
static void mlp_parmsupdate(ctx_dev *cam)
{
    /* Check for some config parameter changes but only every second */
    if (cam->shots_mt != 0) {
        return;
    }

    if (cam->parms_changed  || (cam->passflag == false)) {
        if (cam->conf->picture_output == "on") {
            cam->new_img = NEWIMG_ON;
        } else if (cam->conf->picture_output == "first") {
            cam->new_img = NEWIMG_FIRST;
        } else if (cam->conf->picture_output == "best") {
            cam->new_img = NEWIMG_BEST;
        } else if (cam->conf->picture_output == "center") {
            cam->new_img = NEWIMG_CENTER;
        } else {
            cam->new_img = NEWIMG_OFF;
        }

        cam->parms_changed = false;
    }

}

/* sleep the loop to get framerate requested */
static void mlp_frametiming(ctx_dev *cam)
{
    int indx;
    struct timespec ts2;
    int64_t avgtime;

    /* Shuffle the last wait times*/
    for (indx=0; indx<AVGCNT-1; indx++) {
        cam->frame_wait[indx]=cam->frame_wait[indx+1];
    }

    if (cam->conf->framerate) {
        cam->frame_wait[AVGCNT-1] = 1000000L / cam->conf->framerate;
    } else {
        cam->frame_wait[AVGCNT-1] = 0;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts2);

    cam->frame_wait[AVGCNT-1] = cam->frame_wait[AVGCNT-1] -
            (1000000L * (ts2.tv_sec - cam->frame_curr_ts.tv_sec)) -
            ((ts2.tv_nsec - cam->frame_curr_ts.tv_nsec)/1000);

    avgtime = 0;
    for (indx=0; indx<AVGCNT; indx++) {
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
    cam->passflag = true;
}

/** main processing loop for each camera */
void *mlp_main(void *arg)
{
    ctx_dev *cam =(ctx_dev *) arg;

    cam->running_dev = true;

    pthread_mutex_lock(&cam->motapp->global_lock);
        cam->motapp->threads_running++;
    pthread_mutex_unlock(&cam->motapp->global_lock);

    mythreadname_set("ml",cam->threadnr,cam->conf->device_name.c_str());
    pthread_setspecific(tls_key_threadnr, (void *)((unsigned long)cam->threadnr));

    cam->finish_dev = false;
    cam->restart_dev = false;
    cam->device_status = STATUS_INIT;

    while (cam->finish_dev == false) {
        mlp_init(cam);
        mlp_prepare(cam);
        mlp_resetimages(cam);
        mlp_retry(cam);
        mlp_capture(cam);
        mlp_detection(cam);
        mlp_tuning(cam);
        mlp_overlay(cam);
        mlp_actions(cam);
        mlp_snapshot(cam);
        mlp_timelapse(cam);
        mlp_loopback(cam);
        mlp_parmsupdate(cam);
        mlp_frametiming(cam);
    }

    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Exiting"));

    mlp_cleanup(cam);

    pthread_mutex_lock(&cam->motapp->global_lock);
        cam->motapp->threads_running--;
    pthread_mutex_unlock(&cam->motapp->global_lock);

    cam->finish_dev = true;
    cam->running_dev = false;

    pthread_exit(NULL);
}

