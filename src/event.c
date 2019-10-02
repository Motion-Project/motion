/*
    event.c

    Generalised event handling for motion

    Copyright Jeroen Vreeken, 2002
    This software is distributed under the GNU Public License Version 2
    see also the file 'COPYING'.
*/
#include "motion.h"
#include "logger.h"
#include "util.h"
#include "picture.h"
#include "netcam.h"
#include "movie.h"
#include "event.h"
#include "dbse.h"
#include "video_loopback.h"
#include "video_common.h"
#include "webu_stream.h"

/* Various functions (most doing the actual action)
 * TODO Items:
 * Rework the snprintf uses.
 * Edit directories so they can never be null and eliminate defaults from here
 * Move the movie initialize stuff to movie module
 * eliminate #if for v4l2
 * Eliminate #IF for database items
 * Move database functions out of here.
 * Move stream stuff to webu_stream
 * Use (void) alternative for ATTRIBUTE_UNUSED
 */

const char *eventList[] = {
    "NULL",
    "EVENT_FILECREATE",
    "EVENT_MOTION",
    "EVENT_FIRSTMOTION",
    "EVENT_ENDMOTION",
    "EVENT_STOP",
    "EVENT_TIMELAPSE",
    "EVENT_TIMELAPSEEND",
    "EVENT_STREAM",
    "EVENT_IMAGE_DETECTED",
    "EVENT_IMAGEM_DETECTED",
    "EVENT_IMAGE_SNAPSHOT",
    "EVENT_IMAGE",
    "EVENT_IMAGEM",
    "EVENT_IMAGE_PREVIEW",
    "EVENT_FILECLOSE",
    "EVENT_DEBUG",
    "EVENT_CRITICAL",
    "EVENT_AREA_DETECTED",
    "EVENT_CAMERA_LOST",
    "EVENT_CAMERA_FOUND",
    "EVENT_MOVIE_PUT",
    "EVENT_LAST"
};

/**
 * eventToString
 *
 * returns string label of the event
 */
 /**
 * Future use debug / notification function
static const char *eventToString(motion_event e)
{
    return eventList[(int)e];
}
*/

/**
 * exec_command
 *      Execute 'command' with 'arg' as its argument.
 *      if !arg command is started with no arguments
 *      Before we call execl we need to close all the file handles
 *      that the fork inherited from the parent in order not to pass
 *      the open handles on to the shell
 */
static void exec_command(struct ctx_cam *cam, char *command, char *filename, int filetype)
{
    char stamp[PATH_MAX];
    mystrftime(cam, stamp, sizeof(stamp), command, &cam->current_image->imgts, filename, filetype);

    if (!fork()) {
        int i;

        /* Detach from parent */
        setsid();

        /*
         * Close any file descriptor except console because we will
         * like to see error messages
         */
        for (i = getdtablesize() - 1; i > 2; i--)
            close(i);

        execl("/bin/sh", "sh", "-c", stamp, " &", NULL);

        /* if above function succeeds the program never reach here */
        MOTION_LOG(ALR, TYPE_EVENTS, SHOW_ERRNO
            ,_("Unable to start external command '%s'"), stamp);

        exit(1);
    }

    MOTION_LOG(DBG, TYPE_EVENTS, NO_ERRNO
        ,_("Executing external command '%s'"), stamp);
}

static void event_newfile(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    (void)cam;
    (void)evnt;
    (void)img_data;
    (void)ts1;

    MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO
        ,_("File of type %ld saved to: %s")
        ,(unsigned long)ftype, fname);
}


static void event_beep(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    (void)evnt;
    (void)img_data;
    (void)fname;
    (void)ftype;
    (void)ts1;

    if (!cam->conf.quiet)
        printf("\a");
}

/**
 * on_picture_save_command
 *      handles both on_picture_save and on_movie_start
 *      If arg = FTYPE_IMAGE_ANY on_picture_save script is executed
 *      If arg = FTYPE_MPEG_ANY on_movie_start script is executed
 *      The scripts are executed with the filename of picture or movie appended
 *      to the config parameter.
 */
static void on_picture_save_command(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    int filetype = (unsigned long)ftype;

    (void)evnt;
    (void)img_data;
    (void)ts1;

    if ((filetype & FTYPE_IMAGE_ANY) != 0 && cam->conf.on_picture_save)
        exec_command(cam, cam->conf.on_picture_save, fname, filetype);

    if ((filetype & FTYPE_MPEG_ANY) != 0 && cam->conf.on_movie_start)
        exec_command(cam, cam->conf.on_movie_start, fname, filetype);
}

static void on_motion_detected_command(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    (void)evnt;
    (void)img_data;
    (void)fname;
    (void)ftype;
    (void)ts1;

    if (cam->conf.on_motion_detected)
        exec_command(cam, cam->conf.on_motion_detected, NULL, 0);
}

static void event_sqlfirstmotion(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    (void)evnt;
    (void)img_data;
    (void)fname;
    (void)ftype;
    (void)ts1;

    if (!(cam->conf.database_type)) {
        return;
    } else {
        dbse_firstmotion(cam);
    }
}

static void event_sqlnewfile(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    int sqltype = (unsigned long)ftype;

    (void)evnt;
    (void)img_data;

    /* Only log the file types we want */
    if (!(cam->conf.database_type) || (sqltype & cam->dbse->sql_mask) == 0){
        return;
    } else {
        dbse_newfile(cam, fname, sqltype, ts1);
    }

}

static void event_sqlfileclose(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    int sqltype = (unsigned long)ftype;

    (void)evnt;
    (void)img_data;

    /* Only log the file types we want */
    if (!(cam->conf.database_type) || (sqltype & cam->dbse->sql_mask) == 0){
        return;
    } else {
        dbse_fileclose(cam, fname, sqltype, ts1);
    }


}

static void on_area_command(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    (void)evnt;
    (void)img_data;
    (void)fname;
    (void)ftype;
    (void)ts1;

    if (cam->conf.on_area_detected)
        exec_command(cam, cam->conf.on_area_detected, NULL, 0);
}

static void on_event_start_command(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    (void)evnt;
    (void)img_data;
    (void)fname;
    (void)ftype;
    (void)ts1;

    if (cam->conf.on_event_start)
        exec_command(cam, cam->conf.on_event_start, NULL, 0);
}

static void on_event_end_command(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    (void)evnt;
    (void)img_data;
    (void)fname;
    (void)ftype;
    (void)ts1;

    if (cam->conf.on_event_end)
        exec_command(cam, cam->conf.on_event_end, NULL, 0);
}

static void event_stream_put(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    (void)evnt;
    (void)fname;
    (void)ftype;
    (void)ts1;

    webu_stream_getimg(cam, img_data);

}


static void event_vlp_putpipe(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    (void)evnt;
    (void)fname;
    (void)ts1;

    if (*(int *)ftype >= 0) {
        if (vlp_putpipe(*(int *)ftype, img_data->image_norm, cam->imgs.size_norm) == -1)
            MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                ,_("Failed to put image into video pipe"));
    }
}

const char *imageext(struct ctx_cam *cam) {

    if (cam->imgs.picture_type == IMAGE_TYPE_PPM)
        return "ppm";

    if (cam->imgs.picture_type == IMAGE_TYPE_WEBP)
        return "webp";

    return "jpg";
}

static void event_image_detect(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    char fullfilename[PATH_MAX];
    char filename[PATH_MAX];
    int  passthrough;
    const char *imagepath;

    (void)evnt;
    (void)fname;
    (void)ftype;

    if (cam->new_img & NEWIMG_ON) {
        /*
         *  conf.imagepath would normally be defined but if someone deleted it by control interface
         *  it is better to revert to the default than fail
         */
        if (cam->conf.picture_filename)
            imagepath = cam->conf.picture_filename;
        else
            imagepath = DEF_IMAGEPATH;

        mystrftime(cam, filename, sizeof(filename), imagepath, ts1, NULL, 0);
        snprintf(fullfilename, PATH_MAX, "%.*s/%.*s.%s"
            , (int)(PATH_MAX-2-strlen(filename)-strlen(imageext(cam)))
            , cam->conf.target_dir
            , (int)(PATH_MAX-2-strlen(cam->conf.target_dir)-strlen(imageext(cam)))
            , filename, imageext(cam));

        passthrough = mycheck_passthrough(cam);
        if ((cam->imgs.size_high > 0) && (!passthrough)) {
            put_picture(cam, fullfilename,img_data->image_high, FTYPE_IMAGE);
        } else {
            put_picture(cam, fullfilename,img_data->image_norm, FTYPE_IMAGE);
        }
        event(cam, EVENT_FILECREATE, NULL, fullfilename, (void *)FTYPE_IMAGE, ts1);
    }
}

static void event_imagem_detect(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    struct config *conf = &cam->conf;
    char fullfilenamem[PATH_MAX];
    char filename[PATH_MAX];
    char filenamem[PATH_MAX];
    const char *imagepath;

    (void)evnt;
    (void)img_data;
    (void)fname;
    (void)ftype;

    if (conf->picture_output_motion) {
        /*
         *  conf.picture_filename would normally be defined but if someone deleted it by control interface
         *  it is better to revert to the default than fail
         */
        if (cam->conf.picture_filename)
            imagepath = cam->conf.picture_filename;
        else
            imagepath = DEF_IMAGEPATH;

        mystrftime(cam, filename, sizeof(filename), imagepath, ts1, NULL, 0);

        /* motion images gets same name as normal images plus an appended 'm' */
        snprintf(filenamem, PATH_MAX, "%.*sm"
            , (int)(PATH_MAX-1-strlen(filename))
            , filename);
        snprintf(fullfilenamem, PATH_MAX, "%.*s/%.*s.%s"
            , (int)(PATH_MAX-2-strlen(filenamem)-strlen(imageext(cam)))
            , cam->conf.target_dir
            , (int)(PATH_MAX-2-strlen(cam->conf.target_dir)-strlen(imageext(cam)))
            , filenamem, imageext(cam));
        put_picture(cam, fullfilenamem, cam->imgs.image_motion.image_norm, FTYPE_IMAGE_MOTION);
        event(cam, EVENT_FILECREATE, NULL, fullfilenamem, (void *)FTYPE_IMAGE, ts1);
    }
}

static void event_image_snapshot(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    char fullfilename[PATH_MAX];
    char filename[PATH_MAX];
    char filepath[PATH_MAX];
    int offset = 0;
    int len = strlen(cam->conf.snapshot_filename);
    char linkpath[PATH_MAX];
    const char *snappath;

    (void)evnt;
    (void)fname;
    (void)ftype;

    if (len >= 9)
        offset = len - 8;

    if (mystrne(cam->conf.snapshot_filename+offset, "lastsnap")) {
        /*
         *  conf.snapshot_filename would normally be defined but if someone deleted it by control interface
         *  it is better to revert to the default than fail
         */
        if (cam->conf.snapshot_filename)
            snappath = cam->conf.snapshot_filename;
        else
            snappath = DEF_SNAPPATH;

        mystrftime(cam, filepath, sizeof(filepath), snappath, ts1, NULL, 0);
        snprintf(filename, PATH_MAX, "%.*s.%s"
            , (int)(PATH_MAX-1-strlen(filepath)-strlen(imageext(cam)))
            , filepath, imageext(cam));
        snprintf(fullfilename, PATH_MAX, "%.*s/%.*s"
            , (int)(PATH_MAX-1-strlen(filename))
            , cam->conf.target_dir
            , (int)(PATH_MAX-1-strlen(cam->conf.target_dir))
            , filename);
        put_picture(cam, fullfilename, img_data->image_norm, FTYPE_IMAGE_SNAPSHOT);
        event(cam, EVENT_FILECREATE, NULL, fullfilename, (void *)FTYPE_IMAGE_SNAPSHOT, ts1);

        /*
         *  Update symbolic link *after* image has been written so that
         *  the link always points to a valid file.
         */
        snprintf(linkpath, PATH_MAX, "%.*s/lastsnap.%s"
            , (int)(PATH_MAX-strlen("/lastsnap.")-strlen(imageext(cam)))
            , cam->conf.target_dir, imageext(cam));

        remove(linkpath);

        if (symlink(filename, linkpath)) {
            MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                ,_("Could not create symbolic link [%s]"), filename);
            return;
        }
    } else {
        mystrftime(cam, filepath, sizeof(filepath), cam->conf.snapshot_filename, ts1, NULL, 0);
        snprintf(filename, PATH_MAX, "%.*s.%s"
            , (int)(PATH_MAX-1-strlen(imageext(cam)))
            , filepath, imageext(cam));
        snprintf(fullfilename, PATH_MAX, "%.*s/%.*s"
            , (int)(PATH_MAX-1-strlen(filename))
            , cam->conf.target_dir
            , (int)(PATH_MAX-1-strlen(cam->conf.target_dir))
            , filename);
        remove(fullfilename);
        put_picture(cam, fullfilename, img_data->image_norm, FTYPE_IMAGE_SNAPSHOT);
        event(cam, EVENT_FILECREATE, NULL, fullfilename, (void *)FTYPE_IMAGE_SNAPSHOT, ts1);
    }

    cam->snapshot = 0;
}

static void event_image_preview(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    int use_imagepath;
    const char *imagepath;
    char previewname[PATH_MAX];
    char filename[PATH_MAX];
    struct ctx_image_data *saved_current_image;
    int passthrough, retcd;

    (void)evnt;
    (void)img_data;
    (void)fname;
    (void)ftype;

    if (cam->imgs.image_preview.diffs) {
        saved_current_image = cam->current_image;
        cam->current_image = &cam->imgs.image_preview;

        /* Use filename of movie i.o. jpeg_filename when set to 'preview'. */
        use_imagepath = strcmp(cam->conf.picture_filename, "preview");

        if ((cam->movie_norm || (cam->conf.movie_extpipe_use && cam->extpipe)) && !use_imagepath) {

            if (cam->conf.movie_extpipe_use && cam->extpipe) {
                retcd = snprintf(previewname, PATH_MAX,"%s.%s"
                    , cam->extpipefilename, imageext(cam));
                if ((retcd < 0) || (retcd >= PATH_MAX)) {
                    MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                        ,_("Error creating preview pipe name %d %s")
                        ,retcd, previewname);
                    return;
                }
            } else {
                /* Replace avi/mpg with jpg/ppm and keep the rest of the filename. */
                /* TODO:  Hope that extensions are always 3 bytes*/
                /* -2 to allow for null terminating byte*/
                retcd = snprintf(filename, strlen(cam->newfilename) - 2
                    ,"%s", cam->newfilename);
                if (retcd < 0) {
                    MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                        ,_("Error creating file name base %d %s")
                        ,retcd, filename);
                    return;
                }
                retcd = snprintf(previewname, PATH_MAX
                    ,"%s%s", filename, imageext(cam));
                if ((retcd < 0) || (retcd >= PATH_MAX)) {
                    MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                        ,_("Error creating preview name %d %s")
                        , retcd, previewname);
                    return;
                }
            }

            passthrough = mycheck_passthrough(cam);
            if ((cam->imgs.size_high > 0) && (!passthrough)) {
                put_picture(cam, previewname, cam->imgs.image_preview.image_high , FTYPE_IMAGE);
            } else {
                put_picture(cam, previewname, cam->imgs.image_preview.image_norm , FTYPE_IMAGE);
            }
            event(cam, EVENT_FILECREATE, NULL, previewname, (void *)FTYPE_IMAGE, ts1);
        } else {
            /*
             * Save best preview-shot also when no movies are recorded or imagepath
             * is used. Filename has to be generated - nothing available to reuse!
             */

            /*
             * conf.picture_filename would normally be defined but if someone deleted it by
             * control interface it is better to revert to the default than fail.
             */
            if (cam->conf.picture_filename)
                imagepath = cam->conf.picture_filename;
            else
                imagepath = (char *)DEF_IMAGEPATH;

            mystrftime(cam, filename, sizeof(filename), imagepath, &cam->imgs.image_preview.imgts, NULL, 0);
            snprintf(previewname, PATH_MAX, "%.*s/%.*s.%s"
                , (int)(PATH_MAX-2-strlen(filename)-strlen(imageext(cam)))
                , cam->conf.target_dir
                , (int)(PATH_MAX-2-strlen(cam->conf.target_dir)-strlen(imageext(cam)))
                , filename, imageext(cam));

            passthrough = mycheck_passthrough(cam);
            if ((cam->imgs.size_high > 0) && (!passthrough)) {
                put_picture(cam, previewname, cam->imgs.image_preview.image_high , FTYPE_IMAGE);
            } else {
                put_picture(cam, previewname, cam->imgs.image_preview.image_norm, FTYPE_IMAGE);
            }
            event(cam, EVENT_FILECREATE, NULL, previewname, (void *)FTYPE_IMAGE, ts1);
        }

        /* Restore global context values. */
        cam->current_image = saved_current_image;
    }
}

static void event_camera_lost(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    (void)evnt;
    (void)img_data;
    (void)fname;
    (void)ftype;
    (void)ts1;

    if (cam->conf.on_camera_lost)
        exec_command(cam, cam->conf.on_camera_lost, NULL, 0);
}

static void event_camera_found(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    (void)evnt;
    (void)img_data;
    (void)fname;
    (void)ftype;
    (void)ts1;

    if (cam->conf.on_camera_found)
        exec_command(cam, cam->conf.on_camera_found, NULL, 0);
}

static void on_movie_end_command(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    int filetype = (unsigned long) ftype;

    (void)evnt;
    (void)img_data;
    (void)ts1;

    if ((filetype & FTYPE_MPEG_ANY) && cam->conf.on_movie_end)
        exec_command(cam, cam->conf.on_movie_end, fname, filetype);
}

static void event_extpipe_end(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    (void)evnt;
    (void)img_data;
    (void)fname;
    (void)ftype;

    if (cam->extpipe_open) {
        cam->extpipe_open = 0;
        fflush(cam->extpipe);
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO
            ,_("CLOSING: extpipe file desc %d, error state %d")
            ,fileno(cam->extpipe), ferror(cam->extpipe));
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "pclose return: %d",
                   pclose(cam->extpipe));
        event(cam, EVENT_FILECLOSE, NULL, cam->extpipefilename, (void *)FTYPE_MPEG, ts1);
    }
}

static void event_create_extpipe(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    int retcd;
    char stamp[PATH_MAX] = "";
    const char *moviepath;

    (void)evnt;
    (void)img_data;
    (void)fname;
    (void)ftype;

    if ((cam->conf.movie_extpipe_use) && (cam->conf.movie_extpipe)) {
        /*
         *  conf.mpegpath would normally be defined but if someone deleted it by control interface
         *  it is better to revert to the default than fail
         */
        if (cam->conf.movie_filename) {
            moviepath = cam->conf.movie_filename;
        } else {
            moviepath = DEF_MOVIEPATH;
            MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("moviepath: %s"), moviepath);
        }

        mystrftime(cam, stamp, sizeof(stamp), moviepath, ts1, NULL, 0);
        snprintf(cam->extpipefilename, PATH_MAX - 4, "%.*s/%.*s"
            , (int)(PATH_MAX-5-strlen(stamp))
            , cam->conf.target_dir
            , (int)(PATH_MAX-5-strlen(cam->conf.target_dir))
            , stamp);

        if (access(cam->conf.target_dir, W_OK)!= 0) {
            /* Permission denied */
            if (errno ==  EACCES) {
                MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                    ,_("no write access to target directory %s"), cam->conf.target_dir);
                return ;
            /* Path not found - create it */
            } else if (errno ==  ENOENT) {
                MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                    ,_("path not found, trying to create it %s ..."), cam->conf.target_dir);
                if (mycreate_path(cam->extpipefilename) == -1)
                    return ;
            }
            else {
                MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                    ,_("error accesing path %s"), cam->conf.target_dir);
                return ;
            }
        }

        /* Always create any path specified as file name */
        if (mycreate_path(cam->extpipefilename) == -1)
            return ;

        mystrftime(cam, stamp, sizeof(stamp), cam->conf.movie_extpipe, ts1, cam->extpipefilename, 0);

        retcd = snprintf(cam->extpipecmdline, PATH_MAX, "%s", stamp);
        if ((retcd < 0 ) || (retcd >= PATH_MAX)){
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                , _("Error specifying command line: %s"), cam->extpipecmdline);
            return;
        }
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("pipe: %s"), cam->extpipecmdline);

        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "cam->moviefps: %d", cam->movie_fps);

        event(cam, EVENT_FILECREATE, NULL, cam->extpipefilename, (void *)FTYPE_MPEG, ts1);
        cam->extpipe = popen(cam->extpipecmdline, "we");

        if (cam->extpipe == NULL) {
            MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, _("popen failed"));
            return;
        }

        setbuf(cam->extpipe, NULL);
        cam->extpipe_open = 1;
    }
}

static void event_extpipe_put(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    int passthrough;

    (void)evnt;
    (void)fname;
    (void)ftype;
    (void)ts1;

    /* Check use_extpipe enabled and ext_pipe not NULL */
    if ((cam->conf.movie_extpipe_use) && (cam->extpipe != NULL)) {
        MOTION_LOG(DBG, TYPE_EVENTS, NO_ERRNO, _("Using extpipe"));
        passthrough = mycheck_passthrough(cam);
        /* Check that is open */
        if ((cam->extpipe_open) && (fileno(cam->extpipe) > 0)) {
            if ((cam->imgs.size_high > 0) && (!passthrough)){
                if (!fwrite(img_data->image_high, cam->imgs.size_high, 1, cam->extpipe))
                    MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                        ,_("Error writing in pipe , state error %d"), ferror(cam->extpipe));
            } else {
                if (!fwrite(img_data->image_norm, cam->imgs.size_norm, 1, cam->extpipe))
                    MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                        ,_("Error writing in pipe , state error %d"), ferror(cam->extpipe));
           }
        } else {
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("pipe %s not created or closed already "), cam->extpipecmdline);
        }
    }
}

static void event_new_video(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    (void)evnt;
    (void)img_data;
    (void)fname;
    (void)ftype;
    (void)ts1;

    cam->movie_last_shot = -1;

    cam->movie_fps = cam->lastrate;

    MOTION_LOG(INF, TYPE_EVENTS, NO_ERRNO, _("Source FPS %d"), cam->movie_fps);

    if (cam->movie_fps < 2) cam->movie_fps = 2;

}

static void event_movie_newfile(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    int retcd;

    (void)evnt;
    (void)img_data;
    (void)fname;
    (void)ftype;

    if (!cam->conf.movie_output && !cam->conf.movie_output_motion) return;

    if (cam->conf.movie_output) {
        retcd = movie_init_norm(cam, ts1);
        if (retcd < 0){
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("Error opening ctx_cam for movie output."));
            free(cam->movie_norm);
            cam->movie_norm=NULL;
            return;
        }
        event(cam, EVENT_FILECREATE, NULL, cam->movie_norm->filename, (void *)FTYPE_MPEG, ts1);
    }

    if (cam->conf.movie_output_motion) {
        retcd = movie_init_motion(cam, ts1);
        if (retcd < 0){
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("Error creating motion file [%s]"), cam->movie_motion->filename);
            free(cam->movie_motion);
            cam->movie_motion = NULL;
            return;
        }
    }
}

static void event_movie_timelapse(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    int retcd;

    (void)evnt;
    (void)fname;
    (void)ftype;

    if (!cam->movie_timelapse) {
        retcd = movie_init_timelapse(cam, ts1);
        if (retcd < 0){
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("Error creating timelapse file [%s]"), cam->movie_timelapse->filename);
            free(cam->movie_timelapse);
            cam->movie_timelapse = NULL;
            return;
        }
        event(cam, EVENT_FILECREATE, NULL, cam->movie_timelapse->filename
            , (void *)FTYPE_MPEG_TIMELAPSE, ts1);
    }

    if (movie_put_image(cam->movie_timelapse, img_data, ts1) == -1) {
        MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
    }

}

static void event_movie_put(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    (void)evnt;
    (void)fname;
    (void)ftype;

    if (cam->movie_norm) {
        if (movie_put_image(cam->movie_norm, img_data, ts1) == -1){
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
        }
    }
    if (cam->movie_motion) {
        if (movie_put_image(cam->movie_motion, &cam->imgs.image_motion, ts1) == -1) {
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
        }
    }
}

static void event_movie_closefile(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    (void)evnt;
    (void)img_data;
    (void)fname;
    (void)ftype;

    if (cam->movie_norm) {
        movie_close(cam->movie_norm);
        free(cam->movie_norm);
        cam->movie_norm = NULL;
        event(cam, EVENT_FILECLOSE, NULL, cam->newfilename, (void *)FTYPE_MPEG, ts1);
    }

    if (cam->movie_motion) {
        movie_close(cam->movie_motion);
        free(cam->movie_motion);
        cam->movie_motion = NULL;
        event(cam, EVENT_FILECLOSE, NULL, cam->motionfilename, (void *)FTYPE_MPEG_MOTION, ts1);
    }

}

static void event_movie_timelapseend(struct ctx_cam *cam, motion_event evnt
            ,struct ctx_image_data *img_data, char *fname
            ,void *ftype, struct timespec *ts1) {

    (void)evnt;
    (void)img_data;
    (void)fname;
    (void)ftype;

    if (cam->movie_timelapse) {
        movie_close(cam->movie_timelapse);
        free(cam->movie_timelapse);
        cam->movie_timelapse = NULL;
        event(cam, EVENT_FILECLOSE, NULL, cam->timelapsefilename, (void *)FTYPE_MPEG_TIMELAPSE, ts1);
    }
}

struct event_handlers {
    motion_event type;
    event_handler handler;
};

struct event_handlers event_handlers[] = {
    {
    EVENT_FILECREATE,
    event_sqlnewfile
    },
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
    event_beep
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
    EVENT_FIRSTMOTION,
    event_sqlfirstmotion
    },
    {
    EVENT_FIRSTMOTION,
    on_event_start_command
    },
    {
    EVENT_ENDMOTION,
    on_event_end_command
    },
    {
    EVENT_IMAGE_DETECTED,
    event_image_detect
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
    event_vlp_putpipe
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
    EVENT_FIRSTMOTION,
    event_new_video
    },
    {
    EVENT_FIRSTMOTION,
    event_movie_newfile
    },
    {
    EVENT_IMAGE_DETECTED,
    event_movie_put
    },
    {
    EVENT_MOVIE_PUT,
    event_movie_put
    },
    {
    EVENT_ENDMOTION,
    event_movie_closefile
    },
    {
    EVENT_TIMELAPSE,
    event_movie_timelapse
    },
    {
    EVENT_TIMELAPSEEND,
    event_movie_timelapseend
    },
    {
    EVENT_FILECLOSE,
    event_sqlfileclose
    },
    {
    EVENT_FILECLOSE,
    on_movie_end_command
    },
    {
    EVENT_FIRSTMOTION,
    event_create_extpipe
    },
    {
    EVENT_IMAGE_DETECTED,
    event_extpipe_put
    },
    {
    EVENT_MOVIE_PUT,
    event_extpipe_put
    },
    {
    EVENT_ENDMOTION,
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
    {0, NULL}
};


/**
 * event
 *   defined with the following parameters:
 *      - Type as defined in event.h (EVENT_...)
 *      - The global context struct cam
 *      - img_data - A pointer to a ctx_image_data used for images
 *      - filename - A pointer to typically a string for a file path
 *      - eventdata - A void pointer that can be cast to anything. E.g. FTYPE_...
 *      - tm - A tm struct that carries a full time structure
 * The split between unsigned images and signed filenames was introduced in 3.2.2
 * as a code reading friendly solution to avoid a stream of compiler warnings in gcc 4.0.
 */
void event(struct ctx_cam *cam, motion_event evnt
           ,struct ctx_image_data *img_data, char *fname
           ,void *ftype, struct timespec *ts1) {
    int i=-1;

    while (event_handlers[++i].handler) {
        if (evnt == event_handlers[i].type)
            event_handlers[i].handler(cam, evnt, img_data, fname, ftype, ts1);
    }
}
