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
 *    Copyright 2020-2021 MotionMrDave@gmail.com
 *
*/


#include "motionplus.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "motion_loop.hpp"
#include "rotate.hpp"
#include "movie.hpp"
#include "mmalcam.hpp"
#include "video_v4l2.hpp"
#include "video_loopback.hpp"
#include "netcam.hpp"
#include "conf.hpp"
#include "alg.hpp"
#include "alg_sec.hpp"
#include "event.hpp"
#include "picture.hpp"
#include "rotate.hpp"
#include "webu.hpp"
#include "dbse.hpp"
#include "draw.hpp"
#include "webu_stream.hpp"


static void mlp_ring_resize(struct ctx_cam *cam, int new_size)
{

    int smallest, i;
    struct ctx_image_data *tmp;

    if (cam->event_nr != cam->prev_event) {

        if (new_size < cam->imgs.ring_size) {
            smallest = new_size;
        } else {
            smallest = cam->imgs.ring_size;
        }

        if (cam->imgs.ring_in == smallest - 1 || smallest == 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Resizing pre_capture buffer to %d items"), new_size);

            tmp =(struct ctx_image_data*) mymalloc(new_size * sizeof(struct ctx_image_data));

            if (smallest > 0) {
                memcpy(tmp, cam->imgs.image_ring, sizeof(struct ctx_image_data) * smallest);
            }

            for(i = smallest; i < new_size; i++) {
                tmp[i].image_norm =(unsigned char*) mymalloc(cam->imgs.size_norm);
                memset(tmp[i].image_norm, 0x80, cam->imgs.size_norm);  /* initialize to grey */
                if (cam->imgs.size_high > 0) {
                    tmp[i].image_high =(unsigned char*) mymalloc(cam->imgs.size_high);
                    memset(tmp[i].image_high, 0x80, cam->imgs.size_high);
                }
            }

            if (cam->imgs.image_ring != NULL) {
                free(cam->imgs.image_ring);
            }
            cam->imgs.image_ring = NULL;

            cam->imgs.image_ring = tmp;
            cam->current_image = NULL;

            cam->imgs.ring_size = new_size;

            cam->imgs.ring_in = 0;
            cam->imgs.ring_out = 0;
        }
    }
}

static void mlp_ring_destroy(struct ctx_cam *cam)
{
    int i;

    if (cam->imgs.image_ring == NULL) {
        return;
    }

    for (i = 0; i < cam->imgs.ring_size; i++) {
        if (cam->imgs.image_ring[i].image_norm != NULL) {
            free(cam->imgs.image_ring[i].image_norm);
        }
        cam->imgs.image_ring[i].image_norm = NULL;

        if (cam->imgs.image_ring[i].image_high != NULL) {
            free(cam->imgs.image_ring[i].image_high);
        }
        cam->imgs.image_ring[i].image_high = NULL;
    }
    if (cam->imgs.image_ring != NULL) {
        free(cam->imgs.image_ring);
    }
    cam->imgs.image_ring = NULL;

    cam->imgs.image_ring = NULL;
    cam->current_image = NULL;
    cam->imgs.ring_size = 0;
}

static void mlp_ring_process_debug(struct ctx_cam *cam)
{
    char tmp[32];
    const char *t;

    if (cam->imgs.image_ring[cam->imgs.ring_out].flags & IMAGE_TRIGGER) {
        t = "Trigger";
    } else if (cam->imgs.image_ring[cam->imgs.ring_out].flags & IMAGE_MOTION) {
        t = "Motion";
    } else if (cam->imgs.image_ring[cam->imgs.ring_out].flags & IMAGE_PRECAP) {
        t = "Precap";
    } else if (cam->imgs.image_ring[cam->imgs.ring_out].flags & IMAGE_POSTCAP) {
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

static void mlp_ring_process(struct ctx_cam *cam)
{

    struct ctx_image_data *saved_current_image = cam->current_image;

    do {
        if ((cam->imgs.image_ring[cam->imgs.ring_out].flags & (IMAGE_SAVE | IMAGE_SAVED)) != IMAGE_SAVE) {
            break;
        }

        cam->current_image = &cam->imgs.image_ring[cam->imgs.ring_out];

        if (cam->imgs.image_ring[cam->imgs.ring_out].shot < cam->conf->framerate) {
            if (cam->motapp->log_level >= DBG) {
                mlp_ring_process_debug(cam);
            }

            event(cam, EVENT_IMAGE_DETECTED,
              &cam->imgs.image_ring[cam->imgs.ring_out], NULL, NULL,
              &cam->imgs.image_ring[cam->imgs.ring_out].imgts);

            if (cam->movie_last_shot >= 0) {
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

    } while (cam->imgs.ring_out != cam->imgs.ring_in);

    cam->current_image = saved_current_image;
}

static void mlp_detected_trigger(struct ctx_cam *cam, struct ctx_image_data *img)
{

    if (img->flags & IMAGE_TRIGGER) {
        if (cam->event_nr != cam->prev_event) {

            cam->prev_event = cam->event_nr;
            cam->eventtime = img->imgts.tv_sec;

            if (cam->algsec_inuse) {
                cam->algsec->isdetected = false;
            }

            mystrftime(cam, cam->text_event_string, sizeof(cam->text_event_string),
                       cam->conf->text_event.c_str(), &img->imgts, NULL, 0);

            event(cam, EVENT_FIRSTMOTION, img, NULL, NULL,
                &cam->imgs.image_ring[cam->imgs.ring_out].imgts);

            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motion detected - starting event %d"),
                       cam->event_nr);

            if (cam->new_img & (NEWIMG_FIRST | NEWIMG_BEST | NEWIMG_CENTER)) {
                pic_save_preview(cam, img);
            }

        }

        event(cam, EVENT_MOTION, NULL, NULL, NULL, &img->imgts);
    }

}

static void mlp_track_center(struct ctx_cam *cam)
{

    if ((cam->conf->ptz_auto_track) && (cam->conf->ptz_move_track != "")) {
        cam->track_posx = 0;
        cam->track_posy = 0;
        util_exec_command(cam, cam->conf->ptz_move_track.c_str(), NULL, 0);
        cam->frame_skip = cam->conf->ptz_wait;
    }

}

static void mlp_track_move(struct ctx_cam *cam, struct ctx_coord *cent)
{

    if ((cam->conf->ptz_auto_track) && (cam->conf->ptz_move_track != "")) {
            cam->track_posx += cent->x;
            cam->track_posy += cent->y;
            util_exec_command(cam, cam->conf->ptz_move_track.c_str(), NULL, 0);
            cam->frame_skip = cam->conf->ptz_wait;
    }
}

static void mlp_detected(struct ctx_cam *cam, struct ctx_image_data *img)
{
    struct ctx_config *conf = cam->conf;
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
        if (conf->stream_motion && !cam->motapp->setup_mode && img->shot != 1) {
            event(cam, EVENT_STREAM, img, NULL, NULL, &img->imgts);
        }
        if (conf->picture_output_motion != "off") {
            event(cam, EVENT_IMAGEM_DETECTED, NULL, NULL, NULL, &img->imgts);
        }
    }

    mlp_track_move(cam, &img->location);

}

static void mlp_mask_privacy(struct ctx_cam *cam)
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

void mlp_cam_close(struct ctx_cam *cam)
{

    if (cam->mmalcam) {
        MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO,_("calling mmalcam_cleanup"));
        mmalcam_cleanup(cam->mmalcam);
        cam->mmalcam = NULL;
        cam->running_cam = false;
        return;
    }

    if (cam->netcam) {
        /* This also cleans up high resolution */
        MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO,_("calling netcam_cleanup"));
        netcam_cleanup(cam, false);
        return;
    }

    if (cam->camera_type == CAMERA_TYPE_V4L2) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Cleaning up V4L2 device"));
        v4l2_cleanup(cam);
        return;
    }

    MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("No Camera device cleanup (MMAL, Netcam, V4L2)"));
    return;

}

/**
 * mlp_cam_start
 * Returns
 *     device number
 *     -1 if failed to open device.
 *     -3 image dimensions are not modulo 8
 */
int mlp_cam_start(struct ctx_cam *cam)
{
    int dev = -1;

    if (cam->camera_type == CAMERA_TYPE_MMAL) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Opening MMAL cam"));
        dev = mmalcam_start(cam);
        if (dev < 0) {
            mmalcam_cleanup(cam->mmalcam);
            cam->mmalcam = NULL;
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("MMAL cam failed to open"));
        }
        return dev;
    }

    if (cam->camera_type == CAMERA_TYPE_NETCAM) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Opening Netcam"));
        dev = netcam_setup(cam);
        if (dev < 0) {
            netcam_cleanup(cam, true);
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("Netcam failed to open"));
        }
        return dev;
    }

    if (cam->camera_type == CAMERA_TYPE_V4L2) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Opening V4L2 device"));
        dev = v4l2_start(cam);
        if (dev < 0) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("V4L2 device failed to open"));
        }
        return dev;
    }

    MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
        ,_("No Camera device specified (MMAL, Netcam, V4L2)"));
    return dev;

}

int mlp_cam_next(struct ctx_cam *cam, struct ctx_image_data *img_data)
{

    if (cam->camera_type == CAMERA_TYPE_MMAL) {
        if (cam->mmalcam == NULL) {
            return NETCAM_GENERAL_ERROR;
        }
        return mmalcam_next(cam, img_data);
    }

    if (cam->camera_type == CAMERA_TYPE_NETCAM) {
        if (cam->video_dev == -1) {
            return NETCAM_GENERAL_ERROR;
        }
        return netcam_next(cam, img_data);
    }

    if (cam->camera_type == CAMERA_TYPE_V4L2) {
        return v4l2_next(cam, img_data);
   }

    return -2;
}

static int init_camera_type(struct ctx_cam *cam)
{

    cam->camera_type = CAMERA_TYPE_UNKNOWN;

    if (cam->conf->mmalcam_name != "") {
        cam->camera_type = CAMERA_TYPE_MMAL;
        return 0;
    }

    if (cam->conf->netcam_url != "") {
        cam->camera_type = CAMERA_TYPE_NETCAM;
        return 0;
    }

    if (cam->conf->v4l2_device != "") {
        cam->camera_type = CAMERA_TYPE_V4L2;
        return 0;
    }

    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
        , _("Unable to determine camera type (MMAL, Netcam, V4L2)"));
    return -1;

}

/** Get first images from camera at startup */
static void mlp_init_firstimage(struct ctx_cam *cam)
{

    int indx;

    cam->current_image = &cam->imgs.image_ring[cam->imgs.ring_in];
    if (cam->video_dev >= 0) {
        for (indx = 0; indx < 5; indx++) {
            if (mlp_cam_next(cam, cam->current_image) == 0) {
                break;
            }
            SLEEP(2, 0);
        }

        if (indx >= 5) {
            memset(cam->current_image->image_norm, 0x80, cam->imgs.size_norm);
            /* initialize to grey */
            draw_text(cam->current_image->image_norm , cam->imgs.width, cam->imgs.height,
                      10, 20, "Error capturing first image", cam->text_scale);
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error capturing first image"));
        }
    }
}

/** Check the image size to determine if modulo 8 and over 64 */
static int mlp_check_szimg(struct ctx_cam *cam)
{

    /* Revalidate we got a valid image size */
    if ((cam->imgs.width % 8) || (cam->imgs.height % 8)) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image width (%d) or height(%d) requested is not modulo 8.")
            ,cam->imgs.width, cam->imgs.height);
        return -1;
    }
    if ((cam->imgs.width  < 64) || (cam->imgs.height < 64)) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Motion only supports width and height greater than or equal to 64 %dx%d")
            ,cam->imgs.width, cam->imgs.height);
            return -1;
    }
    /* Substream size notification*/
    if ((cam->imgs.width % 16) || (cam->imgs.height % 16)) {
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("Substream not available.  Image sizes not modulo 16."));
    }

    return 0;

}

/** Set the items required for the area detect */
static void mlp_init_areadetect(struct ctx_cam *cam)
{

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
static void mlp_init_buffers(struct ctx_cam *cam)
{

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
    cam->imgs.image_secondary =(unsigned char*) mymalloc(3 * cam->imgs.width * cam->imgs.height);
    if (cam->imgs.size_high > 0) {
        cam->imgs.image_preview.image_high =(unsigned char*) mymalloc(cam->imgs.size_high);
    } else {
        cam->imgs.image_preview.image_high = NULL;
    }

    memset(cam->imgs.smartmask, 0, cam->imgs.motionsize);
    memset(cam->imgs.smartmask_final, 255, cam->imgs.motionsize);
    memset(cam->imgs.smartmask_buffer, 0, cam->imgs.motionsize * sizeof(*cam->imgs.smartmask_buffer));

}

static void mlp_init_values(struct ctx_cam *cam)
{

    cam->event_nr = 1;
    cam->prev_event = 0;

    clock_gettime(CLOCK_REALTIME, &cam->frame_curr_ts);
    clock_gettime(CLOCK_REALTIME, &cam->frame_last_ts);

    cam->noise = cam->conf->noise_level;

    cam->threshold = cam->conf->threshold;
    if (cam->conf->threshold_maximum > cam->conf->threshold ) {
        cam->threshold_maximum = cam->conf->threshold_maximum;
    } else {
        cam->threshold_maximum = (cam->imgs.height * cam->imgs.width * 3) / 2;
    }

    cam->startup_frames = (cam->conf->framerate * 2) + cam->conf->pre_capture + cam->conf->minimum_motion_frames;

    cam->minimum_frame_time_downcounter = cam->conf->minimum_frame_time;
    cam->get_image = 1;

    cam->movie_passthrough = cam->conf->movie_passthrough;
    if ((cam->camera_type != CAMERA_TYPE_NETCAM) &&
        (cam->movie_passthrough)) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Pass-through processing disabled."));
        cam->movie_passthrough = false;
    }

}

static int mlp_init_cam_start(struct ctx_cam *cam)
{

    cam->video_dev = mlp_cam_start(cam);

    if (cam->video_dev == -1) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Could not fetch initial image from camera "));
        return -1;
    } else if (cam->video_dev == -2) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Could not fetch initial image from camera "));
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Motion only supports width and height modulo 8"));
        return -1;
    } else {
        cam->imgs.motionsize = (cam->imgs.width * cam->imgs.height);
        cam->imgs.size_norm  = (cam->imgs.width * cam->imgs.height * 3) / 2;
        cam->imgs.size_high  = (cam->imgs.width_high * cam->imgs.height_high * 3) / 2;
    }

    return 0;

}

static void mlp_init_ref(struct ctx_cam *cam)
{

    memcpy(cam->imgs.image_virgin, cam->current_image->image_norm, cam->imgs.size_norm);

    mlp_mask_privacy(cam);

    memcpy(cam->imgs.image_vprvcy, cam->current_image->image_norm, cam->imgs.size_norm);

    alg_update_reference_frame(cam, RESET_REF_FRAME);

}

/** mlp_init */
static int mlp_init(struct ctx_cam *cam)
{
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("initialize loop"));

    mythreadname_set("ml",cam->threadnr,cam->conf->camera_name.c_str());

    pthread_setspecific(tls_key_threadnr, (void *)((unsigned long)cam->threadnr));

    if (init_camera_type(cam) != 0 ) {
        return -1;
    }

    mlp_init_values(cam);

    if (mlp_init_cam_start(cam) != 0) {
        return -1;
    }

    if (mlp_check_szimg(cam) != 0) {
        return -1;
    }

    mlp_ring_resize(cam, 1); /* Create a initial precapture ring buffer with 1 frame */

    mlp_init_buffers(cam);

    webu_stream_init(cam);

    algsec_init(cam);

    rotate_init(cam);

    draw_init_scale(cam);

    mlp_init_firstimage(cam);

    vlp_init(cam);

    dbse_init(cam);

    pic_init_mask(cam);

    pic_init_privacy(cam);

    mlp_init_areadetect(cam);

    mlp_init_ref(cam);

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
        ,_("Camera %d started: motion detection %s"),
        cam->camera_id, cam->pause ? _("Disabled"):_("Enabled"));

    if (cam->conf->emulate_motion) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("Emulating motion"));
    }

    return 0;
}

/** clean up all memory etc. from motion init */
void mlp_cleanup(struct ctx_cam *cam)
{

    event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, NULL);

    /*if (cam->event_nr == cam->prev_event) {
        mlp_ring_process(cam);
        if (cam->imgs.image_preview.diffs) {
            event(cam, EVENT_IMAGE_PREVIEW, NULL, NULL, NULL, &cam->current_image->imgts);
            cam->imgs.image_preview.diffs = 0;
        }
    */
        event(cam, EVENT_ENDMOTION, NULL, NULL, NULL, &cam->current_image->imgts);
    /* } */

    webu_stream_deinit(cam);

    algsec_deinit(cam);

    if (cam->video_dev >= 0) {
        mlp_cam_close(cam);
    }

    if (cam->imgs.image_motion.image_norm != NULL) {
        free(cam->imgs.image_motion.image_norm);
    }
    cam->imgs.image_motion.image_norm = NULL;

    if (cam->imgs.ref != NULL) {
        free(cam->imgs.ref);
    }
    cam->imgs.ref = NULL;

    if (cam->imgs.ref_dyn != NULL) {
        free(cam->imgs.ref_dyn);
    }
    cam->imgs.ref_dyn = NULL;

    if (cam->imgs.image_virgin != NULL) {
        free(cam->imgs.image_virgin);
    }
    cam->imgs.image_virgin = NULL;

    if (cam->imgs.image_vprvcy != NULL) {
        free(cam->imgs.image_vprvcy);
    }
    cam->imgs.image_vprvcy = NULL;

    if (cam->imgs.labels != NULL) {
        free(cam->imgs.labels);
    }
    cam->imgs.labels = NULL;

    if (cam->imgs.labelsize != NULL) {
        free(cam->imgs.labelsize);
    }
    cam->imgs.labelsize = NULL;

    if (cam->imgs.smartmask != NULL) {
        free(cam->imgs.smartmask);
    }
    cam->imgs.smartmask = NULL;

    if (cam->imgs.smartmask_final != NULL) {
        free(cam->imgs.smartmask_final);
    }
    cam->imgs.smartmask_final = NULL;

    if (cam->imgs.smartmask_buffer != NULL) {
        free(cam->imgs.smartmask_buffer);
    }
    cam->imgs.smartmask_buffer = NULL;

    if (cam->imgs.mask != NULL) {
        free(cam->imgs.mask);
    }
    cam->imgs.mask = NULL;

    if (cam->imgs.mask_privacy != NULL) {
        free(cam->imgs.mask_privacy);
    }
    cam->imgs.mask_privacy = NULL;

    if (cam->imgs.mask_privacy_uv != NULL) {
        free(cam->imgs.mask_privacy_uv);
    }
    cam->imgs.mask_privacy_uv = NULL;

    if (cam->imgs.mask_privacy_high != NULL) {
        free(cam->imgs.mask_privacy_high);
    }
    cam->imgs.mask_privacy_high = NULL;

    if (cam->imgs.mask_privacy_high_uv != NULL) {
        free(cam->imgs.mask_privacy_high_uv);
    }
    cam->imgs.mask_privacy_high_uv = NULL;

    if (cam->imgs.common_buffer != NULL) {
        free(cam->imgs.common_buffer);
    }
    cam->imgs.common_buffer = NULL;

    if (cam->imgs.image_secondary != NULL) {
        free(cam->imgs.image_secondary);
    }
    cam->imgs.image_secondary = NULL;

    if (cam->imgs.image_preview.image_norm != NULL)  {
        free(cam->imgs.image_preview.image_norm);
    }
    cam->imgs.image_preview.image_norm = NULL;

    if (cam->imgs.image_preview.image_high != NULL) {
        free(cam->imgs.image_preview.image_high);
    }
    cam->imgs.image_preview.image_high = NULL;

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

static void mlp_areadetect(struct ctx_cam *cam)
{
    int i, j, z = 0;

    if ((cam->conf->area_detect != "" ) &&
        (cam->event_nr != cam->areadetect_eventnbr) &&
        (cam->current_image->flags & IMAGE_TRIGGER)) {
        j = cam->conf->area_detect.length();
        for (i = 0; i < j; i++) {
            z = cam->conf->area_detect[i] - 49; /* characters are stored as ascii 48-57 (0-9) */
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

static void mlp_prepare(struct ctx_cam *cam)
{

    int frame_buffer_size;

    cam->watchdog = cam->conf->watchdog_tmo;

    cam->frame_last_ts.tv_sec = cam->frame_curr_ts.tv_sec;
    cam->frame_last_ts.tv_nsec = cam->frame_curr_ts.tv_nsec;
    clock_gettime(CLOCK_REALTIME, &cam->frame_curr_ts);

    if (cam->conf->pre_capture < 0) {
        cam->conf->pre_capture = 0;
    }

    frame_buffer_size = cam->conf->pre_capture + cam->conf->minimum_motion_frames;

    if (cam->imgs.ring_size != frame_buffer_size) {
        mlp_ring_resize(cam, frame_buffer_size);
    }

    if (cam->frame_last_ts.tv_sec != cam->frame_curr_ts.tv_sec) {
        cam->lastrate = cam->shots + 1;
        cam->shots = -1;

        if (cam->conf->minimum_frame_time) {
            cam->minimum_frame_time_downcounter--;
            if (cam->minimum_frame_time_downcounter == 0) {
                cam->get_image = 1;
            }
        } else {
            cam->get_image = 1;
        }
    }

    cam->shots++;

    if (cam->startup_frames > 0) {
        cam->startup_frames--;
    }


}

static void mlp_resetimages(struct ctx_cam *cam)
{

    if (cam->conf->minimum_frame_time) {
        cam->minimum_frame_time_downcounter = cam->conf->minimum_frame_time;
        cam->get_image = 0;
    }

    /* ring_buffer_in is pointing to current pos, update before put in a new image */
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

    /* Store shot number with pre_captured image */
    cam->current_image->shot = cam->shots;

}

static int mlp_retry(struct ctx_cam *cam)
{

    /*
     * If a camera is not available we keep on retrying every 10 seconds
     * until it shows up.
     */
    int size_high;

    if (cam->video_dev < 0 &&
        cam->frame_curr_ts.tv_sec % 10 == 0 && cam->shots == 0) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Retrying until successful connection with camera"));
        cam->video_dev = mlp_cam_start(cam);

        if (cam->video_dev < 0) {
            return 1;
        }

        if (mlp_check_szimg(cam) != 0) {
            return 1;
        }

        /*
         * If the netcam has different dimensions than in the config file
         * we need to restart Motion to re-allocate all the buffers
         */
        if (cam->imgs.width != cam->conf->width || cam->imgs.height != cam->conf->height) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Camera has finally become available\n"
                       "Camera image has different width and height"
                       "from what is in the config file. You should fix that\n"
                       "Restarting Motion thread to reinitialize all "
                       "image buffers to new picture dimensions"));
            cam->conf->width = cam->imgs.width;
            cam->conf->height = cam->imgs.height;
            /*
             * Break out of main loop terminating thread
             * watchdog will start us again
             */
            return 1;
        }
        /*
         * For high res, we check the size of buffer to determine whether to break out
         * the init_motion function allocated the buffer for high using the cam->imgs.size_high
         * and the mlp_cam_start ONLY re-populates the height/width so we can check the size here.
         */
        size_high = (cam->imgs.width_high * cam->imgs.height_high * 3) / 2;
        if (cam->imgs.size_high != size_high) {
            return 1;
        }
    }
    return 0;
}

static int mlp_capture(struct ctx_cam *cam)
{

    const char *tmpin;
    char tmpout[80];
    int vid_return_code = 0;        /* Return code used when calling mlp_cam_next */
    struct timespec ts1;

    if (cam->video_dev >= 0) {
        vid_return_code = mlp_cam_next(cam, cam->current_image);
    } else {
        vid_return_code = 1; /* Non fatal error */
    }

    if (vid_return_code == 0) {
        cam->lost_connection = 0;
        cam->connectionlosttime = 0;

        if (cam->missing_frame_counter >= (cam->conf->camera_tmo * cam->conf->framerate)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Video signal re-acquired"));
            event(cam, EVENT_CAMERA_FOUND, NULL, NULL, NULL, NULL);
        }
        cam->missing_frame_counter = 0;
        memcpy(cam->imgs.image_virgin, cam->current_image->image_norm, cam->imgs.size_norm);
        mlp_mask_privacy(cam);
        memcpy(cam->imgs.image_vprvcy, cam->current_image->image_norm, cam->imgs.size_norm);

    } else if (vid_return_code < 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Video device fatal error - Closing video device"));
        mlp_cam_close(cam);
        memcpy(cam->current_image->image_norm, cam->imgs.image_virgin, cam->imgs.size_norm);
        cam->lost_connection = 1;
    } else {
        if (vid_return_code == NETCAM_RESTART_ERROR) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Restarting Motion thread to reinitialize all "
                "image buffers"));
            cam->lost_connection = 1;
            return 1;
        }

        if (cam->connectionlosttime == 0) {
            cam->connectionlosttime = cam->frame_curr_ts.tv_sec;
        }

        ++cam->missing_frame_counter;

        if (cam->video_dev >= 0 &&
            cam->missing_frame_counter < (cam->conf->camera_tmo * cam->conf->framerate)) {
            memcpy(cam->current_image->image_norm, cam->imgs.image_vprvcy, cam->imgs.size_norm);
        } else {
            cam->lost_connection = 1;

            if (cam->video_dev >= 0) {
                tmpin = "CONNECTION TO CAMERA LOST\\nSINCE %Y-%m-%d %T";
            } else {
                tmpin = "UNABLE TO OPEN VIDEO DEVICE\\nSINCE %Y-%m-%d %T";
            }

            ts1.tv_sec=cam->connectionlosttime;
            ts1.tv_nsec = 0;
            memset(cam->current_image->image_norm, 0x80, cam->imgs.size_norm);
            mystrftime(cam, tmpout, sizeof(tmpout), tmpin, &ts1, NULL, 0);
            draw_text(cam->current_image->image_norm, cam->imgs.width, cam->imgs.height,
                      10, 20 * cam->text_scale, tmpout, cam->text_scale);

            /* Write error message only once */
            if (cam->missing_frame_counter == (cam->conf->camera_tmo * cam->conf->framerate)) {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                    ,_("Video signal lost - Adding grey image"));
                event(cam, EVENT_CAMERA_LOST, NULL, NULL, NULL, &ts1);
            }

            if ((cam->video_dev > 0) &&
                (cam->missing_frame_counter == ((cam->conf->camera_tmo * 4) * cam->conf->framerate))) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Video signal still lost - Trying to close video device"));
                mlp_cam_close(cam);
            }
        }
    }
    return 0;

}

static void mlp_detection(struct ctx_cam *cam)
{

    if (cam->frame_skip) {
        cam->frame_skip--;
        cam->current_image->diffs = 0;
        return;
    }

    if ( !cam->pause ) {
        alg_diff(cam);
    } else {
        cam->current_image->diffs = 0;
        cam->current_image->diffs_raw = 0;
        cam->current_image->diffs_ratio = 100;
    }

}

static void mlp_tuning(struct ctx_cam *cam)
{

    if ((cam->conf->noise_tune && cam->shots == 0) &&
          (!cam->detecting_motion && (cam->current_image->diffs <= cam->threshold))) {
        alg_noise_tune(cam);
    }

    if (cam->conf->threshold_tune) {
        alg_threshold_tune(cam);
    }

    if ((cam->current_image->diffs > cam->threshold) &&
        (cam->current_image->diffs < cam->threshold_maximum)) {
        alg_location(cam);
        alg_stddev(cam);

    }

    if (cam->current_image->diffs_ratio < cam->conf->threshold_ratio) {
        cam->current_image->diffs = 0;
    }

    alg_tune_smartmask(cam);


    alg_update_reference_frame(cam, UPDATE_REF_FRAME);

    cam->previous_diffs = cam->current_image->diffs;
    cam->previous_location_x = cam->current_image->location.x;
    cam->previous_location_y = cam->current_image->location.y;

}

static void mlp_overlay(struct ctx_cam *cam)
{

    char tmp[PATH_MAX];

    if (cam->smartmask_speed &&
        ((cam->conf->picture_output_motion != "off") ||
        cam->conf->movie_output_motion ||
        cam->motapp->setup_mode ||
        (cam->stream.motion.cnct_count > 0))) {
        draw_smartmask(cam, cam->imgs.image_motion.image_norm);
    }

    if (cam->imgs.largest_label &&
        ((cam->conf->picture_output_motion != "off") ||
        cam->conf->movie_output_motion ||
        cam->motapp->setup_mode ||
        (cam->stream.motion.cnct_count > 0))) {
        draw_largest_label(cam, cam->imgs.image_motion.image_norm);
    }

    if (cam->imgs.mask &&
        ((cam->conf->picture_output_motion != "off") ||
        cam->conf->movie_output_motion ||
        cam->motapp->setup_mode ||
        (cam->stream.motion.cnct_count > 0))) {
        draw_fixed_mask(cam, cam->imgs.image_motion.image_norm);
    }

    if (cam->conf->text_changes) {
        if (!cam->pause) {
            sprintf(tmp, "%d", cam->current_image->diffs);
        } else {
            sprintf(tmp, "-");
        }
        draw_text(cam->current_image->image_norm, cam->imgs.width, cam->imgs.height,
                  cam->imgs.width - 10, 10, tmp, cam->text_scale);
    }

    if (cam->motapp->setup_mode || (cam->stream.motion.cnct_count > 0)) {
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
    if (cam->conf->text_left != "") {
        mystrftime(cam, tmp, sizeof(tmp), cam->conf->text_left.c_str(),
                   &cam->current_image->imgts, NULL, 0);
        draw_text(cam->current_image->image_norm, cam->imgs.width, cam->imgs.height,
                  10, cam->imgs.height - (10 * cam->text_scale), tmp, cam->text_scale);
    }

    /* Add text in lower right corner of the pictures */
    if (cam->conf->text_right != "") {
        mystrftime(cam, tmp, sizeof(tmp), cam->conf->text_right.c_str(),
                   &cam->current_image->imgts, NULL, 0);
        draw_text(cam->current_image->image_norm, cam->imgs.width, cam->imgs.height,
                  cam->imgs.width - 10, cam->imgs.height - (10 * cam->text_scale),
                  tmp, cam->text_scale);
    }

}

static void mlp_actions_emulate(struct ctx_cam *cam)
{

    int indx;

    if ( (cam->detecting_motion == false) && (cam->movie_norm != NULL) ) {
        movie_reset_start_time(cam->movie_norm, &cam->current_image->imgts);
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

    mlp_detected(cam, cam->current_image);

}

static void mlp_actions_motion(struct ctx_cam *cam)
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

        if ( (cam->detecting_motion == false) && (cam->movie_norm != NULL) ) {
            movie_reset_start_time(cam->movie_norm, &cam->current_image->imgts);
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

    mlp_detected(cam, cam->current_image);
}

static void mlp_actions_event(struct ctx_cam *cam)
{

    if ((cam->conf->movie_max_time > 0) &&
        (cam->event_nr == cam->prev_event) &&
        ((cam->frame_curr_ts.tv_sec - cam->eventtime) >= cam->conf->movie_max_time)) {
        cam->event_stop = true;
    }
    if ((cam->conf->event_gap > 0) &&
        ((cam->frame_curr_ts.tv_sec - cam->lasttime) >= cam->conf->event_gap)) {
        cam->event_stop = true;
    }

    if (cam->event_stop) {
        if (cam->event_nr == cam->prev_event) {

            mlp_ring_process(cam);

            if (cam->imgs.image_preview.diffs) {
                event(cam, EVENT_IMAGE_PREVIEW, NULL, NULL, NULL, &cam->current_image->imgts);
                cam->imgs.image_preview.diffs = 0;
            }
            event(cam, EVENT_ENDMOTION, NULL, NULL, NULL, &cam->current_image->imgts);

            mlp_track_center(cam);

            if (cam->algsec_inuse) {
                if (cam->algsec->isdetected) {
                    event(cam, EVENT_SECDETECT, NULL, NULL, NULL, &cam->current_image->imgts);
                }
                cam->algsec->isdetected = false;
            }

            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("End of event %d"), cam->event_nr);

            cam->postcap = 0;
            cam->event_nr++;
            cam->text_event_string[0] = '\0';
        }
        cam->event_stop = false;
        cam->event_user = false;
    }
}

static void mlp_actions(struct ctx_cam *cam)
{

     if ((cam->current_image->diffs > cam->threshold) &&
        (cam->current_image->diffs < cam->threshold_maximum)) {
        cam->current_image->flags |= IMAGE_MOTION;
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
        cam->lasttime = cam->current_image->imgts.tv_sec;
    }

    if (cam->detecting_motion) {
        algsec_detect(cam);
    }

    mlp_areadetect(cam);

    mlp_ring_process(cam);

    mlp_actions_event(cam);

}

static void mlp_setupmode(struct ctx_cam *cam)
{

    if (cam->motapp->setup_mode) {
        char msg[1024] = "\0";
        char part[100];

        if (cam->conf->despeckle_filter != "") {
            snprintf(part, 99, _("Raw changes: %5d - changes after '%s': %5d"),
                     cam->olddiffs, cam->conf->despeckle_filter.c_str(), cam->current_image->diffs);
            strcat(msg, part);
            if (cam->conf->despeckle_filter.find('l') != std::string::npos) {
                snprintf(part, 99,_(" - labels: %3d"), cam->current_image->total_labels);
                strcat(msg, part);
            }
        } else {
            snprintf(part, 99,_("Changes: %5d"), cam->current_image->diffs);
            strcat(msg, part);
        }

        if (cam->conf->noise_tune) {
            snprintf(part, 99,_(" - noise level: %2d"), cam->noise);
            strcat(msg, part);
        }

        if (cam->conf->threshold_tune) {
            snprintf(part, 99, _(" - threshold: %d"), cam->threshold);
            strcat(msg, part);
        }

        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "%s", msg);
    }

}

static void mlp_snapshot(struct ctx_cam *cam)
{

    if ((cam->conf->snapshot_interval > 0 && cam->shots == 0 &&
         cam->frame_curr_ts.tv_sec % cam->conf->snapshot_interval <=
         cam->frame_last_ts.tv_sec % cam->conf->snapshot_interval) ||
         cam->snapshot) {
        event(cam, EVENT_IMAGE_SNAPSHOT, cam->current_image, NULL, NULL, &cam->current_image->imgts);
        cam->snapshot = 0;
    }

}

static void mlp_timelapse(struct ctx_cam *cam)
{

    struct tm timestamp_tm;

    if (cam->conf->timelapse_interval) {
        localtime_r(&cam->current_image->imgts.tv_sec, &timestamp_tm);

        if (timestamp_tm.tm_min == 0 &&
            (cam->frame_curr_ts.tv_sec % 60 < cam->frame_last_ts.tv_sec % 60) &&
            cam->shots == 0) {

            if (cam->conf->timelapse_mode == "daily") {
                if (timestamp_tm.tm_hour == 0) {
                    event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cam->current_image->imgts);
                }
            } else if (cam->conf->timelapse_mode == "hourly") {
                event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cam->current_image->imgts);
            } else if (cam->conf->timelapse_mode == "weekly-sunday") {
                if (timestamp_tm.tm_wday == 0 && timestamp_tm.tm_hour == 0) {
                    event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cam->current_image->imgts);
                }
            } else if (cam->conf->timelapse_mode == "weekly-monday") {
                if (timestamp_tm.tm_wday == 1 && timestamp_tm.tm_hour == 0) {
                    event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cam->current_image->imgts);
                }
            } else if (cam->conf->timelapse_mode == "monthly") {
                if (timestamp_tm.tm_mday == 1 && timestamp_tm.tm_hour == 0) {
                    event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cam->current_image->imgts);
                }
            }
        }

        if (cam->shots == 0 &&
            cam->frame_curr_ts.tv_sec % cam->conf->timelapse_interval <=
            cam->frame_last_ts.tv_sec % cam->conf->timelapse_interval) {
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

}

static void mlp_loopback(struct ctx_cam *cam)
{

    if (cam->motapp->setup_mode) {
        event(cam, EVENT_IMAGE, &cam->imgs.image_motion, NULL, &cam->pipe, &cam->current_image->imgts);
        event(cam, EVENT_STREAM, &cam->imgs.image_motion, NULL, NULL, &cam->current_image->imgts);
    } else {
        event(cam, EVENT_IMAGE, cam->current_image, NULL, &cam->pipe, &cam->current_image->imgts);

        if (!cam->conf->stream_motion || cam->shots == 0) {
            event(cam, EVENT_STREAM, cam->current_image, NULL, NULL, &cam->current_image->imgts);
        }
    }

    event(cam, EVENT_IMAGEM, &cam->imgs.image_motion, NULL, &cam->mpipe, &cam->current_image->imgts);

}

static void mlp_parmsupdate(struct ctx_cam *cam)
{

    /* Check for some config parameter changes but only every second */
    if (cam->shots != 0) {
        return;
    }

    if (cam->parms_changed ) {
        draw_init_scale(cam);  /* Initialize and validate text_scale */

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

        if (cam->conf->locate_motion_mode == "on") {
            cam->locate_motion_mode = LOCATE_ON;
        } else if (cam->conf->locate_motion_mode == "preview") {
            cam->locate_motion_mode = LOCATE_PREVIEW;
        } else {
            cam->locate_motion_mode = LOCATE_OFF;
        }

        if (cam->conf->locate_motion_style == "box") {
            cam->locate_motion_style = LOCATE_BOX;
        } else if (cam->conf->locate_motion_style == "redbox") {
            cam->locate_motion_style = LOCATE_REDBOX;
        } else if (cam->conf->locate_motion_style == "cross") {
            cam->locate_motion_style = LOCATE_CROSS;
        } else if (cam->conf->locate_motion_style == "redcross") {
            cam->locate_motion_style = LOCATE_REDCROSS;
        } else {
            cam->locate_motion_style = LOCATE_BOX;
        }

        if (cam->conf->smart_mask_speed != cam->smartmask_speed ||
            cam->smartmask_lastrate != cam->lastrate) {
            if (cam->conf->smart_mask_speed == 0) {
                memset(cam->imgs.smartmask, 0, cam->imgs.motionsize);
                memset(cam->imgs.smartmask_final, 255, cam->imgs.motionsize);
            }
            cam->smartmask_lastrate = cam->lastrate;
            cam->smartmask_speed = cam->conf->smart_mask_speed;
            cam->smartmask_ratio = 5 * cam->lastrate * (11 - cam->smartmask_speed);
        }

        dbse_sqlmask_update(cam);

        cam->threshold = cam->conf->threshold;
        if (cam->conf->threshold_maximum > cam->conf->threshold ) {
            cam->threshold_maximum = cam->conf->threshold_maximum;
        } else {
            cam->threshold_maximum = (cam->imgs.height * cam->imgs.width * 3) / 2;
        }

        if (!cam->conf->noise_tune) {
            cam->noise = cam->conf->noise_level;
        }

        cam->parms_changed = false;
    }

    if (cam->motapp->parms_changed) {
        log_set_level(cam->motapp->log_level);
        log_set_type(cam->motapp->log_type_str.c_str());
        cam->motapp->parms_changed = false;
    }

}

static void mlp_frametiming(struct ctx_cam *cam)
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

    clock_gettime(CLOCK_REALTIME, &ts2);

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
    cam->passflag = 1;
}

/** Thread function for each camera */
void *motion_loop(void *arg)
{
    struct ctx_cam *cam =(struct ctx_cam *) arg;

    cam->running_cam = true;
    cam->finish_cam = false;

    pthread_mutex_lock(&cam->motapp->global_lock);
        cam->motapp->threads_running++;
    pthread_mutex_unlock(&cam->motapp->global_lock);

    cam->watchdog = cam->conf->watchdog_tmo;

    if (mlp_init(cam) == 0) {
        while (cam->finish_cam == false) {
            mlp_prepare(cam);
            if (cam->get_image) {
                mlp_resetimages(cam);
                if (mlp_retry(cam) == 1) {
                    break;
                }
                if (mlp_capture(cam) == 1) {
                    break;
                }
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

    cam->running_cam = false;
    cam->finish_cam = true;

    pthread_exit(NULL);
}

