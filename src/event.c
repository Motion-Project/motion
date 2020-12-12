/*   This file is part of Motion.
 *
 *   Motion is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   Motion is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
    event.c

    Generalised event handling for motion

    Copyright Jeroen Vreeken, 2002
*/

#include "translate.h"
#include "motion.h"
#include "util.h"
#include "logger.h"
#include "picture.h"
#include "netcam.h"
#include "netcam_rtsp.h"
#include "ffmpeg.h"
#include "event.h"
#include "video_loopback.h"
#include "video_common.h"
#include "dbse.h"

/*
 * TODO Items:
 * Rework the snprintf uses.
 * Edit directories so they can never be null and eliminate defaults from here
 * Move the ffmpeg initialize stuff to ffmpeg module
 * eliminate #if for v4l2
 * Move stream stuff to webu_stream
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
    "EVENT_FFMPEG_PUT",
    "EVENT_LAST"
};

/**
 * exec_command
 *      Execute 'command' with 'arg' as its argument.
 *      if !arg command is started with no arguments
 *      Before we call execl we need to close all the file handles
 *      that the fork inherited from the parent in order not to pass
 *      the open handles on to the shell
 */
static void exec_command(struct context *cnt, char *command, char *filename, int filetype)
{
    char stamp[PATH_MAX];
    mystrftime(cnt, stamp, sizeof(stamp), command, &cnt->current_image->timestamp_tv, filename, filetype);

    if (!fork()) {
        int i;

        /* Detach from parent */
        setsid();

        /*
         * Close any file descriptor except console because we will
         * like to see error messages
         */
        for (i = getdtablesize() - 1; i > 2; i--) {
            close(i);
        }

        execl("/bin/sh", "sh", "-c", stamp, " &", NULL);

        /* if above function succeeds the program never reach here */
        MOTION_LOG(ALR, TYPE_EVENTS, SHOW_ERRNO
            ,_("Unable to start external command '%s'"), stamp);

        exit(1);
    }

    MOTION_LOG(DBG, TYPE_EVENTS, NO_ERRNO
        ,_("Executing external command '%s'"), stamp);
}

/*
 * Event handlers
 */

static void event_newfile(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    (void)cnt;
    (void)eventtype;
    (void)img_data;
    (void)tv1;

    MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO
        ,_("File of type %ld saved to: %s")
        ,(unsigned long)eventdata, filename);
}


static void event_beep(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    (void)eventtype;
    (void)img_data;
    (void)filename;
    (void)eventdata;
    (void)tv1;

    if (!cnt->conf.quiet) {
        printf("\a");
    }
}

/**
 * on_picture_save_command
 *      handles both on_picture_save and on_movie_start
 *      If arg = FTYPE_IMAGE_ANY on_picture_save script is executed
 *      If arg = FTYPE_MPEG_ANY on_movie_start script is executed
 *      The scripts are executed with the filename of picture or movie appended
 *      to the config parameter.
 */
static void on_picture_save_command(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{

    int filetype = (unsigned long)eventdata;

    (void)eventtype;
    (void)img_data;
    (void)tv1;

    if ((filetype & FTYPE_IMAGE_ANY) != 0 && cnt->conf.on_picture_save) {
        exec_command(cnt, cnt->conf.on_picture_save, filename, filetype);
    }

    if ((filetype & FTYPE_MPEG_ANY) != 0 && cnt->conf.on_movie_start) {
        exec_command(cnt, cnt->conf.on_movie_start, filename, filetype);
    }
}

static void on_motion_detected_command(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{

    (void)eventtype;
    (void)img_data;
    (void)filename;
    (void)eventdata;
    (void)tv1;

    if (cnt->conf.on_motion_detected) {
        exec_command(cnt, cnt->conf.on_motion_detected, NULL, 0);
    }
}

static void event_sqlfirstmotion(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{

    (void)eventtype;
    (void)img_data;
    (void)filename;
    (void)eventdata;
    (void)tv1;

    if (!(cnt->conf.database_type)) {
        return;
    } else {
        dbse_firstmotion(cnt);
    }
}

static void event_sqlnewfile(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{

    int sqltype = (unsigned long)eventdata;

    (void)eventtype;
    (void)img_data;

    /* Only log the file types we want */
    if (!(cnt->conf.database_type) || (sqltype & cnt->sql_mask) == 0) {
        return;
    } else {
        dbse_newfile(cnt, filename, sqltype, tv1);
    }

}

static void event_sqlfileclose(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{

    int sqltype = (unsigned long)eventdata;

    (void)eventtype;
    (void)img_data;

    /* Only log the file types we want */
    if (!(cnt->conf.database_type) || (sqltype & cnt->sql_mask) == 0) {
        return;
    } else {
         dbse_fileclose(cnt, filename, sqltype, tv1);
    }

}

static void on_area_command(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    (void)eventtype;
    (void)img_data;
    (void)filename;
    (void)eventdata;
    (void)tv1;

    if (cnt->conf.on_area_detected) {
        exec_command(cnt, cnt->conf.on_area_detected, NULL, 0);
    }
}

static void on_event_start_command(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    (void)eventtype;
    (void)img_data;
    (void)filename;
    (void)eventdata;
    (void)tv1;

    if (cnt->conf.on_event_start) {
        exec_command(cnt, cnt->conf.on_event_start, NULL, 0);
    }
}

static void on_event_end_command(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    (void)eventtype;
    (void)img_data;
    (void)filename;
    (void)eventdata;
    (void)tv1;

    if (cnt->conf.on_event_end) {
        exec_command(cnt, cnt->conf.on_event_end, NULL, 0);
    }
}

static void event_stream_put(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    int subsize;

    (void)eventtype;
    (void)filename;
    (void)eventdata;
    (void)tv1;


    pthread_mutex_lock(&cnt->mutex_stream);
        /* Normal stream processing */
        if (cnt->stream_norm.cnct_count > 0) {
            if (cnt->stream_norm.jpeg_data == NULL) {
                cnt->stream_norm.jpeg_data = mymalloc(cnt->imgs.size_norm);
            }
            if (img_data->image_norm != NULL) {
                cnt->stream_norm.jpeg_size = put_picture_memory(cnt
                    ,cnt->stream_norm.jpeg_data
                    ,cnt->imgs.size_norm
                    ,img_data->image_norm
                    ,cnt->conf.stream_quality
                    ,cnt->imgs.width
                    ,cnt->imgs.height);
            }
        }

        /* Substream processing */
        if (cnt->stream_sub.cnct_count > 0) {
            if (cnt->stream_sub.jpeg_data == NULL) {
                cnt->stream_sub.jpeg_data = mymalloc(cnt->imgs.size_norm);
            }
            if (img_data->image_norm != NULL) {
                /* Resulting substream image must be multiple of 8 */
                if (((cnt->imgs.width  % 16) == 0)  &&
                    ((cnt->imgs.height % 16) == 0)) {
                    subsize = ((cnt->imgs.width / 2) * (cnt->imgs.height / 2) * 3 / 2);
                    if (cnt->imgs.substream_image == NULL) {
                        cnt->imgs.substream_image = mymalloc(subsize);
                    }
                    pic_scale_img(cnt->imgs.width
                        ,cnt->imgs.height
                        ,img_data->image_norm
                        ,cnt->imgs.substream_image);
                    cnt->stream_sub.jpeg_size = put_picture_memory(cnt
                        ,cnt->stream_sub.jpeg_data
                        ,subsize
                        ,cnt->imgs.substream_image
                        ,cnt->conf.stream_quality
                        ,(cnt->imgs.width / 2)
                        ,(cnt->imgs.height / 2));
                } else {
                    /* Substream was not multiple of 8 so send full image*/
                    cnt->stream_sub.jpeg_size = put_picture_memory(cnt
                        ,cnt->stream_sub.jpeg_data
                        ,cnt->imgs.size_norm
                        ,img_data->image_norm
                        ,cnt->conf.stream_quality
                        ,cnt->imgs.width
                        ,cnt->imgs.height);
                }
            }
        }

        /* Motion stream processing */
        if (cnt->stream_motion.cnct_count > 0) {
            if (cnt->stream_motion.jpeg_data == NULL) {
                cnt->stream_motion.jpeg_data = mymalloc(cnt->imgs.size_norm);
            }
            if (cnt->imgs.img_motion.image_norm != NULL) {
                cnt->stream_motion.jpeg_size = put_picture_memory(cnt
                    ,cnt->stream_motion.jpeg_data
                    ,cnt->imgs.size_norm
                    ,cnt->imgs.img_motion.image_norm
                    ,cnt->conf.stream_quality
                    ,cnt->imgs.width
                    ,cnt->imgs.height);
            }
        }

        /* Source stream processing */
        if (cnt->stream_source.cnct_count > 0) {
            if (cnt->stream_source.jpeg_data == NULL) {
                cnt->stream_source.jpeg_data = mymalloc(cnt->imgs.size_norm);
            }
            if (cnt->imgs.image_virgin.image_norm != NULL) {
                cnt->stream_source.jpeg_size = put_picture_memory(cnt
                    ,cnt->stream_source.jpeg_data
                    ,cnt->imgs.size_norm
                    ,cnt->imgs.image_virgin.image_norm
                    ,cnt->conf.stream_quality
                    ,cnt->imgs.width
                    ,cnt->imgs.height);
            }
        }
    pthread_mutex_unlock(&cnt->mutex_stream);

}


#if defined(HAVE_V4L2) && !defined(BSD)
static void event_vlp_putpipe(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    (void)eventtype;
    (void)filename;
    (void)tv1;

    if (*(int *)eventdata >= 0) {
        if (vlp_putpipe(*(int *)eventdata, img_data->image_norm, cnt->imgs.size_norm) == -1) {
            MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                ,_("Failed to put image into video pipe"));
        }
    }
}
#endif /* defined(HAVE_V4L2) && !defined(BSD)  */

const char *imageext(struct context *cnt)
{
    if (cnt->imgs.picture_type == IMAGE_TYPE_PPM) {
        return "ppm";
    }

    if (cnt->imgs.picture_type == IMAGE_TYPE_WEBP) {
        return "webp";
    }

    return "jpg";
}

static void event_image_detect(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    char fullfilename[PATH_MAX];
    char fname[PATH_MAX];
    int  passthrough;

    (void)eventtype;
    (void)filename;
    (void)eventdata;

    if (cnt->new_img & NEWIMG_ON) {
        const char *imagepath;

        /*
         *  conf.imagepath would normally be defined but if someone deleted it by control interface
         *  it is better to revert to the default than fail
         */
        if (cnt->conf.picture_filename) {
            imagepath = cnt->conf.picture_filename;
        } else {
            imagepath = DEF_IMAGEPATH;
        }

        mystrftime(cnt, fname, sizeof(fname), imagepath, tv1, NULL, 0);
        snprintf(fullfilename, PATH_MAX, "%.*s/%.*s.%s"
            , (int)(PATH_MAX-2-strlen(fname)-strlen(imageext(cnt)))
            , cnt->conf.target_dir
            , (int)(PATH_MAX-2-strlen(cnt->conf.target_dir)-strlen(imageext(cnt)))
            , fname, imageext(cnt));

        passthrough = util_check_passthrough(cnt);
        if ((cnt->imgs.size_high > 0) && (!passthrough)) {
            put_picture(cnt, fullfilename,img_data->image_high, FTYPE_IMAGE);
        } else {
            put_picture(cnt, fullfilename,img_data->image_norm, FTYPE_IMAGE);
        }
        event(cnt, EVENT_FILECREATE, NULL, fullfilename, (void *)FTYPE_IMAGE, tv1);
    }
}

static void event_imagem_detect(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    struct config *conf = &cnt->conf;
    char fullfilenamem[PATH_MAX];
    char fname[PATH_MAX];
    char filenamem[PATH_MAX];

    (void)eventtype;
    (void)img_data;
    (void)filename;
    (void)eventdata;

    if (conf->picture_output_motion) {
        const char *imagepath;

        /*
         *  conf.picture_filename would normally be defined but if someone deleted it by control interface
         *  it is better to revert to the default than fail
         */
        if (cnt->conf.picture_filename) {
            imagepath = cnt->conf.picture_filename;
        } else {
            imagepath = DEF_IMAGEPATH;
        }

        mystrftime(cnt, fname, sizeof(fname), imagepath, tv1, NULL, 0);

        /* motion images gets same name as normal images plus an appended 'm' */
        snprintf(filenamem, PATH_MAX, "%.*sm"
            , (int)(PATH_MAX-1-strlen(fname))
            , fname);
        snprintf(fullfilenamem, PATH_MAX, "%.*s/%.*s.%s"
            , (int)(PATH_MAX-2-strlen(filenamem)-strlen(imageext(cnt)))
            , cnt->conf.target_dir
            , (int)(PATH_MAX-2-strlen(cnt->conf.target_dir)-strlen(imageext(cnt)))
            , filenamem, imageext(cnt));
        put_picture(cnt, fullfilenamem, cnt->imgs.img_motion.image_norm, FTYPE_IMAGE_MOTION);
        event(cnt, EVENT_FILECREATE, NULL, fullfilenamem, (void *)FTYPE_IMAGE, tv1);
    }
}

static void event_image_snapshot(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    char fullfilename[PATH_MAX];
    char fname[PATH_MAX];
    char filepath[PATH_MAX];
    int offset = 0;
    int len = strlen(cnt->conf.snapshot_filename);
    int passthrough;

    (void)eventtype;
    (void)filename;
    (void)eventdata;

    if (len >= 9) {
        offset = len - 8;
    }

    if (mystrne(cnt->conf.snapshot_filename+offset, "lastsnap")) {
        char linkpath[PATH_MAX];
        const char *snappath;
        /*
         *  conf.snapshot_filename would normally be defined but if someone deleted it by control interface
         *  it is better to revert to the default than fail
         */
        if (cnt->conf.snapshot_filename) {
            snappath = cnt->conf.snapshot_filename;
        } else {
            snappath = DEF_SNAPPATH;
        }

        mystrftime(cnt, filepath, sizeof(filepath), snappath, tv1, NULL, 0);
        snprintf(fname, PATH_MAX, "%.*s.%s"
            , (int)(PATH_MAX-1-strlen(filepath)-strlen(imageext(cnt)))
            , filepath, imageext(cnt));
        snprintf(fullfilename, PATH_MAX, "%.*s/%.*s"
            , (int)(PATH_MAX-1-strlen(fname))
            , cnt->conf.target_dir
            , (int)(PATH_MAX-1-strlen(cnt->conf.target_dir))
            , fname);

        passthrough = util_check_passthrough(cnt);
        if ((cnt->imgs.size_high > 0) && (!passthrough)) {
            put_picture(cnt, fullfilename, img_data->image_high, FTYPE_IMAGE_SNAPSHOT);
        } else {
            put_picture(cnt, fullfilename, img_data->image_norm, FTYPE_IMAGE_SNAPSHOT);
        }
        event(cnt, EVENT_FILECREATE, NULL, fullfilename, (void *)FTYPE_IMAGE_SNAPSHOT, tv1);

        /*
         *  Update symbolic link *after* image has been written so that
         *  the link always points to a valid file.
         */
        snprintf(linkpath, PATH_MAX, "%.*s/lastsnap.%s"
            , (int)(PATH_MAX-strlen("/lastsnap.")-strlen(imageext(cnt)))
            , cnt->conf.target_dir, imageext(cnt));

        remove(linkpath);

        if (symlink(fname, linkpath)) {
            MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                ,_("Could not create symbolic link [%s]"), fname);
            return;
        }
    } else {
        mystrftime(cnt, filepath, sizeof(filepath), cnt->conf.snapshot_filename, tv1, NULL, 0);
        snprintf(fname, PATH_MAX, "%.*s.%s"
            , (int)(PATH_MAX-1-strlen(imageext(cnt)))
            , filepath, imageext(cnt));
        snprintf(fullfilename, PATH_MAX, "%.*s/%.*s"
            , (int)(PATH_MAX-1-strlen(fname))
            , cnt->conf.target_dir
            , (int)(PATH_MAX-1-strlen(cnt->conf.target_dir))
            , fname);
        remove(fullfilename);

        passthrough = util_check_passthrough(cnt);
        if ((cnt->imgs.size_high > 0) && (!passthrough)) {
            put_picture(cnt, fullfilename, img_data->image_high, FTYPE_IMAGE_SNAPSHOT);
        } else {
            put_picture(cnt, fullfilename, img_data->image_norm, FTYPE_IMAGE_SNAPSHOT);
        }

        event(cnt, EVENT_FILECREATE, NULL, fullfilename, (void *)FTYPE_IMAGE_SNAPSHOT, tv1);
    }

    cnt->snapshot = 0;
}

/**
 * event_image_preview
 *      event_image_preview
 *
 * Returns nothing.
 */
static void event_image_preview(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    int use_imagepath;
    const char *imagepath;
    char previewname[PATH_MAX];
    char fname[PATH_MAX];
    struct image_data *saved_current_image;
    int passthrough, retcd;

    (void)eventtype;
    (void)img_data;
    (void)filename;
    (void)eventdata;

    if (cnt->imgs.preview_image.diffs) {
        saved_current_image = cnt->current_image;
        cnt->current_image = &cnt->imgs.preview_image;

        /* Use filename of movie i.o. jpeg_filename when set to 'preview'. */
        use_imagepath = strcmp(cnt->conf.picture_filename, "preview");

        if ((cnt->ffmpeg_output || (cnt->conf.movie_extpipe_use && cnt->extpipe)) && !use_imagepath) {

            if (cnt->conf.movie_extpipe_use && cnt->extpipe) {
                retcd = snprintf(previewname, PATH_MAX,"%s.%s"
                    , cnt->extpipefilename, imageext(cnt));
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
                retcd = snprintf(fname, strlen(cnt->newfilename) - 2
                    ,"%s", cnt->newfilename);
                if (retcd < 0) {
                    MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                        ,_("Error creating file name base %d %s")
                        ,retcd, fname);
                    return;
                }
                retcd = snprintf(previewname, PATH_MAX
                    ,"%s%s", fname, imageext(cnt));
                if ((retcd < 0) || (retcd >= PATH_MAX)) {
                    MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                        ,_("Error creating preview name %d %s")
                        , retcd, previewname);
                    return;
                }
            }

            passthrough = util_check_passthrough(cnt);
            if ((cnt->imgs.size_high > 0) && (!passthrough)) {
                put_picture(cnt, previewname, cnt->imgs.preview_image.image_high , FTYPE_IMAGE);
            } else {
                put_picture(cnt, previewname, cnt->imgs.preview_image.image_norm , FTYPE_IMAGE);
            }
            event(cnt, EVENT_FILECREATE, NULL, previewname, (void *)FTYPE_IMAGE, tv1);
        } else {
            /*
             * Save best preview-shot also when no movies are recorded or imagepath
             * is used. Filename has to be generated - nothing available to reuse!
             */

            /*
             * conf.picture_filename would normally be defined but if someone deleted it by
             * control interface it is better to revert to the default than fail.
             */
            if (cnt->conf.picture_filename) {
                imagepath = cnt->conf.picture_filename;
            } else {
                imagepath = (char *)DEF_IMAGEPATH;
            }

            mystrftime(cnt, fname, sizeof(fname), imagepath, &cnt->imgs.preview_image.timestamp_tv, NULL, 0);
            snprintf(previewname, PATH_MAX, "%.*s/%.*s.%s"
                , (int)(PATH_MAX-2-strlen(fname)-strlen(imageext(cnt)))
                , cnt->conf.target_dir
                , (int)(PATH_MAX-2-strlen(cnt->conf.target_dir)-strlen(imageext(cnt)))
                , fname, imageext(cnt));

            passthrough = util_check_passthrough(cnt);
            if ((cnt->imgs.size_high > 0) && (!passthrough)) {
                put_picture(cnt, previewname, cnt->imgs.preview_image.image_high , FTYPE_IMAGE);
            } else {
                put_picture(cnt, previewname, cnt->imgs.preview_image.image_norm, FTYPE_IMAGE);
            }
            event(cnt, EVENT_FILECREATE, NULL, previewname, (void *)FTYPE_IMAGE, tv1);
        }

        /* Restore global context values. */
        cnt->current_image = saved_current_image;
    }
}


static void event_camera_lost(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    (void)eventtype;
    (void)img_data;
    (void)filename;
    (void)eventdata;
    (void)tv1;

    if (cnt->conf.on_camera_lost) {
        exec_command(cnt, cnt->conf.on_camera_lost, NULL, 0);
    }
}

static void event_camera_found(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    (void)eventtype;
    (void)img_data;
    (void)filename;
    (void)eventdata;
    (void)tv1;

    if (cnt->conf.on_camera_found) {
        exec_command(cnt, cnt->conf.on_camera_found, NULL, 0);
    }
}

static void on_movie_end_command(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    int filetype = (unsigned long) eventdata;

    (void)eventtype;
    (void)img_data;
    (void)tv1;

    if ((filetype & FTYPE_MPEG_ANY) && cnt->conf.on_movie_end) {
        exec_command(cnt, cnt->conf.on_movie_end, filename, filetype);
    }
}

static void event_extpipe_end(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    (void)eventtype;
    (void)img_data;
    (void)filename;
    (void)eventdata;

    if (cnt->extpipe_open) {
        cnt->extpipe_open = 0;
        fflush(cnt->extpipe);
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO
            ,_("CLOSING: extpipe file desc %d, error state %d")
            ,fileno(cnt->extpipe), ferror(cnt->extpipe));
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("pclose return: %d"),
                   pclose(cnt->extpipe));
        event(cnt, EVENT_FILECLOSE, NULL, cnt->extpipefilename, (void *)FTYPE_MPEG, tv1);
    }
}

static void event_create_extpipe(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    int retcd;

    (void)eventtype;
    (void)img_data;
    (void)filename;
    (void)eventdata;

    if ((cnt->conf.movie_extpipe_use) && (cnt->conf.movie_extpipe)) {
        char stamp[PATH_MAX] = "";
        const char *moviepath;

        /*
         *  conf.mpegpath would normally be defined but if someone deleted it by control interface
         *  it is better to revert to the default than fail
         */
        if (cnt->conf.movie_filename) {
            moviepath = cnt->conf.movie_filename;
        } else {
            moviepath = DEF_MOVIEPATH;
            MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("moviepath: %s"), moviepath);
        }

        mystrftime(cnt, stamp, sizeof(stamp), moviepath, tv1, NULL, 0);
        snprintf(cnt->extpipefilename, PATH_MAX - 4, "%.*s/%.*s"
            , (int)(PATH_MAX-5-strlen(stamp))
            , cnt->conf.target_dir
            , (int)(PATH_MAX-5-strlen(cnt->conf.target_dir))
            , stamp);

        if (access(cnt->conf.target_dir, W_OK)!= 0) {
            /* Permission denied */
            if (errno ==  EACCES) {
                MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                    ,_("no write access to target directory %s"), cnt->conf.target_dir);
                return ;
            /* Path not found - create it */
            } else if (errno ==  ENOENT) {
                MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                    ,_("path not found, trying to create it %s ..."), cnt->conf.target_dir);
                if (mycreate_path(cnt->extpipefilename) == -1) {
                    return;
                }
            } else {
                MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                    ,_("error accesing path %s"), cnt->conf.target_dir);
                return ;
            }
        }

        /* Always create any path specified as file name */
        if (mycreate_path(cnt->extpipefilename) == -1) {
            return;
        }

        mystrftime(cnt, stamp, sizeof(stamp), cnt->conf.movie_extpipe, tv1, cnt->extpipefilename, 0);

        retcd = snprintf(cnt->extpipecmdline, PATH_MAX, "%s", stamp);
        if ((retcd < 0 ) || (retcd >= PATH_MAX)) {
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                , _("Error specifying command line: %s"), cnt->extpipecmdline);
            return;
        }
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("pipe: %s"), cnt->extpipecmdline);

        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("cnt->moviefps: %d"), cnt->movie_fps);

        event(cnt, EVENT_FILECREATE, NULL, cnt->extpipefilename, (void *)FTYPE_MPEG, tv1);
        cnt->extpipe = popen(cnt->extpipecmdline, "we");

        if (cnt->extpipe == NULL) {
            MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, _("popen failed"));
            return;
        }

        setbuf(cnt->extpipe, NULL);
        cnt->extpipe_open = 1;
    }
}

static void event_extpipe_put(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    int passthrough;

    (void)eventtype;
    (void)filename;
    (void)eventdata;
    (void)tv1;

    /* Check use_extpipe enabled and ext_pipe not NULL */
    if ((cnt->conf.movie_extpipe_use) && (cnt->extpipe != NULL)) {
        MOTION_LOG(DBG, TYPE_EVENTS, NO_ERRNO, _("Using extpipe"));
        passthrough = util_check_passthrough(cnt);
        /* Check that is open */
        if ((cnt->extpipe_open) && (fileno(cnt->extpipe) > 0)) {
            if ((cnt->imgs.size_high > 0) && (!passthrough)) {
                if (!fwrite(img_data->image_high, cnt->imgs.size_high, 1, cnt->extpipe)) {
                    MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                        ,_("Error writing in pipe , state error %d"), ferror(cnt->extpipe));
                }
            } else {
                if (!fwrite(img_data->image_norm, cnt->imgs.size_norm, 1, cnt->extpipe)) {
                    MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                        ,_("Error writing in pipe , state error %d"), ferror(cnt->extpipe));
                }
           }
        } else {
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("pipe %s not created or closed already "), cnt->extpipecmdline);
        }
    }
}


static void event_new_video(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    (void)eventtype;
    (void)img_data;
    (void)filename;
    (void)eventdata;
    (void)tv1;

    cnt->movie_last_shot = -1;

    cnt->movie_fps = cnt->lastrate;

    MOTION_LOG(INF, TYPE_EVENTS, NO_ERRNO, _("Source FPS %d"), cnt->movie_fps);

    if (cnt->movie_fps < 2) {
        cnt->movie_fps = 2;
    }

}


static void event_ffmpeg_newfile(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    char stamp[PATH_MAX];
    const char *moviepath;
    const char *codec;
    long codenbr;
    int retcd;

    (void)eventtype;
    (void)img_data;
    (void)filename;
    (void)eventdata;

    if (!cnt->conf.movie_output && !cnt->conf.movie_output_motion) {
        return;
    }

    /*
     *  conf.mpegpath would normally be defined but if someone deleted it by control interface
     *  it is better to revert to the default than fail
     */
    if (cnt->conf.movie_filename) {
        moviepath = cnt->conf.movie_filename;
    } else {
        moviepath = DEF_MOVIEPATH;
    }

    mystrftime(cnt, stamp, sizeof(stamp), moviepath, tv1, NULL, 0);

    /*
     *  motion movies get the same name as normal movies plus an appended 'm'
     *  PATH_MAX - 4 to allow for .mpg to be appended without overflow
     */

     /* The following section allows for testing of all the various containers
      * that Motion permits. The container type is pre-pended to the name of the
      * file so that we can determine which container type created what movie.
      * The intent for this is be used for developer testing when the ffmpeg libs
      * change or the code inside our ffmpeg module changes.  For each event, the
      * container type will change.  This way, you can turn on emulate motion, then
      * specify a maximum movie time and let Motion run for days creating all the
      * different types of movies checking for crashes, warnings, etc.
     */
    codec = cnt->conf.movie_codec;
    if (mystreq(codec, "ogg")) {
        MOTION_LOG(WRN, TYPE_ENCODER, NO_ERRNO, _("The ogg container is no longer supported.  Changing to mpeg4"));
        codec = "mpeg4";
    }
    if (mystreq(codec, "test")) {
        MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO, _("Running test of the various output formats."));
        codenbr = cnt->event_nr % 10;
        switch (codenbr) {
        case 1:
            codec = "mpeg4";
            break;
        case 2:
            codec = "msmpeg4";
            break;
        case 3:
            codec = "swf";
            break;
        case 4:
            codec = "flv";
            break;
        case 5:
            codec = "ffv1";
            break;
        case 6:
            codec = "mov";
            break;
        case 7:
            codec = "mp4";
            break;
        case 8:
            codec = "mkv";
            break;
        case 9:
            codec = "hevc";
            break;
        default:
            codec = "msmpeg4";
            break;
        }
        snprintf(cnt->motionfilename, PATH_MAX - 4, "%.*s/%s_%.*sm"
            , (int)(PATH_MAX-7-strlen(stamp)-strlen(codec))
            , cnt->conf.target_dir, codec
            , (int)(PATH_MAX-7-strlen(cnt->conf.target_dir)-strlen(codec))
            , stamp);
        snprintf(cnt->newfilename, PATH_MAX - 4, "%.*s/%s_%.*s"
            , (int)(PATH_MAX-6-strlen(stamp)-strlen(codec))
            , cnt->conf.target_dir, codec
            , (int)(PATH_MAX-6-strlen(cnt->conf.target_dir)-strlen(codec))
            , stamp);
    } else {
        snprintf(cnt->motionfilename, PATH_MAX - 4, "%.*s/%.*sm"
            , (int)(PATH_MAX-6-strlen(stamp))
            , cnt->conf.target_dir
            , (int)(PATH_MAX-6-strlen(cnt->conf.target_dir))
            , stamp);
        snprintf(cnt->newfilename, PATH_MAX - 4, "%.*s/%.*s"
            , (int)(PATH_MAX-5-strlen(stamp))
            , cnt->conf.target_dir
            , (int)(PATH_MAX-5-strlen(cnt->conf.target_dir))
            , stamp);
    }
    if (cnt->conf.movie_output) {
        cnt->ffmpeg_output = mymalloc(sizeof(struct ffmpeg));
        if (cnt->imgs.size_high > 0) {
            cnt->ffmpeg_output->width  = cnt->imgs.width_high;
            cnt->ffmpeg_output->height = cnt->imgs.height_high;
            cnt->ffmpeg_output->high_resolution = TRUE;
            cnt->ffmpeg_output->rtsp_data = cnt->rtsp_high;
        } else {
            cnt->ffmpeg_output->width  = cnt->imgs.width;
            cnt->ffmpeg_output->height = cnt->imgs.height;
            cnt->ffmpeg_output->high_resolution = FALSE;
            cnt->ffmpeg_output->rtsp_data = cnt->rtsp;
        }
        cnt->ffmpeg_output->tlapse = TIMELAPSE_NONE;
        cnt->ffmpeg_output->fps = cnt->movie_fps;
        cnt->ffmpeg_output->bps = cnt->conf.movie_bps;
        cnt->ffmpeg_output->filename = cnt->newfilename;
        cnt->ffmpeg_output->quality = cnt->conf.movie_quality;
        cnt->ffmpeg_output->start_time.tv_sec = tv1->tv_sec;
        cnt->ffmpeg_output->start_time.tv_usec = tv1->tv_usec;
        cnt->ffmpeg_output->last_pts = -1;
        cnt->ffmpeg_output->base_pts = 0;
        cnt->ffmpeg_output->gop_cnt = 0;
        cnt->ffmpeg_output->codec_name = codec;
        if (mystreq(cnt->conf.movie_codec, "test")) {
            cnt->ffmpeg_output->test_mode = 1;
        } else {
            cnt->ffmpeg_output->test_mode = 0;
        }
        cnt->ffmpeg_output->motion_images = 0;
        cnt->ffmpeg_output->passthrough =util_check_passthrough(cnt);


        retcd = ffmpeg_open(cnt->ffmpeg_output);
        if (retcd < 0) {
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("Error opening context for movie output."));
            free(cnt->ffmpeg_output);
            cnt->ffmpeg_output=NULL;
            return;
        }
        event(cnt, EVENT_FILECREATE, NULL, cnt->newfilename, (void *)FTYPE_MPEG, tv1);
    }

    if (cnt->conf.movie_output_motion) {
        cnt->ffmpeg_output_motion = mymalloc(sizeof(struct ffmpeg));
        cnt->ffmpeg_output_motion->width  = cnt->imgs.width;
        cnt->ffmpeg_output_motion->height = cnt->imgs.height;
        cnt->ffmpeg_output_motion->rtsp_data = NULL;
        cnt->ffmpeg_output_motion->tlapse = TIMELAPSE_NONE;
        cnt->ffmpeg_output_motion->fps = cnt->movie_fps;
        cnt->ffmpeg_output_motion->bps = cnt->conf.movie_bps;
        cnt->ffmpeg_output_motion->filename = cnt->motionfilename;
        cnt->ffmpeg_output_motion->quality = cnt->conf.movie_quality;
        cnt->ffmpeg_output_motion->start_time.tv_sec = tv1->tv_sec;
        cnt->ffmpeg_output_motion->start_time.tv_usec = tv1->tv_usec;
        cnt->ffmpeg_output_motion->last_pts = -1;
        cnt->ffmpeg_output_motion->base_pts = 0;
        cnt->ffmpeg_output_motion->gop_cnt = 0;
        cnt->ffmpeg_output_motion->codec_name = codec;
        if (mystreq(cnt->conf.movie_codec, "test")) {
            cnt->ffmpeg_output_motion->test_mode = TRUE;
        } else {
            cnt->ffmpeg_output_motion->test_mode = FALSE;
        }
        cnt->ffmpeg_output_motion->motion_images = TRUE;
        cnt->ffmpeg_output_motion->passthrough = FALSE;
        cnt->ffmpeg_output_motion->high_resolution = FALSE;
        cnt->ffmpeg_output_motion->rtsp_data = NULL;

        retcd = ffmpeg_open(cnt->ffmpeg_output_motion);
        if (retcd < 0) {
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("ffopen_open error creating (motion) file [%s]"), cnt->motionfilename);
            free(cnt->ffmpeg_output_motion);
            cnt->ffmpeg_output_motion = NULL;
            return;
        }
    }
}

static void event_ffmpeg_timelapse(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    int retcd;
    int passthrough;

    (void)eventtype;
    (void)filename;
    (void)eventdata;

    if (!cnt->ffmpeg_timelapse) {
        char tmp[PATH_MAX];
        const char *timepath;
        const char *codec_mpg = "mpg";
        const char *codec_mpeg = "mpeg4";

        /*
         *  conf.timelapse_filename would normally be defined but if someone deleted it by control interface
         *  it is better to revert to the default than fail
         */
        if (cnt->conf.timelapse_filename) {
            timepath = cnt->conf.timelapse_filename;
        } else {
            timepath = DEF_TIMEPATH;
        }

        mystrftime(cnt, tmp, sizeof(tmp), timepath, tv1, NULL, 0);

        /* PATH_MAX - 4 to allow for .mpg to be appended without overflow */
        snprintf(cnt->timelapsefilename, PATH_MAX - 4, "%.*s/%.*s"
            , (int)(PATH_MAX-5-strlen(tmp))
            , cnt->conf.target_dir
            , (int)(PATH_MAX-5-strlen(cnt->conf.target_dir))
            , tmp);
        passthrough = util_check_passthrough(cnt);
        cnt->ffmpeg_timelapse = mymalloc(sizeof(struct ffmpeg));
        if ((cnt->imgs.size_high > 0) && (!passthrough)) {
            cnt->ffmpeg_timelapse->width  = cnt->imgs.width_high;
            cnt->ffmpeg_timelapse->height = cnt->imgs.height_high;
            cnt->ffmpeg_timelapse->high_resolution = TRUE;
        } else {
            cnt->ffmpeg_timelapse->width  = cnt->imgs.width;
            cnt->ffmpeg_timelapse->height = cnt->imgs.height;
            cnt->ffmpeg_timelapse->high_resolution = FALSE;
        }
        cnt->ffmpeg_timelapse->fps = cnt->conf.timelapse_fps;
        cnt->ffmpeg_timelapse->bps = cnt->conf.movie_bps;
        cnt->ffmpeg_timelapse->filename = cnt->timelapsefilename;
        cnt->ffmpeg_timelapse->quality = cnt->conf.movie_quality;
        cnt->ffmpeg_timelapse->start_time.tv_sec = tv1->tv_sec;
        cnt->ffmpeg_timelapse->start_time.tv_usec = tv1->tv_usec;
        cnt->ffmpeg_timelapse->last_pts = -1;
        cnt->ffmpeg_timelapse->base_pts = 0;
        cnt->ffmpeg_timelapse->test_mode = FALSE;
        cnt->ffmpeg_timelapse->gop_cnt = 0;
        cnt->ffmpeg_timelapse->motion_images = FALSE;
        cnt->ffmpeg_timelapse->passthrough = FALSE;
        cnt->ffmpeg_timelapse->rtsp_data = NULL;

        if ((mystreq(cnt->conf.timelapse_codec,"mpg")) ||
            (mystreq(cnt->conf.timelapse_codec,"swf"))) {
            if (mystreq(cnt->conf.timelapse_codec,"swf")) {
                MOTION_LOG(WRN, TYPE_EVENTS, NO_ERRNO
                    ,_("The swf container for timelapse no longer supported.  Using mpg container."));
            }

            MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("Timelapse using mpg codec."));
            MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("Events will be appended to file"));

            cnt->ffmpeg_timelapse->tlapse = TIMELAPSE_APPEND;
            cnt->ffmpeg_timelapse->codec_name = codec_mpg;
            retcd = ffmpeg_open(cnt->ffmpeg_timelapse);
        } else {
            MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("Timelapse using mpeg4 codec."));
            MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("Events will be trigger new files"));

            cnt->ffmpeg_timelapse->tlapse = TIMELAPSE_NEW;
            cnt->ffmpeg_timelapse->codec_name = codec_mpeg;
            retcd = ffmpeg_open(cnt->ffmpeg_timelapse);
        }

        if (retcd < 0) {
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO
                ,_("ffopen_open error creating (timelapse) file [%s]"), cnt->timelapsefilename);
            free(cnt->ffmpeg_timelapse);
            cnt->ffmpeg_timelapse = NULL;
            return;
        }
        event(cnt, EVENT_FILECREATE, NULL, cnt->timelapsefilename, (void *)FTYPE_MPEG_TIMELAPSE, tv1);
    }

    if (ffmpeg_put_image(cnt->ffmpeg_timelapse, img_data, tv1) == -1) {
        MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
    }

}

static void event_ffmpeg_put(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    (void)eventtype;
    (void)filename;
    (void)eventdata;

    if (cnt->ffmpeg_output) {
        if (ffmpeg_put_image(cnt->ffmpeg_output, img_data, tv1) == -1) {
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
        }
    }
    if (cnt->ffmpeg_output_motion) {
        if (ffmpeg_put_image(cnt->ffmpeg_output_motion, &cnt->imgs.img_motion, tv1) == -1) {
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, _("Error encoding image"));
        }
    }
}

static void event_ffmpeg_closefile(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    (void)eventtype;
    (void)img_data;
    (void)filename;
    (void)eventdata;

    if (cnt->ffmpeg_output) {
        ffmpeg_close(cnt->ffmpeg_output);
        free(cnt->ffmpeg_output);
        cnt->ffmpeg_output = NULL;
        event(cnt, EVENT_FILECLOSE, NULL, cnt->newfilename, (void *)FTYPE_MPEG, tv1);
    }

    if (cnt->ffmpeg_output_motion) {
        ffmpeg_close(cnt->ffmpeg_output_motion);
        free(cnt->ffmpeg_output_motion);
        cnt->ffmpeg_output_motion = NULL;
        event(cnt, EVENT_FILECLOSE, NULL, cnt->motionfilename, (void *)FTYPE_MPEG_MOTION, tv1);
    }

}

static void event_ffmpeg_timelapseend(struct context *cnt, motion_event eventtype
            , struct image_data *img_data, char *filename, void *eventdata, struct timeval *tv1)
{
    (void)eventtype;
    (void)img_data;
    (void)filename;
    (void)eventdata;

    if (cnt->ffmpeg_timelapse) {
        ffmpeg_close(cnt->ffmpeg_timelapse);
        free(cnt->ffmpeg_timelapse);
        cnt->ffmpeg_timelapse = NULL;
        event(cnt, EVENT_FILECLOSE, NULL, cnt->timelapsefilename, (void *)FTYPE_MPEG_TIMELAPSE, tv1);
    }
}



/*
 * Starting point for all events
 */

struct event_handlers {
    motion_event eventtype;
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
    #if defined(HAVE_V4L2) && !defined(BSD)
        {
        EVENT_IMAGE,
        event_vlp_putpipe
        },
        {
        EVENT_IMAGEM,
        event_vlp_putpipe
        },
    #endif /* defined(HAVE_V4L2) && !defined(BSD) */
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
    event_ffmpeg_newfile
    },
    {
    EVENT_IMAGE_DETECTED,
    event_ffmpeg_put
    },
    {
    EVENT_FFMPEG_PUT,
    event_ffmpeg_put
    },
    {
    EVENT_ENDMOTION,
    event_ffmpeg_closefile
    },
    {
    EVENT_TIMELAPSE,
    event_ffmpeg_timelapse
    },
    {
    EVENT_TIMELAPSEEND,
    event_ffmpeg_timelapseend
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
    EVENT_FFMPEG_PUT,
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
 *      - The global context struct cnt
 *      - img_data - A pointer to a image_data context used for images
 *      - filename - A pointer to typically a string for a file path
 *      - eventdata - A void pointer that can be cast to anything. E.g. FTYPE_...
 *      - tm - A tm struct that carries a full time structure
 * The split between unsigned images and signed filenames was introduced in 3.2.2
 * as a code reading friendly solution to avoid a stream of compiler warnings in gcc 4.0.
 */
void event(struct context *cnt, motion_event eventtype, struct image_data *img_data,
           char *filename, void *eventdata, struct timeval *tv1)
{
    int i=-1;

    while (event_handlers[++i].handler) {
        if (eventtype == event_handlers[i].eventtype) {
            event_handlers[i].handler(cnt, eventtype, img_data, filename, eventdata, tv1);
        }
    }
}
