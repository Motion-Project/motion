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
#include "netcam.hpp"
#include "movie.hpp"
#include "event.hpp"
#include "dbse.hpp"
#include "video_loopback.hpp"
#include "video_common.hpp"
#include "webu_stream.hpp"
#include "alg_sec.hpp"

const char *eventList[] = {
    "NULL",
    "EVENT_FILECREATE",
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
    "EVENT_FILECLOSE",
    "EVENT_AREA_DETECTED",
    "EVENT_CAMERA_LOST",
    "EVENT_CAMERA_FOUND",
    "EVENT_MOVIE_PUT",
    "EVENT_LAST"
};

static void event_newfile(ctx_dev *cam, char *fname, void *ftype)
{
    (void)cam;
    (void)ftype;

    MOTPLS_LOG(NTC, TYPE_EVENTS, NO_ERRNO
        ,_("File saved to: %s"), fname);
}


/**
 * on_picture_save_command
 *      handles both on_picture_save and on_movie_start
 *      If arg = FTYPE_IMAGE_ANY on_picture_save script is executed
 *      If arg = FTYPE_MOVIE_ANY on_movie_start script is executed
 *      The scripts are executed with the filename of picture or movie appended
 *      to the config parameter.
 */
static void on_picture_save_command(ctx_dev *cam, char *fname, void *ftype)
{
    /*Fix me*/
    long filetype = (long)ftype;

    if ((filetype & FTYPE_IMAGE_ANY) != 0 && (cam->conf->on_picture_save != "")) {
        util_exec_command(cam, cam->conf->on_picture_save.c_str(), fname, (int)filetype);
    }

    if ((filetype & FTYPE_MOVIE_ANY) != 0 && (cam->conf->on_movie_start != "")) {
        util_exec_command(cam, cam->conf->on_movie_start.c_str(), fname, (int)filetype);
    }
}

static void on_motion_detected_command(ctx_dev *cam, char *fname, void *ftype)
{
    (void)fname;
    (void)ftype;

    if (cam->conf->on_motion_detected != "") {
        util_exec_command(cam, cam->conf->on_motion_detected.c_str(), NULL, 0);
    }
}

static void on_area_command(ctx_dev *cam
        , char *fname, void *ftype)
{

    (void)fname;
    (void)ftype;

    if (cam->conf->on_area_detected != "") {
        util_exec_command(cam, cam->conf->on_area_detected.c_str(), NULL, 0);
    }
}

static void on_event_start_command(ctx_dev *cam
        , char *fname, void *ftype)
{

    (void)fname;
    (void)ftype;

    if (cam->conf->on_event_start != "") {
        util_exec_command(cam, cam->conf->on_event_start.c_str(), NULL, 0);
    }
}

static void on_event_end_command(ctx_dev *cam
        , char *fname, void *ftype)
{

    (void)fname;
    (void)ftype;

    if (cam->conf->on_event_end != "") {
        util_exec_command(cam, cam->conf->on_event_end.c_str(), NULL, 0);
    }
}

static void event_stream_put(ctx_dev *cam
        , char *fname, void *ftype)
{

    (void)fname;
    (void)ftype;

    webu_stream_getimg(cam);

}


static void event_vlp_putpipe(ctx_dev *cam
        , char *fname, void *ftype)
{

    (void)fname;

    if (*(int *)ftype >= 0) {
        if (vlp_putpipe(*(int *)ftype
                , cam->current_image->image_norm
                , cam->imgs.size_norm) == -1) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                ,_("Failed to put image into video pipe"));
        }
    }
}

static void event_vlp_putpipem(ctx_dev *cam
        , char *fname, void *ftype)
{

    (void)fname;

    if (*(int *)ftype >= 0) {
        if (vlp_putpipe(*(int *)ftype
                , cam->imgs.image_motion.image_norm
                , cam->imgs.size_norm) == -1) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                ,_("Failed to put image into video pipe"));
        }
    }
}

const char *imageext(ctx_dev *cam) {

    if (cam->conf->picture_type == "ppm") return "ppm";
    if (cam->conf->picture_type == "webp") return "webp";
    return "jpg";
}

static void event_image_detect(ctx_dev *cam
        , char *fname, void *ftype)
{
    char fullfilename[PATH_MAX];
    char filename[PATH_MAX];
    int  passthrough, retcd;


    (void)fname;
    (void)ftype;

    if (cam->new_img & NEWIMG_ON) {
        mystrftime(cam, filename, sizeof(filename)
            , cam->conf->picture_filename.c_str()
            , &cam->current_image->imgts, NULL, 0);
        retcd = snprintf(fullfilename, PATH_MAX, "%s/%s.%s"
            , cam->conf->target_dir.c_str(), filename, imageext(cam));
        if ((retcd < 0) || (retcd >= PATH_MAX)) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("Error creating image file name"));
            return;
        }
        passthrough = mycheck_passthrough(cam);
        if ((cam->imgs.size_high > 0) && (!passthrough)) {
            pic_save_norm(cam, fullfilename
                ,cam->current_image->image_high, FTYPE_IMAGE);
        } else {
            pic_save_norm(cam, fullfilename
                ,cam->current_image->image_norm, FTYPE_IMAGE);
        }
        event(cam, EVENT_FILECREATE, fullfilename, (void *)FTYPE_IMAGE);
        dbse_exec(cam, fullfilename, FTYPE_IMAGE
            , &cam->current_image->imgts, "pic_save");
    }
}

static void event_imagem_detect(ctx_dev *cam
        , char *fname, void *ftype)
{
    char fullfilename[PATH_MAX];
    char filename[PATH_MAX];
    int retcd;


    (void)fname;
    (void)ftype;

    if (cam->conf->picture_output_motion == "on") {
        mystrftime(cam, filename, sizeof(filename)
            , cam->conf->picture_filename.c_str()
            , &cam->current_image->imgts, NULL, 0);
        retcd = snprintf(fullfilename, PATH_MAX, "%s/%sm.%s"
            , cam->conf->target_dir.c_str(), filename, imageext(cam));
        if ((retcd < 0) || (retcd >= PATH_MAX)) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("Error creating image motion file name"));
            return;
        }
        pic_save_norm(cam, fullfilename, cam->imgs.image_motion.image_norm, FTYPE_IMAGE_MOTION);
        event(cam, EVENT_FILECREATE, fullfilename
            , (void *)FTYPE_IMAGE_MOTION);
        dbse_exec(cam, fullfilename, FTYPE_IMAGE_MOTION
            , &cam->current_image->imgts, "pic_save");
    } else if (cam->conf->picture_output_motion == "roi") {
        mystrftime(cam, filename, sizeof(filename)
            , cam->conf->picture_filename.c_str()
            , &cam->current_image->imgts, NULL, 0);
        retcd = snprintf(fullfilename, PATH_MAX, "%s/%sr.%s"
            , cam->conf->target_dir.c_str(), filename, imageext(cam));
        if ((retcd < 0) || (retcd >= PATH_MAX)) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("Error creating image motion roi file name"));
            return;
        }
        pic_save_roi(cam, fullfilename, cam->current_image->image_norm);
        event(cam, EVENT_FILECREATE, fullfilename
            , (void *)FTYPE_IMAGE_ROI);
        dbse_exec(cam, fullfilename, FTYPE_IMAGE_ROI
            , &cam->current_image->imgts, "pic_save");
    }
}

static void event_image_snapshot(ctx_dev *cam
        , char *fname, void *ftype)
{
    char fullfilename[PATH_MAX];
    char filename[PATH_MAX];
    char filepath[PATH_MAX];
    char linkpath[PATH_MAX];
    int offset, retcd, passthrough;


    (void)fname;
    (void)ftype;

    offset = (int)cam->conf->snapshot_filename.length() - 8;
    if (offset < 0) {
        offset = 1;
    }

    if (cam->conf->snapshot_filename.compare(offset, 8, "lastsnap") != 0) {
        mystrftime(cam, filepath, sizeof(filepath)
            , cam->conf->snapshot_filename.c_str()
            , &cam->current_image->imgts, NULL, 0);
        retcd = snprintf(filename, PATH_MAX, "%s.%s", filepath, imageext(cam));
        if (retcd <0) {
            MOTPLS_LOG(INF, TYPE_STREAM, NO_ERRNO, _("Error option"));
        }

        retcd =snprintf(fullfilename, PATH_MAX, "%s/%s"
            , cam->conf->target_dir.c_str(), filename);
        if (retcd <0) {
            MOTPLS_LOG(INF, TYPE_STREAM, NO_ERRNO, _("Error option"));
        }
        passthrough = mycheck_passthrough(cam);
        if ((cam->imgs.size_high > 0) && (!passthrough)) {
            pic_save_norm(cam, fullfilename
                , cam->current_image->image_high, FTYPE_IMAGE_SNAPSHOT);
        } else {
            pic_save_norm(cam, fullfilename
                , cam->current_image->image_norm, FTYPE_IMAGE_SNAPSHOT);
        }
        event(cam, EVENT_FILECREATE, fullfilename
            , (void *)FTYPE_IMAGE_SNAPSHOT);
        dbse_exec(cam, fullfilename, FTYPE_IMAGE_SNAPSHOT
            , &cam->current_image->imgts, "pic_save");

        /* Update symbolic link */
        snprintf(linkpath, PATH_MAX, "%s/lastsnap.%s"
            , cam->conf->target_dir.c_str(), imageext(cam));
        remove(linkpath);
        if (symlink(filename, linkpath)) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                ,_("Could not create symbolic link [%s]"), filename);
            return;
        }
    } else {
        mystrftime(cam, filepath, sizeof(filepath)
            , cam->conf->snapshot_filename.c_str()
            , &cam->current_image->imgts, NULL, 0);
        retcd = snprintf(filename, PATH_MAX, "%s.%s", filepath, imageext(cam));
        if (retcd <0) {
            MOTPLS_LOG(INF, TYPE_STREAM, NO_ERRNO, _("Error option"));
        }

        retcd = snprintf(fullfilename, PATH_MAX, "%s/%s"
            , cam->conf->target_dir.c_str(), filename);
        if (retcd <0) {
            MOTPLS_LOG(INF, TYPE_STREAM, NO_ERRNO, _("Error option"));
        }

        remove(fullfilename);
        passthrough = mycheck_passthrough(cam);
        if ((cam->imgs.size_high > 0) && (!passthrough)) {
            pic_save_norm(cam, fullfilename
                , cam->current_image->image_high, FTYPE_IMAGE_SNAPSHOT);
        } else {
            pic_save_norm(cam, fullfilename
                , cam->current_image->image_norm, FTYPE_IMAGE_SNAPSHOT);
        }
        event(cam, EVENT_FILECREATE, fullfilename
            , (void *)FTYPE_IMAGE_SNAPSHOT);
        dbse_exec(cam, fullfilename, FTYPE_IMAGE_SNAPSHOT
            , &cam->current_image->imgts, "pic_save");
    }

    cam->snapshot = 0;
}

static void event_image_preview(ctx_dev *cam
        , char *fname, void *ftype)
{
    char previewname[PATH_MAX];
    char filename[PATH_MAX];
    ctx_image_data *saved_current_image;
    int passthrough, retcd;


    (void)fname;
    (void)ftype;

    if (cam->imgs.image_preview.diffs) {
        saved_current_image = cam->current_image;
        cam->current_image = &cam->imgs.image_preview;

        mystrftime(cam, filename, sizeof(filename), cam->conf->picture_filename.c_str()
            , &cam->imgs.image_preview.imgts, NULL, 0);

        retcd = snprintf(previewname, PATH_MAX, "%s/%s.%s"
            , cam->conf->target_dir.c_str(), filename, imageext(cam));
        if ((retcd < 0) || (retcd >= PATH_MAX)) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("Error creating preview file name"));
            return;
        }
        passthrough = mycheck_passthrough(cam);
        if ((cam->imgs.size_high > 0) && (!passthrough)) {
            pic_save_norm(cam, previewname
                , cam->imgs.image_preview.image_high , FTYPE_IMAGE);
        } else {
            pic_save_norm(cam, previewname
                , cam->imgs.image_preview.image_norm, FTYPE_IMAGE);
        }
        event(cam, EVENT_FILECREATE, previewname, (void *)FTYPE_IMAGE);
        dbse_exec(cam, previewname, FTYPE_IMAGE
            , &cam->current_image->imgts, "pic_save");

        /* Restore global context values. */
        cam->current_image = saved_current_image;
    }
}

static void event_camera_lost(ctx_dev *cam
        , char *fname, void *ftype)
{

    (void)fname;
    (void)ftype;

    if (cam->conf->on_camera_lost != "") {
        util_exec_command(cam, cam->conf->on_camera_lost.c_str(), NULL, 0);
    }
}

static void event_secondary_detect(ctx_dev *cam
        , char *fname, void *ftype)
{

    (void)fname;
    (void)ftype;

    MOTPLS_LOG(NTC, TYPE_EVENTS, NO_ERRNO,_("Event secondary detect"));

    if (cam->conf->on_secondary_detect != "") {
        util_exec_command(cam, cam->conf->on_secondary_detect.c_str(), NULL, 0);
    }
}

static void event_camera_found(ctx_dev *cam
        , char *fname, void *ftype)
{

    (void)fname;
    (void)ftype;

    if (cam->conf->on_camera_found != "") {
        util_exec_command(cam, cam->conf->on_camera_found.c_str(), NULL, 0);
    }
}

static void on_movie_end_command(ctx_dev *cam
        , char *fname, void *ftype)
{
    long filetype = (long) ftype;



    if ((filetype & FTYPE_MOVIE_ANY) && (cam->conf->on_movie_end != "")) {
        util_exec_command(cam, cam->conf->on_movie_end.c_str(), fname, (int)filetype);
    }
}

static void event_extpipe_end(ctx_dev *cam
        , char *fname, void *ftype)
{
    int retcd;

    (void)fname;
    (void)ftype;

    if (cam->extpipe_open) {
        MOTPLS_LOG(NTC, TYPE_EVENTS, NO_ERRNO,_("Closing extpipe"));
        cam->extpipe_open = 0;
        fflush(cam->extpipe);
        pclose(cam->extpipe);

        if ((cam->conf->movie_retain == "secondary") && (cam->algsec_inuse)) {
            if (cam->algsec->isdetected == false) {
                retcd = remove(cam->extpipefilename);
                if (retcd != 0) {
                    MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                        , _("Unable to remove file %s"), cam->extpipefilename);
                }
            } else {
                event(cam, EVENT_FILECLOSE, cam->extpipefilename
                    , (void *)FTYPE_MOVIE);
                dbse_exec(cam,  cam->extpipefilename
                    , FTYPE_MOVIE, &cam->current_image->imgts, "movie_end");
            }
        } else {
            event(cam, EVENT_FILECLOSE, cam->extpipefilename
                , (void *)FTYPE_MOVIE);
            dbse_exec(cam, cam->extpipefilename
                , FTYPE_MOVIE, &cam->current_image->imgts, "movie_end");
        }
        cam->extpipe = NULL;
    }
}

static void event_extpipe_start(ctx_dev *cam
        , char *fname, void *ftype)
{
    int retcd;
    char stamp[PATH_MAX] = "";


    (void)fname;
    (void)ftype;

    if ((cam->conf->movie_extpipe_use) && (cam->conf->movie_extpipe != "" )) {
        mystrftime(cam, stamp, sizeof(stamp)
            , cam->conf->movie_filename.c_str()
            , &cam->current_image->imgts, NULL, 0);
        retcd = snprintf(cam->extpipefilename, PATH_MAX - 4, "%s/%s"
            , cam->conf->target_dir.c_str(), stamp);
        if (retcd < 0) {
            MOTPLS_LOG(INF, TYPE_STREAM, NO_ERRNO, _("Error %d"), retcd);
        }

        if (access(cam->conf->target_dir.c_str(), W_OK)!= 0) {
            /* Permission denied */
            if (errno ==  EACCES) {
                MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                    ,_("no write access to target directory %s")
                    , cam->conf->target_dir.c_str());
                return ;
            /* Path not found - create it */
            } else if (errno ==  ENOENT) {
                MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                    ,_("path not found, trying to create it %s ...")
                    , cam->conf->target_dir.c_str());
                if (mycreate_path(cam->extpipefilename) == -1) {
                    return ;
                }
            }
            else {
                MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                    ,_("error accesing path %s"), cam->conf->target_dir.c_str());
                return ;
            }
        }

        /* Always create any path specified as file name */
        if (mycreate_path(cam->extpipefilename) == -1) {
            return ;
        }

        mystrftime(cam, stamp, sizeof(stamp), cam->conf->movie_extpipe.c_str()
            , &cam->current_image->imgts, cam->extpipefilename, 0);

        retcd = snprintf(cam->extpipecmdline, PATH_MAX, "%s", stamp);
        if ((retcd < 0 ) || (retcd >= PATH_MAX)) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                , _("Error specifying command line: %s"), cam->extpipecmdline);
            return;
        }
        MOTPLS_LOG(NTC, TYPE_EVENTS, NO_ERRNO
            , _("fps %d pipe: %s"), cam->movie_fps, cam->extpipecmdline);

        event(cam, EVENT_FILECREATE
            , cam->extpipefilename, (void *)FTYPE_MOVIE);
        dbse_exec(cam, cam->extpipefilename, FTYPE_MOVIE
            , &cam->current_image->imgts, "movie_start");
        cam->extpipe = popen(cam->extpipecmdline, "we");

        if (cam->extpipe == NULL) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, _("popen failed"));
            return;
        }

        setbuf(cam->extpipe, NULL);
        cam->extpipe_open = 1;
    }
}

static void event_extpipe_put(ctx_dev *cam
        , char *fname, void *ftype)
{

    (void)fname;
    (void)ftype;
    int passthrough;

    /* Check use_extpipe enabled and ext_pipe not NULL */
    if ((cam->conf->movie_extpipe_use) &&
        (cam->extpipe != NULL) &&
        (cam->finish_dev == false)) {
        passthrough = mycheck_passthrough(cam);
        /* Check that is open */
        if ((cam->extpipe_open) && (fileno(cam->extpipe) > 0)) {
            if ((cam->imgs.size_high > 0) && (!passthrough)) {
                if (!fwrite(cam->current_image->image_high
                        , cam->imgs.size_high, 1, cam->extpipe)) {
                    MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                        ,_("Error writing in pipe , state error %d"), ferror(cam->extpipe));
                }
            } else {
                if (!fwrite(cam->current_image->image_norm
                        , cam->imgs.size_norm, 1, cam->extpipe)) {
                    MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                        ,_("Error writing in pipe , state error %d"), ferror(cam->extpipe));
                }
           }
        } else {
            MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("pipe %s not created or closed already "), cam->extpipecmdline);
        }
    }
}

static void event_movie_start(ctx_dev *cam
        , char *fname, void *ftype)
{
    int retcd;


    (void)fname;
    (void)ftype;

    /* This will cascade to extpipe_start*/
    cam->movie_start_time = cam->frame_curr_ts.tv_sec;

    if (cam->lastrate < 2) {
        cam->movie_fps = 2;
    } else {
        cam->movie_fps = cam->lastrate;
    }

    if (cam->conf->movie_output) {
        retcd = movie_init_norm(cam, &cam->current_image->imgts);
        if (retcd < 0) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("Error initializing movie output."));
            myfree(&cam->movie_norm);
            return;
        }
        event(cam, EVENT_FILECREATE, cam->movie_norm->full_nm
            , (void *)FTYPE_MOVIE);
        dbse_exec(cam, cam->movie_norm->full_nm
            , FTYPE_MOVIE, &cam->current_image->imgts, "movie_start");
    }

    if (cam->conf->movie_output_motion) {
        retcd = movie_init_motion(cam, &cam->current_image->imgts);
        if (retcd < 0) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("Error creating motion file [%s]"), cam->movie_motion->full_nm);
            myfree(&cam->movie_motion);
            return;
        }
    }
}

static void event_movie_put(ctx_dev *cam
        , char *fname, void *ftype)
{

    (void)fname;
    (void)ftype;

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
}

static void event_movie_end(ctx_dev *cam
        , char *fname, void *ftype)
{
    int retcd;

    (void)fname;
    (void)ftype;

    if (cam->movie_norm) {
        if ((cam->conf->movie_retain == "secondary") && (cam->algsec_inuse)) {
            if (cam->algsec->isdetected == false) {
                retcd = remove(cam->movie_norm->full_nm);
                if (retcd != 0) {
                    MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                        , _("Unable to remove file %s")
                        , cam->movie_norm->full_nm);
                }
            } else {
                event(cam, EVENT_FILECLOSE, cam->movie_norm->full_nm
                    , (void *)FTYPE_MOVIE);
                dbse_exec(cam, cam->movie_norm->full_nm, FTYPE_MOVIE
                    , &cam->current_image->imgts, "movie_end");
                dbse_movies_addrec(cam, cam->movie_norm
                    , &cam->current_image->imgts);
            }
        } else {
            event(cam, EVENT_FILECLOSE, cam->movie_norm->full_nm
                , (void *)FTYPE_MOVIE);
            dbse_exec(cam, cam->movie_norm->full_nm
                , FTYPE_MOVIE, &cam->current_image->imgts, "movie_end");
            dbse_movies_addrec(cam, cam->movie_norm
                , &cam->current_image->imgts);
        }
        movie_close(cam->movie_norm);
        myfree(&cam->movie_norm);

    }

    if (cam->movie_motion) {
        if ((cam->conf->movie_retain == "secondary") && (cam->algsec_inuse)) {
            if (cam->algsec->isdetected == false) {
                retcd = remove(cam->movie_motion->full_nm);
                if (retcd != 0) {
                    MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                        , _("Unable to remove file %s")
                        , cam->movie_motion->full_nm);
                }
            } else {
                event(cam, EVENT_FILECLOSE, cam->movie_motion->full_nm
                    , (void *)FTYPE_MOVIE_MOTION);
                dbse_exec(cam, cam->movie_motion->full_nm
                    , FTYPE_MOVIE_MOTION
                    , &cam->imgs.image_motion.imgts, "movie_end");
                dbse_movies_addrec(cam, cam->movie_motion
                    , &cam->imgs.image_motion.imgts);
            }
        } else {
            event(cam, EVENT_FILECLOSE
                , cam->movie_motion->full_nm
                , (void *)FTYPE_MOVIE_MOTION);
            dbse_exec(cam, cam->movie_motion->full_nm
                , FTYPE_MOVIE_MOTION
                , &cam->imgs.image_motion.imgts, "movie_end");
            dbse_movies_addrec(cam, cam->movie_motion
                , &cam->imgs.image_motion.imgts);
        }
        movie_close(cam->movie_motion);
        myfree(&cam->movie_motion);
    }

}

static void event_tlapse_start(ctx_dev *cam
        , char *fname, void *ftype)
{
    int retcd;


    (void)fname;
    (void)ftype;

    if (!cam->movie_timelapse) {
        retcd = movie_init_timelapse(cam);
        if (retcd < 0) {
            MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("Error creating timelapse file [%s]"), cam->movie_timelapse->full_nm);
            myfree(&cam->movie_timelapse);
            return;
        }
        event(cam, EVENT_FILECREATE, cam->movie_timelapse->full_nm
            , (void *)FTYPE_MOVIE_TIMELAPSE);
        dbse_exec(cam, cam->movie_timelapse->full_nm, FTYPE_MOVIE_TIMELAPSE
            , &cam->current_image->imgts, "movie_start");
    }

    if (movie_put_image(cam->movie_timelapse
            , cam->current_image, &cam->current_image->imgts) == -1) {
        MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
    }

}

static void event_tlapse_end(ctx_dev *cam
        , char *fname, void *ftype)
{

    (void)fname;
    (void)ftype;

    if (cam->movie_timelapse) {
        event(cam, EVENT_FILECLOSE, cam->movie_timelapse->full_nm
            , (void *)FTYPE_MOVIE_TIMELAPSE);
        dbse_exec(cam, cam->movie_timelapse->full_nm
            , FTYPE_MOVIE_TIMELAPSE, &cam->current_image->imgts, "movie_end");
        movie_close(cam->movie_timelapse);
        myfree(&cam->movie_timelapse);
    }
}

struct event_handlers {
    motion_event type;
    event_handler handler;
};

struct event_handlers event_handlers[] = {
    {
    EVENT_FILECREATE,
    on_picture_save_command
    },
    {
    EVENT_FILECREATE,
    event_newfile
    },
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
    EVENT_START,
    event_extpipe_start
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
    EVENT_END,
    event_extpipe_end
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
    EVENT_IMAGE_DETECTED,
    event_extpipe_put
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
    EVENT_MOVIE_PUT,
    event_extpipe_put
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
    EVENT_FILECLOSE,
    on_movie_end_command
    },
    {
    EVENT_MOVIE_START,
    event_movie_start
    },
    {
    EVENT_MOVIE_START,
    event_extpipe_start
    },
    {
    EVENT_MOVIE_END,
    event_movie_end
    },
    {
    EVENT_MOVIE_END,
    event_extpipe_end
    },
    {
    EVENT_END,
    event_extpipe_end
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


/**
 * event
 *   defined with the following parameters:
 *      - Type as defined in event.h (EVENT_...)
 *      - The global context struct cam
  *      - filename - A pointer to typically a string for a file path
 *      - eventdata - A void pointer that can be cast to anything. E.g. FTYPE_...
 */
void event(ctx_dev *cam, motion_event evnt, char *fname,void *ftype)
{
    int i=-1;

    while (event_handlers[++i].handler) {
        if (evnt == event_handlers[i].type) {
            event_handlers[i].handler(cam, fname, ftype);
        }
    }
}
