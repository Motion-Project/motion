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
#include "util.hpp"
#include "logger.hpp"
#include "camera.hpp"
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

static void *camera_handler(void *arg)
{
    ((cls_camera *)arg)->handler();
    return nullptr;
}

/* Resize the image ring */
void cls_camera::ring_resize()
{
    int i, new_size;
    ctx_image_data *tmp;

    new_size = cfg->pre_capture + cfg->minimum_motion_frames;
    if (new_size < 1) {
        new_size = 1;
    }

    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
        ,_("Resizing buffer to %d items"), new_size);

    tmp =(ctx_image_data*) mymalloc((uint)new_size * sizeof(ctx_image_data));

    for(i = 0; i < new_size; i++) {
        tmp[i].image_norm =(u_char*) mymalloc((uint)imgs.size_norm);
        memset(tmp[i].image_norm, 0x80, (uint)imgs.size_norm);
        if (imgs.size_high > 0) {
            tmp[i].image_high =(u_char*) mymalloc((uint)imgs.size_high);
            memset(tmp[i].image_high, 0x80, (uint)imgs.size_high);
        }
    }

    imgs.image_ring = tmp;
    current_image = NULL;
    imgs.ring_size = new_size;
    imgs.ring_in = 0;
    imgs.ring_out = 0;

}

/* Clean image ring */
void cls_camera::ring_destroy()
{
    int i;

    if (imgs.image_ring == NULL) {
        return;
    }

    for (i = 0; i < imgs.ring_size; i++) {
        myfree(imgs.image_ring[i].image_norm);
        myfree(imgs.image_ring[i].image_high);
    }
    myfree(imgs.image_ring);

    /*
     * current_image is an alias from the pointers above which have
     * already been freed so we just set it equal to NULL here
    */
    current_image = NULL;

    imgs.ring_size = 0;
}

/* Add debug messsage to image */
void cls_camera::ring_process_debug()
{
    char tmp[32];
    const char *t;

    if (current_image->flags & IMAGE_TRIGGER) {
        t = "Trigger";
    } else if (current_image->flags & IMAGE_MOTION) {
        t = "Motion";
    } else if (current_image->flags & IMAGE_PRECAP) {
        t = "Precap";
    } else if (current_image->flags & IMAGE_POSTCAP) {
        t = "Postcap";
    } else {
        t = "Other";
    }

    mystrftime(this, tmp, sizeof(tmp), "%H%M%S-%q", NULL);
    draw->text(imgs.image_ring[imgs.ring_out].image_norm
            , imgs.width, imgs.height
            , 10, 20, tmp, text_scale);
    draw->text(imgs.image_ring[imgs.ring_out].image_norm
            , imgs.width, imgs.height
            , 10, 30, t, text_scale);
}

void cls_camera::ring_process_image()
{
    picture->process_norm();
    if (movie_norm->put_image(current_image
            , &current_image->imgts) == -1) {
        MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
    }
    if (movie_motion->put_image(&imgs.image_motion
            , &imgs.image_motion.imgts) == -1) {
        MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
    }
    if (movie_extpipe->put_image(current_image
            , &current_image->imgts) == -1) {
        MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
    }
}

/* Process the entire image ring */
void cls_camera::ring_process()
{
    ctx_image_data *saved_current_image = current_image;

    do {
        if ((imgs.image_ring[imgs.ring_out].flags & (IMAGE_SAVE | IMAGE_SAVED)) != IMAGE_SAVE) {
            break;
        }

        current_image = &imgs.image_ring[imgs.ring_out];

        if (current_image->shot <= cfg->framerate) {
            if (app->cfg->log_level >= DBG) {
                ring_process_debug();
            }
            ring_process_image();
        }

        current_image->flags |= IMAGE_SAVED;

        if (current_image->flags & IMAGE_MOTION) {
            if (cfg->picture_output == "best") {
                if (current_image->diffs > imgs.image_preview.diffs) {
                    picture->save_preview();
                }
            }
            if (cfg->picture_output == "center") {
                if (current_image->cent_dist < imgs.image_preview.cent_dist) {
                    picture->save_preview();
                }
            }
        }

        if (++imgs.ring_out >= imgs.ring_size) {
            imgs.ring_out = 0;
        }

    } while (imgs.ring_out != imgs.ring_in);

    current_image = saved_current_image;
}

/* Reset the image info variables*/
void cls_camera::info_reset()
{
    info_diff_cnt = 0;
    info_diff_tot = 0;
    info_sdev_min = 99999999;
    info_sdev_max = 0;
    info_sdev_tot = 0;
}

void cls_camera::movie_start()
{
    movie_start_time = frame_curr_ts.tv_sec;
    if (lastrate < 2) {
        movie_fps = 2;
    } else {
        movie_fps = lastrate;
    }
    movie_norm->start();
    movie_motion->start();
    movie_extpipe->start();
}

void cls_camera::movie_end()
{
    movie_norm->stop();
    movie_motion->stop();
    movie_extpipe->stop();
}

/* Process the motion detected items*/
void cls_camera::detected_trigger()
{
    time_t raw_time;
    struct tm evt_tm;

    if (current_image->flags & IMAGE_TRIGGER) {
        if (event_curr_nbr != event_prev_nbr) {
            info_reset();
            event_prev_nbr = event_curr_nbr;

            algsec->detected = false;

            time(&raw_time);
            localtime_r(&raw_time, &evt_tm);
            sprintf(eventid, "%05d", cfg->device_id);
            strftime(eventid+5, 15, "%Y%m%d%H%M%S", &evt_tm);

            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motion detected - starting event %d"),
                       event_curr_nbr);

            mystrftime(this, text_event_string
                , sizeof(text_event_string)
                , cfg->text_event.c_str(), NULL);

            if (cfg->on_event_start != "") {
                util_exec_command(this, cfg->on_event_start.c_str(), NULL);
            }
            movie_start();
            app->dbse->exec(this, "", "event_start");

            if ((cfg->picture_output == "first") ||
                (cfg->picture_output == "best") ||
                (cfg->picture_output == "center")) {
                picture->save_preview();
            }

        }
        if (cfg->on_motion_detected != "") {
            util_exec_command(this, cfg->on_motion_detected.c_str(), NULL);
        }
    }
}

/* call ptz camera center */
void cls_camera::track_center()
{
    if ((cfg->ptz_auto_track) && (cfg->ptz_move_track != "")) {
        track_posx = 0;
        track_posy = 0;
        util_exec_command(this, cfg->ptz_move_track.c_str(), NULL);
        frame_skip = cfg->ptz_wait;
    }
}

/* call ptz camera move */
void cls_camera::track_move()
{
    if ((cfg->ptz_auto_track) && (cfg->ptz_move_track != "")) {
            track_posx += current_image->location.x;
            track_posy += current_image->location.y;
            util_exec_command(this, cfg->ptz_move_track.c_str(), NULL);
            frame_skip = cfg->ptz_wait;
    }
}

/* motion detected */
void cls_camera::detected()
{
    unsigned int distX, distY;

    draw->locate();

    /* Calculate how centric motion is if configured preview center*/
    if (cfg->picture_output == "center") {
        distX = (uint)abs((imgs.width / 2) - current_image->location.x );
        distY = (uint)abs((imgs.height / 2) - current_image->location.y);
        current_image->cent_dist = distX * distX + distY * distY;
    }

    detected_trigger();

    if (current_image->shot <= cfg->framerate) {
        if ((cfg->stream_motion == true) &&
            (current_image->shot != 1)) {
            webu_getimg_main(this);
        }
        picture->process_motion();
    }

    track_move();
}

/* Apply the privacy mask to image*/
void cls_camera::mask_privacy()
{
    if (imgs.mask_privacy == NULL) {
        return;
    }

    /*
    * This function uses long operations to process 4 (32 bit) or 8 (64 bit)
    * bytes at a time, providing a significant boost in performance.
    * Then a trailer loop takes care of any remaining bytes.
    */
    u_char *image;
    const u_char *mask;
    const u_char *maskuv;

    int index_y;
    int index_crcb;
    int increment;
    int indx_img;                /* Counter for how many images we need to apply the mask to */
    int indx_max;                /* 1 if we are only doing norm, 2 if we are doing both norm and high */

    indx_img = 1;
    if (imgs.size_high > 0) {
        indx_max = 2;
    } else {
        indx_max = 1;
    }
    increment = sizeof(unsigned long);

    while (indx_img <= indx_max) {
        if (indx_img == 1) {
            /* Normal Resolution */
            index_y = imgs.height * imgs.width;
            image = current_image->image_norm;
            mask = imgs.mask_privacy;
            index_crcb = imgs.size_norm - index_y;
            maskuv = imgs.mask_privacy_uv;
        } else {
            /* High Resolution */
            index_y = imgs.height_high * imgs.width_high;
            image = current_image->image_high;
            mask = imgs.mask_privacy_high;
            index_crcb = imgs.size_high - index_y;
            maskuv = imgs.mask_privacy_high_uv;
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
void cls_camera::cam_close()
{
    mydelete(libcam);
    mydelete(v4l2cam);
    mydelete(netcam);
    mydelete(netcam_high);
    device_status = STATUS_CLOSED;
}

/* Start camera */
void cls_camera::cam_start()
{
    watchdog = cfg->watchdog_tmo;
    if (camera_type == CAMERA_TYPE_LIBCAM) {
        libcam = new cls_libcam(this);
    } else if (camera_type == CAMERA_TYPE_NETCAM) {
        netcam = new cls_netcam(this, false);
        netcam->netcam_start();
        if (cfg->netcam_high_url != "") {
            watchdog = cfg->watchdog_tmo;
            netcam_high = new cls_netcam(this, true);
            netcam_high->netcam_start();
        }
    } else if (camera_type == CAMERA_TYPE_V4L2) {
        v4l2cam = new cls_v4l2cam(this);
    } else {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("No Camera device specified"));
        device_status = STATUS_CLOSED;
    }
    watchdog = cfg->watchdog_tmo;
}

/* Get next image from camera */
int cls_camera::cam_next(ctx_image_data *img_data)
{
    int retcd, imgsz;

    if (device_status != STATUS_OPENED) {
        return CAPTURE_FAILURE;
    }

    if (camera_type == CAMERA_TYPE_LIBCAM) {
        retcd = libcam->next(img_data);
    } else if (camera_type == CAMERA_TYPE_NETCAM) {
        retcd = netcam->next(img_data);
        if ((retcd == CAPTURE_SUCCESS) &&
            (netcam_high != nullptr)) {
            retcd = netcam_high->next(img_data);
        }
        rotate->process(img_data);
    } else if (camera_type == CAMERA_TYPE_V4L2) {
        retcd = v4l2cam->next(img_data);
    } else {
        return CAPTURE_FAILURE;
    }

    if (retcd == CAPTURE_SUCCESS) {
        imgsz = (imgs.width * imgs.height * 3) / 2;
        if (imgs.size_norm != imgsz) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Resetting image buffers"));
            device_status = STATUS_CLOSED;
            restart = true;
            return CAPTURE_FAILURE;
        }
        imgsz = (imgs.width_high * imgs.height_high * 3) / 2;
        if (imgs.size_high != imgsz) {
            device_status = STATUS_CLOSED;
            restart = true;
            return CAPTURE_FAILURE;
        }
    }

    return retcd;

}

/* Assign the camera type */
void cls_camera::init_camera_type()
{
    if (cfg->libcam_device != "") {
        camera_type = CAMERA_TYPE_LIBCAM;
    } else if (cfg->netcam_url != "") {
        camera_type = CAMERA_TYPE_NETCAM;
    } else if (cfg->v4l2_device != "") {
        camera_type = CAMERA_TYPE_V4L2;
    } else {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("Unable to determine camera type"));
        camera_type = CAMERA_TYPE_UNKNOWN;
        handler_stop = true;
        restart = false;
    }
}

/** Get first images from camera at startup */
void cls_camera::init_firstimage()
{
    int indx;

    current_image = &imgs.image_ring[imgs.ring_in];
    if (device_status == STATUS_OPENED) {
        for (indx = 0; indx < 5; indx++) {
            if (cam_next(current_image) == CAPTURE_SUCCESS) {
                break;
            }
            SLEEP(2, 0);
        }
    } else {
        indx = 0;
    }

    if (finish == true) {
        return;
    }

    if ((indx >= 5) || (device_status != STATUS_OPENED)) {
        if (device_status != STATUS_OPENED) {
            MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO, "%s", "Unable to open camera");
        } else {
            MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO, "%s", "Error capturing first image");
        }
        for (indx = 0; indx<imgs.ring_size; indx++) {
            memset(imgs.image_ring[indx].image_norm
                , 0x80, (uint)imgs.size_norm);
        }
    }

    noise = cfg->noise_level;
    threshold = cfg->threshold;
    if (cfg->threshold_maximum > cfg->threshold ) {
        threshold_maximum = cfg->threshold_maximum;
    } else {
        threshold_maximum = (imgs.height * imgs.width * 3) / 2;
    }

}

/** Check the image size to determine if modulo 8 and over 64 */
void cls_camera::check_szimg()
{
    if ((imgs.width % 8) || (imgs.height % 8)) {
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("Image width (%d) or height(%d) requested is not modulo 8.")
            ,imgs.width, imgs.height);
        device_status = STATUS_CLOSED;
    }
    if ((imgs.width  < 64) || (imgs.height < 64)) {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Motion only supports width and height greater than or equal to 64 %dx%d")
            ,imgs.width, imgs.height);
        device_status = STATUS_CLOSED;
    }
    /* Substream size notification*/
    if ((imgs.width % 16) || (imgs.height % 16)) {
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("Substream not available.  Image sizes not modulo 16."));
    }

}

/** Set the items required for the area detect */
void cls_camera::init_areadetect()
{
    area_minx[0] = area_minx[3] = area_minx[6] = 0;
    area_miny[0] = area_miny[1] = area_miny[2] = 0;

    area_minx[1] = area_minx[4] = area_minx[7] = imgs.width / 3;
    area_maxx[0] = area_maxx[3] = area_maxx[6] = imgs.width / 3;

    area_minx[2] = area_minx[5] = area_minx[8] = imgs.width / 3 * 2;
    area_maxx[1] = area_maxx[4] = area_maxx[7] = imgs.width / 3 * 2;

    area_miny[3] = area_miny[4] = area_miny[5] = imgs.height / 3;
    area_maxy[0] = area_maxy[1] = area_maxy[2] = imgs.height / 3;

    area_miny[6] = area_miny[7] = area_miny[8] = imgs.height / 3 * 2;
    area_maxy[3] = area_maxy[4] = area_maxy[5] = imgs.height / 3 * 2;

    area_maxx[2] = area_maxx[5] = area_maxx[8] = imgs.width;
    area_maxy[6] = area_maxy[7] = area_maxy[8] = imgs.height;

    areadetect_eventnbr = 0;
}

/** Allocate the required buffers */
void cls_camera::init_buffers()
{
    imgs.ref =(u_char*) mymalloc((uint)imgs.size_norm);
    imgs.image_motion.image_norm = (u_char*)mymalloc((uint)imgs.size_norm);
    imgs.ref_dyn =(int*) mymalloc((uint)imgs.motionsize * sizeof(*imgs.ref_dyn));
    imgs.image_virgin =(u_char*) mymalloc((uint)imgs.size_norm);
    imgs.image_vprvcy = (u_char*)mymalloc((uint)imgs.size_norm);
    imgs.labels =(int*)mymalloc((uint)imgs.motionsize * sizeof(*imgs.labels));
    imgs.labelsize =(int*) mymalloc((uint)(imgs.motionsize/2+1) * sizeof(*imgs.labelsize));
    imgs.image_preview.image_norm =(u_char*) mymalloc((uint)imgs.size_norm);
    imgs.common_buffer =(u_char*) mymalloc((uint)(3 * imgs.width * imgs.height));
    imgs.image_secondary =(u_char*) mymalloc((uint)(3 * imgs.width * imgs.height));
    if (imgs.size_high > 0) {
        imgs.image_preview.image_high =(u_char*) mymalloc((uint)imgs.size_high);
    } else {
        imgs.image_preview.image_high = NULL;
    }

}

/* Initialize loop values */
void cls_camera::init_values()
{
    event_curr_nbr = 1;
    event_prev_nbr = 0;

    watchdog = cfg->watchdog_tmo;

    clock_gettime(CLOCK_MONOTONIC, &frame_curr_ts);
    clock_gettime(CLOCK_MONOTONIC, &frame_last_ts);

    noise = cfg->noise_level;
    passflag = false;
    threshold = cfg->threshold;
    device_status = STATUS_CLOSED;
    startup_frames = (cfg->framerate * 2) + cfg->pre_capture + cfg->minimum_motion_frames;
    missing_frame_counter = 0;
    frame_skip = 0;
    detecting_motion = false;
    shots_mt = 0;
    lastrate = cfg->framerate;
    event_user = false;
    lasttime = frame_curr_ts.tv_sec;
    postcap = 0;
    event_stop = false;
    text_scale = cfg->text_scale;
    connectionlosttime.tv_sec = 0;
    connectionlosttime.tv_nsec = 0;
    info_reset();

    movie_passthrough = cfg->movie_passthrough;
    if ((camera_type != CAMERA_TYPE_NETCAM) &&
        (movie_passthrough)) {
        MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Pass-through processing disabled."));
        movie_passthrough = false;
    }

    user_pause = false;
    if (app->user_pause) {
        user_pause = true;
    } else if (cfg->pause) {
        user_pause = true;
    }

    pause = user_pause;
    v4l2cam = nullptr;
    netcam = nullptr;
    netcam_high = nullptr;
    libcam = nullptr;
    rotate = nullptr;
    picture = nullptr;
    movie_norm = nullptr;
    movie_norm = nullptr;
    movie_motion = nullptr;
    movie_timelapse = nullptr;
    movie_extpipe = nullptr;
    draw = nullptr;
    cleandir = nullptr;

    gethostname (hostname, PATH_MAX);
    hostname[PATH_MAX-1] = '\0';

    memset(&imgs,0,sizeof(ctx_images));

}

/* start the camera */
void cls_camera::init_cam_start()
{
    cam_start();

    if (device_status == STATUS_CLOSED) {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO,_("Failed to start camera."));
        imgs.width = cfg->width;
        imgs.height = cfg->height;
    }

    imgs.motionsize = (imgs.width * imgs.height);
    imgs.size_norm  = (imgs.width * imgs.height * 3) / 2;
    imgs.size_high  = (imgs.width_high * imgs.height_high * 3) / 2;
    imgs.labelsize_max = 0;
    imgs.largest_label = 0;
}

/* initialize reference images*/
void cls_camera::init_ref()
{
    memcpy(imgs.image_virgin, current_image->image_norm
        , (uint)imgs.size_norm);

    mask_privacy();

    memcpy(imgs.image_vprvcy, current_image->image_norm
        , (uint)imgs.size_norm);

    alg->ref_frame_reset();
}

/** clean up all memory etc. from motion init */
void cls_camera::cleanup()
{
    movie_timelapse->stop();
    if (event_curr_nbr == event_prev_nbr) {
        ring_process();
        if (imgs.image_preview.diffs) {
            picture->process_preview();
            imgs.image_preview.diffs = 0;
        }
        if (cfg->on_event_end != "") {
            util_exec_command(this, cfg->on_event_end.c_str(), NULL);
        }
        movie_end();
        app->dbse->exec(this, "", "event_end");
    }

    webu_getimg_deinit(this);

    cam_close();

    myfree(imgs.image_motion.image_norm);
    myfree(imgs.ref);
    myfree(imgs.ref_dyn);
    myfree(imgs.image_virgin);
    myfree(imgs.image_vprvcy);
    myfree(imgs.labels);
    myfree(imgs.labelsize);
    myfree(imgs.mask);
    myfree(imgs.mask_privacy);
    myfree(imgs.mask_privacy_uv);
    myfree(imgs.mask_privacy_high);
    myfree(imgs.mask_privacy_high_uv);
    myfree(imgs.common_buffer);
    myfree(imgs.image_secondary);
    myfree(imgs.image_preview.image_norm);
    myfree(imgs.image_preview.image_high);

    ring_destroy(); /* Cleanup the precapture ring buffer */

    mydelete(alg);
    mydelete(algsec);
    mydelete(rotate);
    mydelete(picture);
    mydelete(movie_norm);
    mydelete(movie_motion);
    mydelete(movie_timelapse);
    mydelete(movie_extpipe);
    mydelete(draw);
    mydelete(cleandir);

    if (pipe != -1) {
        close(pipe);
        pipe = -1;
    }

    if (mpipe != -1) {
        close(mpipe);
        mpipe = -1;
    }

}

void cls_camera::init_cleandir_default()
{
    if ((cleandir->action != "delete") &&
        (cleandir->action != "script")) {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Invalid clean directory action : %s")
            ,cleandir->action.c_str());
        cleandir->action = "";
    }
    if (cleandir->action == "") {
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Setting default clean directory action to delete."));
        cleandir->action = "delete";
    }

    if ((cleandir->freq != "hourly") &&
        (cleandir->freq != "daily") &&
        (cleandir->freq != "weekly")) {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Invalid clean directory freq : %s")
            ,cleandir->freq.c_str());
        cleandir->freq = "";
    }
    if (cleandir->freq == "") {
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Setting default clean directory frequency to weekly."));
        cleandir->freq = "weekly";
    }

    if (cleandir->runtime == "") {
        if (cleandir->freq == "hourly") {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Setting default clean directory runtime to 15."));
            cleandir->runtime = "15";
        } else if (cleandir->freq == "daily") {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Setting default clean directory runtime to 0115."));
            cleandir->runtime = "0115";
        } else if (cleandir->freq == "weekly") {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Setting default clean directory runtime to mon-0115."));
            cleandir->runtime = "mon-0115";
        }
    }

    if ((cleandir->dur_unit != "m") &&
        (cleandir->dur_unit != "h") &&
        (cleandir->dur_unit != "d") &&
        (cleandir->dur_unit != "w")) {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Invalid clean directory duration unit : %s")
            ,cleandir->dur_unit.c_str());
        cleandir->dur_unit = "";
    }
    if (cleandir->dur_unit == "") {
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Setting default clean directory duration unit to d."));
        cleandir->dur_unit = "d";
    }

    if ((cleandir->dur_val == 0 )) {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Invalid clean directory duration number : %d")
            ,cleandir->dur_val);
        cleandir->dur_val = -1;
    }
    if (cleandir->dur_val <= 0) {
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Setting default clean directory duration value to 7."));
        cleandir->dur_val = 7;
    }
}

void cls_camera::init_cleandir_runtime()
{
    std::string ptst,punit,premove;
    struct tm c_tm;
    struct timespec curr_ts;
    int p_min, p_hr, p_dow;

    clock_gettime(CLOCK_REALTIME, &curr_ts);
    localtime_r(&curr_ts.tv_sec, &c_tm);

    cleandir->next_ts.tv_nsec = 0;
    cleandir->next_ts.tv_sec = curr_ts.tv_sec;

    if (cleandir->freq == "hourly") {
        punit = "hours";
        if (cleandir->runtime.length() < 2) {
            p_min = -1;
        } else {
            p_min = mtoi(cleandir->runtime);
        }
        if ((p_min > 59) || (p_min < 0)) {
            MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("Invalid clean directory hourly runtime : %s")
                ,cleandir->runtime.c_str());
            cleandir->runtime = "";
            p_min = 15;
        }
        cleandir->next_ts.tv_sec += ((p_min - c_tm.tm_min) * 60);
        if (cleandir->next_ts.tv_sec < curr_ts.tv_sec) {
            cleandir->next_ts.tv_sec += 3600;
        }

    } else if (cleandir->freq == "daily") {
        punit = "days";
        if (cleandir->runtime.length() < 4) {
            p_min = -1;
        } else {
            p_hr = mtoi(cleandir->runtime.substr(0,2));
            p_min = mtoi(cleandir->runtime.substr(2,2));
        }
        if ((p_min > 59) || (p_min < 0) ||
            (p_hr > 23)  || (p_hr < 0)) {
            MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("Invalid clean directory daily runtime : %s")
                ,cleandir->runtime.c_str());
            cleandir->runtime = "";
            p_min = 15;
            p_hr = 1;
        }
        cleandir->next_ts.tv_sec += ((p_min - c_tm.tm_min) * 60);
        cleandir->next_ts.tv_sec += ((p_hr - c_tm.tm_hour) * 3600);
        if (cleandir->next_ts.tv_sec < curr_ts.tv_sec) {
            cleandir->next_ts.tv_sec += (60 * 60 * 24);
        }

    } else if (cleandir->freq == "weekly") {
        punit = "weeks";
        if (cleandir->runtime.length() < 8) {
            p_min = -1;
        } else {
            ptst = cleandir->runtime.substr(0,3);   /*"mon-0115"*/
            if (ptst == "sun") {        p_dow = 0;
            } else if (ptst == "mon") { p_dow = 1;
            } else if (ptst == "tue") { p_dow = 2;
            } else if (ptst == "wed") { p_dow = 3;
            } else if (ptst == "thu") { p_dow = 4;
            } else if (ptst == "fri") { p_dow = 5;
            } else if (ptst == "sat") { p_dow = 6;
            } else {                    p_dow= -1;
            }
            p_hr = mtoi(cleandir->runtime.substr(4,2));
            p_min = mtoi(cleandir->runtime.substr(6,2));
        }
        if ((p_min > 59) || (p_min < 0) ||
            (p_hr > 23)  || (p_hr < 0) ||
            (p_dow == -1)) {
            MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("Invalid clean directory weekly runtime : %s")
                ,cleandir->runtime.c_str());
            cleandir->runtime = "";
            p_min = 15;
            p_hr = 1;
            p_dow = 1;
        }
        cleandir->next_ts.tv_sec += ((p_min - c_tm.tm_min) * 60);
        cleandir->next_ts.tv_sec += ((p_hr - c_tm.tm_hour) * 3600);
        cleandir->next_ts.tv_sec += ((p_dow - c_tm.tm_wday) * 86400);
        if (cleandir->next_ts.tv_sec < curr_ts.tv_sec) {
            cleandir->next_ts.tv_sec += (60 * 60 * 24 * 7);
        }
    }

    localtime_r(&cleandir->next_ts.tv_sec, &c_tm);
    cleandir->next_ts.tv_sec -= c_tm.tm_sec;

    if (cleandir->action == "delete") {
        MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO
            , _("Cleandir next run:%04d-%02d-%02d %02d:%02d Criteria:%d%s RemoveDir:%s")
            ,c_tm.tm_year+1900,c_tm.tm_mon+1,c_tm.tm_mday
            ,c_tm.tm_hour,c_tm.tm_min
            ,cleandir->dur_val
            ,cleandir->dur_unit.c_str()
            ,cleandir->removedir ? "Y":"N");
    } else {
        MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO
            , _("Clean directory set to run script at %04d-%02d-%02d %02d:%02d")
            ,c_tm.tm_year+1900,c_tm.tm_mon+1,c_tm.tm_mday
            ,c_tm.tm_hour,c_tm.tm_min);
    }

}

void cls_camera::init_cleandir()
{
    int indx;
    ctx_params  *params;
    std::string  pnm, pvl;

    if (cfg->cleandir_params == "") {
        return;
    }

    params = new ctx_params;
    util_parms_parse(params, "cleandir_params", cfg->cleandir_params);

    if (params->params_cnt == 0) {
        mydelete(params);
        return;
    }

    cleandir = new ctx_cleandir;
    cleandir->action = "delete";
    cleandir->freq = "weekly";
    cleandir->script = "";
    cleandir->runtime = "mon-0115";
    cleandir->removedir = false;
    cleandir->dur_unit = "w";
    cleandir->dur_val = 2;

    for (indx=0; indx<params->params_cnt; indx++) {
        pnm = params->params_array[indx].param_name;
        pvl = params->params_array[indx].param_value;
        if (pnm == "runtime") {
            cleandir->runtime = pvl;
        }
        if (pnm == "freq") {
            cleandir->freq = pvl;
        }
        if (pnm == "action") {
            cleandir->action = pvl;
        }
        if (pnm == "script") {
            cleandir->script = pvl;
        }
        if (pnm == "dur_val") {
            cleandir->dur_val =mtoi(pvl);
        }
        if (pnm == "dur_unit") {
            cleandir->dur_unit = pvl;
        }
        if (pnm == "removedir") {
            cleandir->removedir = mtob(pvl);
        }
    }
    init_cleandir_default();
    init_cleandir_runtime();

}

void cls_camera::init_schedule()
{
    int indx, indx1;
    bool dflt_detect, rev_detect;
    ctx_params  *params;
    std::string  pnm, pvl, tst, action;
    std::vector<ctx_schedule_data> sched_day;
    ctx_schedule_data sched_itm;

    if (cfg->schedule_params == "") {
        return;
    }

    params = new ctx_params;
    util_parms_parse(params, "schedule_params", cfg->schedule_params);

    if (params->params_cnt == 0) {
        mydelete(params);
        return;
    }

    action = "pause";
    dflt_detect = true;
    for (indx=0; indx<params->params_cnt; indx++) {
        pnm = params->params_array[indx].param_name;
        pvl = params->params_array[indx].param_value;
        if (pnm == "default") {
            dflt_detect = mtob(params->params_array[indx].param_value);
        }
        if (pnm == "action") {
            if (pvl == "stop") {
                action = "stop";
            } else {
                action = "pause";
            }
        }
    }

    if (dflt_detect) {
        rev_detect = false;
    } else {
        rev_detect = true;
    }

    for (indx=0; indx<7; indx++) {
        sched_day.clear();

        sched_itm.st_hr = 0;
        sched_itm.st_min = 0;
        sched_itm.en_hr = 23;
        sched_itm.en_min = 59;
        sched_itm.action = action;
        sched_itm.detect = dflt_detect;
        sched_day.push_back(sched_itm);

        sched_itm.st_hr = -1;
        sched_itm.st_min = -1;
        sched_itm.en_hr = -1;
        sched_itm.en_min = -1;
        sched_itm.action = action;
        sched_itm.detect = rev_detect;

        if (indx == 0) {        tst = "sun";
        } else if (indx == 1) { tst = "mon";
        } else if (indx == 2) { tst = "tue";
        } else if (indx == 3) { tst = "wed";
        } else if (indx == 4) { tst = "thu";
        } else if (indx == 5) { tst = "fri";
        } else if (indx == 6) { tst = "sat";
        }
        for (indx1=0; indx1<params->params_cnt; indx1++) {
            pnm = params->params_array[indx1].param_name;
            pvl = params->params_array[indx1].param_value;
            if ((pnm == tst) || (pnm == "sun-sat") ||
                ((pnm == "mon-fri") && (indx>=1) && (indx<=5))) {
                //example:  mon 0902-1430
                sched_itm.st_hr = mtoi(pvl.substr(0,2));
                sched_itm.st_min = mtoi(pvl.substr(2,2));
                sched_itm.en_hr = mtoi(pvl.substr(5,2));
                sched_itm.en_min = mtoi(pvl.substr(7,2));
                sched_itm.action = action;
                sched_itm.detect = rev_detect;
                if ((sched_itm.st_hr  < 0) || (sched_itm.st_hr  > 23) ||
                    (sched_itm.st_min < 0) || (sched_itm.st_min > 59) ||
                    (sched_itm.en_hr  < 0) || (sched_itm.en_hr  > 23) ||
                    (sched_itm.en_min < 0) || (sched_itm.en_min > 59) ||
                    (sched_itm.st_hr  > sched_itm.en_hr) ||
                    (pvl.length() != 9)) {
                    MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                        ,_("Invalid schedule parameter: %s %s")
                        , pnm.c_str(), pvl.c_str());
                } else {
                    sched_day.push_back(sched_itm);
                }
            }
        }
        schedule.push_back(sched_day);
    }

    for (indx=0; indx<7; indx++) {
        if (indx == 0) {        tst = "sun";
        } else if (indx == 1) { tst = "mon";
        } else if (indx == 2) { tst = "tue";
        } else if (indx == 3) { tst = "wed";
        } else if (indx == 4) { tst = "thu";
        } else if (indx == 5) { tst = "fri";
        } else if (indx == 6) { tst = "sat";
        }
        for (indx1=0; indx1<schedule[indx].size(); indx1++) {
            if ((schedule[indx][indx1].action == "stop") &&
                (schedule[indx][indx1].detect == false)) {
                action = "stopped";
            } else if (sched_day[indx1].detect == false) {
               action = "paused";
            } else {
                action = "active";
            }
            MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO
                ,_("Schedule: %s %02d:%02d to %02d:%02d %s")
                ,tst.c_str()
                ,sched_day[indx1].st_hr
                ,sched_day[indx1].st_min
                ,sched_day[indx1].en_hr
                ,sched_day[indx1].en_min
                ,action.c_str()
                );
        }
    }

    mydelete(params);
}

/* initialize everything for the loop */
void cls_camera::init()
{
    if (((device_status != STATUS_INIT) && (restart == false)) ||
        (handler_stop == true)) {
        return;
    }

    if (restart == true) {
        cleanup();
        restart = false;
    }

    cfg->parms_copy(conf_src);

    mythreadname_set("cl",cfg->device_id, cfg->device_name.c_str());

    MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO,_("Initialize Camera"));

    init_camera_type();

    init_values();

    init_cam_start();

    check_szimg();

    ring_resize();

    init_buffers();

    webu_getimg_init(this);

    rotate = new cls_rotate(this);

    init_firstimage();

    vlp_init(this);

    alg = new cls_alg(this);
    algsec = new cls_algsec(this);
    picture = new cls_picture(this);
    draw = new cls_draw(this);
    movie_norm = new cls_movie(this, "norm");
    movie_motion = new cls_movie(this, "motion");
    movie_timelapse = new cls_movie(this, "timelapse");
    movie_extpipe = new cls_movie(this, "extpipe");

    init_cleandir();

    init_schedule();

    init_areadetect();

    init_ref();

    if (device_status == STATUS_OPENED) {
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Camera %d started: motion detection %s"),
            cfg->device_id, pause ? _("disabled"):_("enabled"));

        if (cfg->emulate_motion) {
            MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, _("Emulating motion"));
        }
    }

}

/* check the area detect */
void cls_camera::areadetect()
{
    int i, j, z = 0;

    if ((cfg->area_detect != "" ) &&
        (event_curr_nbr != areadetect_eventnbr) &&
        (current_image->flags & IMAGE_TRIGGER)) {
        j = (int)cfg->area_detect.length();
        for (i = 0; i < j; i++) {
            z = cfg->area_detect[(uint)i] - 49; /* characters are stored as ascii 48-57 (0-9) */
            if ((z >= 0) && (z < 9)) {
                if (current_image->location.x > area_minx[z] &&
                    current_image->location.x < area_maxx[z] &&
                    current_image->location.y > area_miny[z] &&
                    current_image->location.y < area_maxy[z]) {
                    if (cfg->on_area_detected != "") {
                        util_exec_command(this, cfg->on_area_detected.c_str(), NULL);
                    }
                    areadetect_eventnbr = event_curr_nbr; /* Fire script only once per event */
                    MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO
                        ,_("Motion in area %d detected."), z + 1);
                    break;
                }
            }
        }
    }
}

/* Prepare for the next iteration of loop*/
void cls_camera::prepare()
{
    if ((restart == true) || (handler_stop == true)) {
        return;
    }

    watchdog = cfg->watchdog_tmo;

    frame_last_ts.tv_sec = frame_curr_ts.tv_sec;
    frame_last_ts.tv_nsec = frame_curr_ts.tv_nsec;
    clock_gettime(CLOCK_MONOTONIC, &frame_curr_ts);

    if (frame_last_ts.tv_sec != frame_curr_ts.tv_sec) {
        lastrate = shots_mt + 1;
        shots_mt = -1;
    }
    shots_mt++;

    if (cfg->pre_capture < 0) {
        cfg->pre_capture = 0;
    }

    if (startup_frames > 0) {
        startup_frames--;
    }
}

void cls_camera::resetimages()
{
    int64_t tmpsec;

    if ((restart == true) || (handler_stop == true)) {
        return;
    }

    /* ring_buffer_in is pointing to current pos, update before put in a new image */
    tmpsec =current_image->imgts.tv_sec;
    if (++imgs.ring_in >= imgs.ring_size) {
        imgs.ring_in = 0;
    }

    /* Check if we have filled the ring buffer, throw away last image */
    if (imgs.ring_in == imgs.ring_out) {
        if (++imgs.ring_out >= imgs.ring_size) {
            imgs.ring_out = 0;
        }
    }

    current_image = &imgs.image_ring[imgs.ring_in];
    current_image->diffs = 0;
    current_image->flags = 0;
    current_image->cent_dist = 0;
    memset(&current_image->location, 0, sizeof(current_image->location));
    current_image->total_labels = 0;

    clock_gettime(CLOCK_REALTIME, &current_image->imgts);
    clock_gettime(CLOCK_MONOTONIC, &current_image->monots);

    if (tmpsec != current_image->imgts.tv_sec) {
        shots_rt = 1;
    }  else {
        shots_rt++;
    }
    /* Store shot number with pre_captured image */
    current_image->shot = shots_rt;

}

int cls_camera::capture()
{
    const char *tmpin;
    char tmpout[80];
    int retcd;

    retcd = cam_next(current_image);

    if ((restart == true) || (handler_stop == true)) {
        return 0;
    }

    if (retcd == CAPTURE_SUCCESS) {
        watchdog = cfg->watchdog_tmo;
        lost_connection = false;
        connectionlosttime.tv_sec = 0;

        if (missing_frame_counter >= (cfg->device_tmo * cfg->framerate)) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Video signal re-acquired"));
            if (cfg->on_camera_found != "") {
                util_exec_command(this, cfg->on_camera_found.c_str(), NULL);
            }
        }
        missing_frame_counter = 0;
        memcpy(imgs.image_virgin, current_image->image_norm
            , (uint)imgs.size_norm);
        mask_privacy();
        memcpy(imgs.image_vprvcy, current_image->image_norm
            , (uint)imgs.size_norm);

    } else {
        if (connectionlosttime.tv_sec == 0) {
            clock_gettime(CLOCK_REALTIME, &connectionlosttime);
        }

        missing_frame_counter++;

        if ((device_status == STATUS_OPENED) &&
            (missing_frame_counter <
                (cfg->device_tmo * cfg->framerate))) {
            memcpy(current_image->image_norm, imgs.image_vprvcy
                , (uint)imgs.size_norm);
        } else {
            lost_connection = true;
            if (device_status == STATUS_OPENED) {
                tmpin = "CONNECTION TO CAMERA LOST\\nSINCE %Y-%m-%d %T";
            } else {
                tmpin = "UNABLE TO OPEN VIDEO DEVICE\\nSINCE %Y-%m-%d %T";
            }

            memset(current_image->image_norm, 0x80, (uint)imgs.size_norm);
            current_image->imgts =connectionlosttime;
            mystrftime(this, tmpout, sizeof(tmpout), tmpin, NULL);
            draw->text(current_image->image_norm
                    , imgs.width, imgs.height
                    , 10, 20 * text_scale
                    , tmpout, text_scale);

            /* Write error message only once */
            if (missing_frame_counter == (cfg->device_tmo * cfg->framerate)) {
                MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                    ,_("Video signal lost - Adding grey image"));
                if (cfg->on_camera_lost != "") {
                    util_exec_command(this, cfg->on_camera_lost.c_str(), NULL);
                }
            }

            if (camera_type == CAMERA_TYPE_LIBCAM) {
                libcam->noimage();
            } else if (camera_type == CAMERA_TYPE_NETCAM) {
                netcam->noimage();
            } else if (camera_type == CAMERA_TYPE_V4L2) {
                v4l2cam->noimage();
            } else {
                MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("Unknown camera type"));
            }
        }
    }
    return 0;

}

/* call detection */
void cls_camera::detection()
{
    if ((restart == true) || (handler_stop == true)) {
        return;
    }

    if (frame_skip) {
        frame_skip--;
        current_image->diffs = 0;
        return;
    }

    if (pause == false) {
        alg->diff();
    } else {
        current_image->diffs = 0;
        current_image->diffs_raw = 0;
        current_image->diffs_ratio = 100;
    }
}

/* tune the detection parameters*/
void cls_camera::tuning()
{
    if ((restart == true) || (handler_stop == true)) {
        return;
    }

    if ((cfg->noise_tune && shots_mt == 0) &&
          (!detecting_motion && (current_image->diffs <= threshold))) {
        alg->noise_tune();
    }

    if (cfg->threshold_tune) {
        alg->threshold_tune();
    }

    if ((current_image->diffs > threshold) &&
        (current_image->diffs < threshold_maximum)) {
        alg->location();
        alg->stddev();

    }

    if (current_image->diffs_ratio < cfg->threshold_ratio) {
        current_image->diffs = 0;
    }

    alg->tune_smartmask();

    alg->ref_frame_update();

    previous_diffs = current_image->diffs;
    previous_location_x = current_image->location.x;
    previous_location_y = current_image->location.y;
}

/* apply image overlays */
void cls_camera::overlay()
{
    if ((restart == true) || (handler_stop == true)) {
        return;
    }

    char tmp[PATH_MAX];

    if ((cfg->smart_mask_speed >0) &&
        ((cfg->picture_output_motion != "off") ||
        cfg->movie_output_motion ||
        (stream.motion.jpg_cnct > 0) ||
        (stream.motion.ts_cnct > 0))) {
        draw->smartmask();
    }

    if (imgs.largest_label &&
        ((cfg->picture_output_motion != "off") ||
        cfg->movie_output_motion ||
        (stream.motion.jpg_cnct > 0) ||
        (stream.motion.ts_cnct > 0))) {
        draw->largest_label();
    }

    if (imgs.mask &&
        ((cfg->picture_output_motion != "off") ||
        cfg->movie_output_motion ||
        (stream.motion.jpg_cnct > 0) ||
        (stream.motion.ts_cnct > 0))) {
        draw->fixed_mask();
    }

    if (cfg->text_changes) {
        if (pause == false) {
            sprintf(tmp, "%d", current_image->diffs);
        } else {
            sprintf(tmp, "-");
        }
        draw->text(current_image->image_norm
                , imgs.width, imgs.height
                , imgs.width - 10, 10
                , tmp, text_scale);
    }

    if ((stream.motion.jpg_cnct > 0) ||
        (stream.motion.ts_cnct > 0)) {
        sprintf(tmp, "D:%5d L:%3d N:%3d", current_image->diffs,
            current_image->total_labels, noise);
        draw->text(imgs.image_motion.image_norm
                , imgs.width, imgs.height
                , imgs.width - 10
                , imgs.height - (30 * text_scale)
                , tmp, text_scale);
        sprintf(tmp, "THREAD %d SETUP", threadnr);
        draw->text(imgs.image_motion.image_norm
                , imgs.width, imgs.height
                , imgs.width - 10
                , imgs.height - (10 * text_scale)
                , tmp, text_scale);
    }

    /* Add text in lower left corner of the pictures */
    if (cfg->text_left != "") {
        mystrftime(this, tmp, sizeof(tmp), cfg->text_left.c_str(), NULL);
        draw->text(current_image->image_norm
                , imgs.width, imgs.height
                , 10, imgs.height - (10 * text_scale)
                , tmp, text_scale);
    }

    /* Add text in lower right corner of the pictures */
    if (cfg->text_right != "") {
        mystrftime(this, tmp, sizeof(tmp), cfg->text_right.c_str(), NULL);
        draw->text(current_image->image_norm
                , imgs.width, imgs.height
                , imgs.width - 10, imgs.height - (10 * text_scale)
                , tmp, text_scale);
    }
}

/* emulate motion */
void cls_camera::actions_emulate()
{
    int indx;

    if ((detecting_motion == false) &&
        (movie_norm->is_running == true)) {
        movie_norm->reset_start_time(&current_image->imgts);
    }

    if ((detecting_motion == false) &&
        (movie_motion->is_running == true)) {
        movie_motion->reset_start_time(&imgs.image_motion.imgts);
    }

    detecting_motion = true;
    if (cfg->post_capture > 0) {
        postcap = cfg->post_capture;
    }

    current_image->flags |= (IMAGE_TRIGGER | IMAGE_SAVE);
    /* Mark all images in image_ring to be saved */
    for (indx = 0; indx < imgs.ring_size; indx++) {
        imgs.image_ring[indx].flags |= IMAGE_SAVE;
    }

    detected();
}

/* call the actions */
void cls_camera::actions_motion()
{
    int indx, frame_count = 0;
    int pos = imgs.ring_in;

    for (indx = 0; indx < cfg->minimum_motion_frames; indx++) {
        if (imgs.image_ring[pos].flags & IMAGE_MOTION) {
            frame_count++;
        }
        if (pos == 0) {
            pos = imgs.ring_size-1;
        } else {
            pos--;
        }
    }

    if (frame_count >= cfg->minimum_motion_frames) {

        current_image->flags |= (IMAGE_TRIGGER | IMAGE_SAVE);

        if ((detecting_motion == false) &&
            (movie_norm->is_running == true)) {
            movie_norm->reset_start_time(&current_image->imgts);
        }
        if ((detecting_motion == false) &&
            (movie_motion->is_running == true)) {
            movie_motion->reset_start_time(&imgs.image_motion.imgts);
        }
        detecting_motion = true;
        postcap = cfg->post_capture;

        for (indx = 0; indx < imgs.ring_size; indx++) {
            imgs.image_ring[indx].flags |= IMAGE_SAVE;
        }

    } else if (postcap > 0) {
        /* we have motion in this frame, but not enough frames for trigger. Check postcap */
        current_image->flags |= (IMAGE_POSTCAP | IMAGE_SAVE);
        postcap--;
    } else {
        current_image->flags |= IMAGE_PRECAP;
    }

    detected();
}

/* call the event actions*/
void cls_camera::actions_event()
{
    if ((cfg->event_gap > 0) &&
        ((frame_curr_ts.tv_sec - lasttime ) >= cfg->event_gap)) {
        event_stop = true;
    }

    if (event_stop) {
        if (event_curr_nbr == event_prev_nbr) {

            ring_process();

            if (imgs.image_preview.diffs) {
                picture->process_preview();
                imgs.image_preview.diffs = 0;
            }
            if (cfg->on_event_end != "") {
                util_exec_command(this, cfg->on_event_end.c_str(), NULL);
            }
            movie_end();
            app->dbse->exec(this, "", "event_end");

            track_center();

            if (algsec->detected) {
                MOTPLS_LOG(NTC, TYPE_EVENTS
                    , NO_ERRNO, _("Secondary detect"));
                if (cfg->on_secondary_detect != "") {
                    util_exec_command(this
                        , cfg->on_secondary_detect.c_str()
                        , NULL);
                }
            }
            algsec->detected = false;

            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("End of event %d"), event_curr_nbr);

            postcap = 0;
            event_curr_nbr++;
            text_event_string[0] = '\0';
        }
        event_stop = false;
        event_user = false;
    }

    if ((cfg->movie_max_time > 0) &&
        (event_curr_nbr == event_prev_nbr) &&
        ((frame_curr_ts.tv_sec - movie_start_time) >=
            cfg->movie_max_time) &&
        ( !(current_image->flags & IMAGE_POSTCAP)) &&
        ( !(current_image->flags & IMAGE_PRECAP))) {
        movie_end();
        movie_start();
    }

}

void cls_camera::actions()
{
    if ((restart == true) || (handler_stop == true)) {
        return;
    }

     if ((current_image->diffs > threshold) &&
        (current_image->diffs < threshold_maximum)) {
        current_image->flags |= IMAGE_MOTION;
        info_diff_cnt++;
        info_diff_tot += (uint)current_image->diffs;
        info_sdev_tot += (uint)current_image->location.stddev_xy;
        if (info_sdev_min > current_image->location.stddev_xy ) {
            info_sdev_min = current_image->location.stddev_xy;
        }
        if (info_sdev_max < current_image->location.stddev_xy ) {
            info_sdev_max = current_image->location.stddev_xy;
        }
        /*
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO
        , "dev_x %d dev_y %d dev_xy %d, diff %d ratio %d"
        , current_image->location.stddev_x
        , current_image->location.stddev_y
        , current_image->location.stddev_xy
        , current_image->diffs
        , current_image->diffs_ratio);
        */
    }

    if ((cfg->emulate_motion || event_user) && (startup_frames == 0)) {
        actions_emulate();
    } else if ((current_image->flags & IMAGE_MOTION) && (startup_frames == 0)) {
        actions_motion();
    } else if (postcap > 0) {
        current_image->flags |= (IMAGE_POSTCAP | IMAGE_SAVE);
        postcap--;
    } else {
        current_image->flags |= IMAGE_PRECAP;
        if ((cfg->event_gap == 0) && detecting_motion) {
            event_stop = true;
        }
        detecting_motion = false;
    }

    if (current_image->flags & IMAGE_SAVE) {
        lasttime = current_image->monots.tv_sec;
    }

    if (detecting_motion) {
        algsec->detect();
    }

    areadetect();

    ring_process();

    actions_event();

}

/* Snapshot interval*/
void cls_camera::snapshot()
{
    if ((restart == true) || (handler_stop == true)) {
        return;
    }

    if ((cfg->snapshot_interval > 0 && shots_mt == 0 &&
         frame_curr_ts.tv_sec % cfg->snapshot_interval <=
         frame_last_ts.tv_sec % cfg->snapshot_interval) ||
         action_snapshot) {
        picture->process_snapshot();
        action_snapshot = false;
    }
}

/* Create timelapse video*/
void cls_camera::timelapse()
{
    struct tm timestamp_tm;

    if ((restart == true) || (handler_stop == true)) {
        return;
    }

    if (cfg->timelapse_interval) {
        localtime_r(&current_image->imgts.tv_sec, &timestamp_tm);

        if (timestamp_tm.tm_min == 0 &&
            (frame_curr_ts.tv_sec % 60 < frame_last_ts.tv_sec % 60) &&
            shots_mt == 0) {

            if (cfg->timelapse_mode == "daily") {
                if (timestamp_tm.tm_hour == 0) {
                    movie_timelapse->stop();
                }
            } else if (cfg->timelapse_mode == "hourly") {
                movie_timelapse->stop();
            } else if (cfg->timelapse_mode == "weekly-sunday") {
                if (timestamp_tm.tm_wday == 0 && timestamp_tm.tm_hour == 0) {
                    movie_timelapse->stop();
                }
            } else if (cfg->timelapse_mode == "weekly-monday") {
                if (timestamp_tm.tm_wday == 1 && timestamp_tm.tm_hour == 0) {
                    movie_timelapse->stop();
                }
            } else if (cfg->timelapse_mode == "monthly") {
                if (timestamp_tm.tm_mday == 1 && timestamp_tm.tm_hour == 0) {
                    movie_timelapse->stop();
                }
            }
        }

        if (shots_mt == 0 &&
            frame_curr_ts.tv_sec % cfg->timelapse_interval <=
            frame_last_ts.tv_sec % cfg->timelapse_interval) {
            movie_timelapse->start();
            if (movie_timelapse->put_image(
                current_image, &current_image->imgts) == -1) {
                MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
            }
        }

    } else if (movie_timelapse->is_running) {
    /*
     * If timelapse movie is in progress but conf.timelapse_interval is zero then close timelapse file
     * This is an important feature that allows manual roll-over of timelapse file using the http
     * remote control via a cron job.
     */
        movie_timelapse->stop();
    }
}

/* send images to loopback device*/
void cls_camera::loopback()
{
    if ((restart == true) || (handler_stop == true)) {
        return;
    }

    vlp_putpipe(this);

    if (!cfg->stream_motion || shots_mt == 0) {
        webu_getimg_main(this);
    }

}

void cls_camera::check_schedule()
{
    struct tm c_tm;
    int indx, cur_dy;
    bool prev;

    if ((restart == true) || (handler_stop == true)) {
        return;
    }

    if (user_pause) {
        pause = true;
        return;
    }

    if (schedule.size() == 0) {
        return;
    }

    localtime_r(&current_image->imgts.tv_sec, &c_tm);
    cur_dy = c_tm.tm_wday;
    prev = pause;

    for (indx=0; indx<schedule[cur_dy].size(); indx++) {
        if ((schedule[cur_dy][indx].action == "pause") &&
            (c_tm.tm_hour >= schedule[cur_dy][indx].st_hr) &&
            (c_tm.tm_min  >= schedule[cur_dy][indx].st_min) &&
            (c_tm.tm_hour <= schedule[cur_dy][indx].en_hr) &&
            (c_tm.tm_min  <= schedule[cur_dy][indx].en_min) ) {
            if (schedule[cur_dy][indx].detect) {
                pause = false;
            } else {
                pause = true;
            }
        }
    }
    if (prev != pause) {
        MOTPLS_LOG(NTC, TYPE_EVENTS, NO_ERRNO
            , _("Scheduled action. Motion detection %s")
            , pause ? _("disabled"):_("enabled"));
    }

    /*
    MOTPLS_LOG(NTC, TYPE_EVENTS, NO_ERRNO
    , "Motion %02d:%02d %02d:%02d detection %d %d"
    ,schedule[cur_dy][indx].st_hr
    ,schedule[cur_dy][indx].st_min
    ,schedule[cur_dy][indx].en_hr
    ,schedule[cur_dy][indx].en_min
    ,schedule[cur_dy][indx].detect
    ,pause);
    */
}

/* sleep the loop to get framerate requested */
void cls_camera::frametiming()
{
    struct timespec ts2;
    int64_t sleeptm;

    if ((restart == true) || (handler_stop == true)) {
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts2);

    sleeptm = ((1000000L / cfg->framerate) -
          (1000000L * (ts2.tv_sec - frame_curr_ts.tv_sec)) -
          ((ts2.tv_nsec - frame_curr_ts.tv_nsec)/1000))*1000;

    /* If over 1 second, just do one*/
    if (sleeptm > 999999999L) {
        SLEEP(1, 0);
    } else if (sleeptm > 0) {
        SLEEP(0, sleeptm);
    }

    passflag = true;
}

void cls_camera::handler()
{
    mythreadname_set("cl", cfg->device_id, cfg->device_name.c_str());
    device_status = STATUS_INIT;

    while (handler_stop == false) {
        init();
        prepare();
        resetimages();
        capture();
        detection();
        tuning();
        overlay();
        actions();
        snapshot();
        timelapse();
        loopback();
        check_schedule();
        frametiming();
    }

    cleanup();

    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Camera closed"));

    handler_running = false;
    pthread_exit(NULL);
}

void cls_camera::handler_startup()
{
    int retcd;
    pthread_attr_t thread_attr;

    if (handler_running == false) {
        handler_running = true;
        handler_stop = false;
        restart = false;
        pthread_attr_init(&thread_attr);
        pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
        retcd = pthread_create(&handler_thread, &thread_attr, &camera_handler, this);
        if (retcd != 0) {
            MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Unable to start camera thread."));
            handler_running = false;
            handler_stop = true;
        }
        pthread_attr_destroy(&thread_attr);
    }
}

void cls_camera::handler_shutdown()
{
    int waitcnt;

    if (handler_running == true) {
        handler_stop = true;
        waitcnt = 0;
        while ((handler_running == true) && (waitcnt < cfg->watchdog_tmo)){
            SLEEP(1,0)
            waitcnt++;
        }
        if (waitcnt == cfg->watchdog_tmo) {
            MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                , _("Normal shutdown of camera failed"));
            if (cfg->watchdog_kill > 0) {
                MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Waiting additional %d seconds (watchdog_kill).")
                    ,cfg->watchdog_kill);
                waitcnt = 0;
                while ((handler_running == true) && (waitcnt < cfg->watchdog_kill)){
                    SLEEP(1,0)
                    waitcnt++;
                }
                if (waitcnt == cfg->watchdog_kill) {
                    MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                        , _("No response to shutdown.  Killing it."));
                    MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                        , _("Memory leaks will occur."));
                    pthread_kill(handler_thread, SIGVTALRM);
                }
            } else {
                MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                    , _("watchdog_kill set to terminate application."));
                exit(1);
            }
        }
        handler_running = false;
        watchdog = cfg->watchdog_tmo;
    }
}

cls_camera::cls_camera(cls_motapp *p_app)
{
    app = p_app;

    cfg = nullptr;
    conf_src = nullptr;
    current_image = nullptr;
    alg = nullptr;
    algsec = nullptr;
    rotate = nullptr;
    netcam = nullptr;
    netcam_high = nullptr;
    draw = nullptr;
    picture = nullptr;

    threadnr = -1;
    noise = -1;
    detecting_motion = false;
    event_curr_nbr = -1;
    event_prev_nbr = -1;
    threshold = -1;
    lastrate = -1;
    frame_skip = -1;
    lost_connection = false;
    text_scale = 1;
    movie_passthrough = false;

    memset(&eventid, 0, sizeof(eventid));
    memset(&text_event_string, 0, sizeof(text_event_string));
    memset(hostname, 0, sizeof(hostname));
    memset(action_user, 0, sizeof(action_user));

    movie_fps = -1;
    pipe = -1;
    mpipe = -1;
    pause = false;
    user_pause = false;
    missing_frame_counter = -1;
    schedule.clear();
    cleandir = nullptr;

    info_diff_tot = 0;
    info_diff_cnt = 0;
    info_sdev_min = 0;
    info_sdev_max = 0;
    info_sdev_tot = 0;

    action_snapshot = false;
    event_stop = false;
    event_user = false;
    camera_type = CAMERA_TYPE_UNKNOWN;
    connectionlosttime.tv_sec = 0;
    connectionlosttime.tv_nsec = 0;

    handler_running = false;
    handler_stop = true;
    restart = false;
    finish = false;
    watchdog = 90;
    passflag = false;
    pthread_mutex_init(&stream.mutex, NULL);
    device_status = STATUS_CLOSED;
    memset(&imgs, 0, sizeof(ctx_images));
    memset(&stream, 0, sizeof(ctx_stream));
    memset(&all_loc, 0, sizeof(ctx_all_loc));
    memset(&all_sizes, 0, sizeof(ctx_all_sizes));
    all_sizes.reset = true;
}

cls_camera::~cls_camera()
{
    mydelete(conf_src);
    mydelete(cfg);
    pthread_mutex_destroy(&stream.mutex);
    device_status = STATUS_CLOSED;
}
