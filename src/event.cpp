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
*/

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "picture.hpp"
#include "netcam.hpp"
#include "movie.hpp"
#include "event.hpp"
#include "dbse.hpp"
#include "video_loopback.hpp"
#include "webu_getimg.hpp"
#include "alg_sec.hpp"

const char *eventList[] = {
    "NULL",
    "EVENT_MOTION",
    "EVENT_START",
    "EVENT_END",
    "EVENT_STOP",
    "EVENT_TLAPSE_START",
    "EVENT_TLAPSE_END",
    "EVENT_STREAM",
    "EVENT_IMAGE_DETECTED",
    "EVENT_IMAGEM_DETECTED",
    "EVENT_IMAGE_SNAPSHOT",
    "EVENT_IMAGE",
    "EVENT_IMAGEM",
    "EVENT_IMAGE_PREVIEW",
    "EVENT_AREA_DETECTED",
    "EVENT_CAMERA_LOST",
    "EVENT_CAMERA_FOUND",
    "EVENT_MOVIE_PUT",
    "EVENT_LAST"
};

static void on_picture_save_command(ctx_dev *cam, char *fname)
{
    MOTPLS_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("File saved to: %s"), fname);

    if (cam->conf->on_picture_save != "") {
        util_exec_command(cam, cam->conf->on_picture_save.c_str(), fname);
    }
}

static void on_movie_start_command(ctx_dev *cam, char *fname)
{
    MOTPLS_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("File saved to: %s"), fname);
    if (cam->conf->on_movie_start != "") {
        util_exec_command(cam, cam->conf->on_movie_start.c_str(), fname);
    }
}

static void on_movie_end_command(ctx_dev *cam, char *fname)
{
    if (cam->conf->on_movie_end != "") {
        util_exec_command(cam, cam->conf->on_movie_end.c_str(), fname);
    }
}

static void on_motion_detected_command(ctx_dev *cam)
{
    if (cam->conf->on_motion_detected != "") {
        util_exec_command(cam, cam->conf->on_motion_detected.c_str(), NULL);
    }
}

static void on_area_command(ctx_dev *cam)
{
    if (cam->conf->on_area_detected != "") {
        util_exec_command(cam, cam->conf->on_area_detected.c_str(), NULL);
    }
}

static void on_event_start_command(ctx_dev *cam)
{
    if (cam->conf->on_event_start != "") {
        util_exec_command(cam, cam->conf->on_event_start.c_str(), NULL);
    }
}

static void on_event_end_command(ctx_dev *cam)
{
    if (cam->conf->on_event_end != "") {
        util_exec_command(cam, cam->conf->on_event_end.c_str(), NULL);
    }
}

static void event_stream_put(ctx_dev *cam)
{
    webu_getimg_main(cam);
}

static void event_vlp_putpipe(ctx_dev *cam)
{
    if (cam->pipe >= 0) {
        if (vlp_putpipe(cam->pipe
                , cam->current_image->image_norm
                , cam->imgs.size_norm) == -1) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                ,_("Failed to put image into video pipe"));
        }
    }
}

static void event_vlp_putpipem(ctx_dev *cam)
{
    if (cam->mpipe >= 0) {
        if (vlp_putpipe(cam->mpipe
                , cam->imgs.image_motion.image_norm
                , cam->imgs.size_norm) == -1) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                ,_("Failed to put image into video pipe"));
        }
    }
}

static void event_image_detect(ctx_dev *cam)
{
    char filename[PATH_MAX];

    if (cam->new_img & NEWIMG_ON) {
        mypicname(cam, filename,"%s/%s.%s"
            , cam->conf->picture_filename
            , cam->conf->picture_type);
        cam->filetype = FTYPE_IMAGE;
        if ((cam->imgs.size_high > 0) && (cam->movie_passthrough == false)) {
            cam->picture->save_norm(filename, cam->current_image->image_high);
        } else {
            cam->picture->save_norm(filename,cam->current_image->image_norm);
        }
        on_picture_save_command(cam, filename);
        dbse_exec(cam, filename, "pic_save");
    }
}

static void event_imagem_detect(ctx_dev *cam)
{
    char filename[PATH_MAX];

    if (cam->conf->picture_output_motion == "on") {
        mypicname(cam, filename,"%s/%sm.%s"
            , cam->conf->picture_filename
            , cam->conf->picture_type);
        cam->filetype = FTYPE_IMAGE_MOTION;
        cam->picture->save_norm(filename, cam->imgs.image_motion.image_norm);
        on_picture_save_command(cam, filename);
        dbse_exec(cam, filename, "pic_save");

    } else if (cam->conf->picture_output_motion == "roi") {
        mypicname(cam, filename,"%s/%sr.%s"
            , cam->conf->picture_filename
            , cam->conf->picture_type);
        cam->filetype = FTYPE_IMAGE_ROI;
        cam->picture->save_roi(filename, cam->current_image->image_norm);
        on_picture_save_command(cam, filename);
        dbse_exec(cam, filename, "pic_save");
    }
}

static void event_image_snapshot(ctx_dev *cam)
{
    char filename[PATH_MAX];
    char linkpath[PATH_MAX];
    int offset;

    offset = (int)cam->conf->snapshot_filename.length() - 8;
    if (offset < 0) {
        offset = 1;
    }

    if (cam->conf->snapshot_filename.compare(offset, 8, "lastsnap") != 0) {
        mypicname(cam, filename,"%s/%s.%s"
            , cam->conf->snapshot_filename
            , cam->conf->picture_type);
        cam->filetype = FTYPE_IMAGE_SNAPSHOT;
        if ((cam->imgs.size_high > 0) && (cam->movie_passthrough == false)) {
            cam->picture->save_norm(filename, cam->current_image->image_high);
        } else {
            cam->picture->save_norm(filename, cam->current_image->image_norm);
        }
        on_picture_save_command(cam, filename);
        dbse_exec(cam, filename, "pic_save");

        /* Update symbolic link */
        mypicname(cam, linkpath,"%s/%s.%s"
            , "lastsnap", cam->conf->picture_type);
        remove(linkpath);
        if (symlink(filename, linkpath)) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                ,_("Could not create symbolic link [%s]"), filename);
            return;
        }

    } else {
        mypicname(cam, filename,"%s/%s.%s"
            , cam->conf->snapshot_filename
            , cam->conf->picture_type);
        remove(filename);
        cam->filetype = FTYPE_IMAGE_SNAPSHOT;
        if ((cam->imgs.size_high > 0) && (cam->movie_passthrough == false)) {
            cam->picture->save_norm(filename, cam->current_image->image_high);
        } else {
            cam->picture->save_norm(filename, cam->current_image->image_norm);
        }
        on_picture_save_command(cam, filename);
        dbse_exec(cam, filename, "pic_save");
    }

    cam->snapshot = 0;
}

static void event_image_preview(ctx_dev *cam)
{
    char filename[PATH_MAX];
    ctx_image_data *saved_current_image;

    if (cam->imgs.image_preview.diffs) {
        saved_current_image = cam->current_image;
        saved_current_image->imgts= cam->current_image->imgts;

        cam->current_image = &cam->imgs.image_preview;
        cam->current_image->imgts = cam->imgs.image_preview.imgts;

        mypicname(cam, filename,"%s/%s.%s"
            , cam->conf->picture_filename
            , cam->conf->picture_type);

        cam->filetype = FTYPE_IMAGE;
        if ((cam->imgs.size_high > 0) && (cam->movie_passthrough == false)) {
            cam->picture->save_norm(filename, cam->imgs.image_preview.image_high);
        } else {
            cam->picture->save_norm(filename, cam->imgs.image_preview.image_norm);
        }
        on_picture_save_command(cam, filename);
        dbse_exec(cam, filename, "pic_save");

        /* Restore global context values. */
        cam->current_image = saved_current_image;
        cam->current_image->imgts = saved_current_image->imgts;
    }
}

static void event_camera_lost(ctx_dev *cam)
{
    if (cam->conf->on_camera_lost != "") {
        util_exec_command(cam, cam->conf->on_camera_lost.c_str(), NULL);
    }
}

static void event_secondary_detect(ctx_dev *cam)
{
    MOTPLS_LOG(NTC, TYPE_EVENTS, NO_ERRNO,_("Event secondary detect"));

    if (cam->conf->on_secondary_detect != "") {
        util_exec_command(cam, cam->conf->on_secondary_detect.c_str(), NULL);
    }
}

static void event_camera_found(ctx_dev *cam)
{
    if (cam->conf->on_camera_found != "") {
        util_exec_command(cam, cam->conf->on_camera_found.c_str(), NULL);
    }
}

static void event_movie_start(ctx_dev *cam)
{
    int retcd;

    cam->movie_start_time = cam->frame_curr_ts.tv_sec;

    if (cam->lastrate < 2) {
        cam->movie_fps = 2;
    } else {
        cam->movie_fps = cam->lastrate;
    }

    if (cam->conf->movie_output) {
        retcd = movie_init_norm(cam);
        if (retcd < 0) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("Error initializing movie."));
            myfree(&cam->movie_norm);
            return;
        }
        cam->filetype = FTYPE_MOVIE;
        on_movie_start_command(cam, cam->movie_norm->full_nm);
        dbse_exec(cam, cam->movie_norm->full_nm, "movie_start");
    }

    if (cam->conf->movie_output_motion) {
        retcd = movie_init_motion(cam);
        if (retcd < 0) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("Error initializing motion movie"));
            myfree(&cam->movie_motion);
            return;
        }
        cam->filetype = FTYPE_MOVIE;
        on_movie_start_command(cam, cam->movie_motion->full_nm);
        dbse_exec(cam, cam->movie_motion->full_nm, "movie_start");
    }

    if ((cam->conf->movie_extpipe_use) && (cam->conf->movie_extpipe != "" )) {
        retcd = movie_init_extpipe(cam);
        if (retcd < 0) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("Error initializing extpipe movie."));
            return;
        }
        cam->filetype = FTYPE_MOVIE;
        on_movie_start_command(cam, cam->extpipe_filename);
        dbse_exec(cam, cam->extpipe_filename, "movie_start");
    }
}

static void event_movie_put(ctx_dev *cam)
{
    if (cam->movie_norm) {
        if (movie_put_image(cam->movie_norm
                , cam->current_image
                , &cam->current_image->imgts) == -1) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
        }
    }
    if (cam->movie_motion) {
        if (movie_put_image(cam->movie_motion
                , &cam->imgs.image_motion
                , &cam->imgs.image_motion.imgts) == -1) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
        }
    }

    if (cam->extpipe_isopen) {
        if (movie_put_extpipe(cam) == -1) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
        }
    }

}

static void event_movie_end(ctx_dev *cam)
{
    if (cam->movie_norm) {
        cam->filetype = FTYPE_MOVIE;
        movie_close(cam->movie_norm);
        on_movie_end_command(cam, cam->movie_norm->full_nm);
        dbse_exec(cam, cam->movie_norm->full_nm, "movie_end");
        if ((cam->conf->movie_retain == "secondary") &&
            (cam->algsec_inuse) && (cam->algsec->isdetected == false)) {
            if (remove(cam->movie_norm->full_nm) != 0) {
                MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                    , _("Unable to remove file %s"), cam->movie_norm->full_nm);
            } else {
                dbse_movies_addrec(cam, cam->movie_norm
                    , &cam->current_image->imgts);
            }
        } else {
            dbse_movies_addrec(cam, cam->movie_norm, &cam->current_image->imgts);
        }
        movie_free(cam->movie_norm);
        myfree(&cam->movie_norm);
    }

    if (cam->movie_motion) {
        cam->filetype = FTYPE_MOVIE;
        movie_close(cam->movie_motion);
        on_movie_end_command(cam, cam->movie_motion->full_nm);
        dbse_exec(cam, cam->movie_motion->full_nm, "movie_end");

        if ((cam->conf->movie_retain == "secondary") &&
            (cam->algsec_inuse) && (cam->algsec->isdetected == false)) {
            if (remove(cam->movie_motion->full_nm) != 0) {
                MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                    , _("Unable to remove file %s")
                    , cam->movie_motion->full_nm);
            } else {
                dbse_movies_addrec(cam, cam->movie_motion, &cam->imgs.image_motion.imgts);
            }
        } else {
            dbse_movies_addrec(cam, cam->movie_motion, &cam->imgs.image_motion.imgts);
        }
        movie_free(cam->movie_motion);
        myfree(&cam->movie_motion);
    }

    if (cam->extpipe_isopen) {
        MOTPLS_LOG(NTC, TYPE_EVENTS, NO_ERRNO,_("Closing extpipe"));
        cam->extpipe_isopen = false;
        fflush(cam->extpipe_stream);
        pclose(cam->extpipe_stream);
        cam->extpipe_stream = NULL;

        cam->filetype = FTYPE_MOVIE;
        on_movie_end_command(cam, cam->extpipe_filename);
        dbse_exec(cam, cam->extpipe_filename, "movie_end");

        if ((cam->conf->movie_retain == "secondary") &&
            (cam->algsec_inuse) && (cam->algsec->isdetected == false)) {
            if (remove(cam->extpipe_filename) != 0) {
                MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                    , _("Unable to remove file %s"), cam->extpipe_filename);
            } else {
                MOTPLS_LOG(INF, TYPE_EVENTS, NO_ERRNO
                    , _("No secondary detection. Removed file %s.  ")
                    , cam->extpipe_filename);
            }
        }
    }
}

static void event_tlapse_start(ctx_dev *cam)
{
    int retcd;

    if (!cam->movie_timelapse) {
        retcd = movie_init_timelapse(cam);
        if (retcd < 0) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("Error creating timelapse file [%s]"), cam->movie_timelapse->full_nm);
            myfree(&cam->movie_timelapse);
            return;
        }
        cam->filetype = FTYPE_MOVIE_TIMELAPSE;
        on_movie_start_command(cam, cam->movie_timelapse->full_nm);
        dbse_exec(cam, cam->movie_timelapse->full_nm, "movie_start");
    }

    if (movie_put_image(cam->movie_timelapse
            , cam->current_image, &cam->current_image->imgts) == -1) {
        MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
    }

}

static void event_tlapse_end(ctx_dev *cam)
{
    if (cam->movie_timelapse) {
        cam->filetype = FTYPE_MOVIE_TIMELAPSE;
        movie_close(cam->movie_timelapse);
        on_movie_end_command(cam, cam->movie_timelapse->full_nm);
        dbse_exec(cam, cam->movie_timelapse->full_nm, "movie_end");
        movie_free(cam->movie_timelapse);
        myfree(&cam->movie_timelapse);
    }
}

struct event_handlers {
    motion_event type;
    event_handler handler;
};

struct event_handlers event_handlers[] = {
    {
    EVENT_MOTION,
    on_motion_detected_command
    },
    {
    EVENT_AREA_DETECTED,
    on_area_command
    },
    {
    EVENT_START,
    on_event_start_command
    },
    {
    EVENT_START,
    event_movie_start
    },
    {
    EVENT_END,
    on_event_end_command
    },
    {
    EVENT_END,
    event_movie_end
    },
    {
    EVENT_IMAGE_DETECTED,
    event_image_detect
    },
    {
    EVENT_IMAGE_DETECTED,
    event_movie_put
    },
    {
    EVENT_IMAGEM_DETECTED,
    event_imagem_detect
    },
    {
    EVENT_IMAGE_SNAPSHOT,
    event_image_snapshot
    },
    {
    EVENT_IMAGE,
    event_vlp_putpipe
    },
    {
    EVENT_IMAGEM,
    event_vlp_putpipem
    },
    {
    EVENT_IMAGE_PREVIEW,
    event_image_preview
    },
    {
    EVENT_STREAM,
    event_stream_put
    },
    {
    EVENT_MOVIE_PUT,
    event_movie_put
    },
    {
    EVENT_TLAPSE_START,
    event_tlapse_start
    },
    {
    EVENT_TLAPSE_END,
    event_tlapse_end
    },
    {
    EVENT_MOVIE_START,
    event_movie_start
    },
    {
    EVENT_MOVIE_END,
    event_movie_end
    },
    {
    EVENT_CAMERA_LOST,
    event_camera_lost
    },
    {
    EVENT_CAMERA_FOUND,
    event_camera_found
    },
    {
    EVENT_SECDETECT,
    event_secondary_detect
    },
    {(motion_event)0, NULL}
};


void event(ctx_dev *cam, motion_event evnt)
{
    int i=-1;

    while (event_handlers[++i].handler) {
        if (evnt == event_handlers[i].type) {
            event_handlers[i].handler(cam);
        }
    }
}
