/*
    event.c

    Generalised event handling for motion

    Copyright Jeroen Vreeken, 2002
    This software is distributed under the GNU Public License Version 2
    see also the file 'COPYING'.
*/

#include "ffmpeg.h"    /* must be first to avoid 'shadow' warning */
#include "picture.h"   /* already includes motion.h */
#include "event.h"
#if (!defined(BSD))
#include "video.h"
#endif

/* Various functions (most doing the actual action) */

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
    "EVENT_FILECLOSE",
    "EVENT_DEBUG",
    "EVENT_CRITICAL",
    "EVENT_AREA_DETECTED",
    "EVENT_CAMERA_LOST",
    "EVENT_FFMPEG_PUT",
    "EVENT_SDL_PUT",
    "EVENT_LAST"
};

/**
 * eventToString
 *
 * returns string label of the event
 */
static const char *eventToString(motion_event e)
{
    return eventList[(int)e];
}

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
    mystrftime(cnt, stamp, sizeof(stamp), command, &cnt->current_image->timestamp_tm, filename, filetype);

    if (!fork()) {
        int i;

        /* Detach from parent */
        setsid();

        /*
         * Close any file descriptor except console because we will
         * like to see error messages
         */
        for (i = getdtablesize(); i > 2; --i)
            close(i);

        execl("/bin/sh", "sh", "-c", stamp, " &", NULL);

        /* if above function succeeds the program never reach here */
        MOTION_LOG(ALR, TYPE_EVENTS, SHOW_ERRNO, "%s: Unable to start external command '%s'",
                   stamp);

        exit(1);
    }

    MOTION_LOG(DBG, TYPE_EVENTS, NO_ERRNO, "%s: Executing external command '%s'",
               stamp);
}

/*
 * Event handlers
 */

static void event_newfile(struct context *cnt ATTRIBUTE_UNUSED,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *dummy ATTRIBUTE_UNUSED, char *filename, void *ftype,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "%s: File of type %ld saved to: %s",
               (unsigned long)ftype, filename);
}


static void event_beep(struct context *cnt, motion_event type ATTRIBUTE_UNUSED,
            unsigned char *dummy ATTRIBUTE_UNUSED,
            char *filename ATTRIBUTE_UNUSED,
            void *ftype ATTRIBUTE_UNUSED,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    if (!cnt->conf.quiet)
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
static void on_picture_save_command(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *dummy ATTRIBUTE_UNUSED,
            char *filename, void *arg, struct tm *tm ATTRIBUTE_UNUSED)
{
    int filetype = (unsigned long)arg;

    if ((filetype & FTYPE_IMAGE_ANY) != 0 && cnt->conf.on_picture_save)
        exec_command(cnt, cnt->conf.on_picture_save, filename, filetype);

    if ((filetype & FTYPE_MPEG_ANY) != 0 && cnt->conf.on_movie_start)
        exec_command(cnt, cnt->conf.on_movie_start, filename, filetype);
}

static void on_motion_detected_command(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    if (cnt->conf.on_motion_detected)
        exec_command(cnt, cnt->conf.on_motion_detected, NULL, 0);
}

#if defined(HAVE_MYSQL) || defined(HAVE_PGSQL) || defined(HAVE_SQLITE3)

static void event_sqlnewfile(struct context *cnt,
            motion_event type  ATTRIBUTE_UNUSED,
            unsigned char *dummy ATTRIBUTE_UNUSED,
            char *filename, void *arg, struct tm *tm ATTRIBUTE_UNUSED)
{
    int sqltype = (unsigned long)arg;

    /* Only log the file types we want */
    if (!(cnt->conf.database_type) || (sqltype & cnt->sql_mask) == 0)
        return;

    /*
     * We place the code in a block so we only spend time making space in memory
     * for the sqlquery and timestr when we actually need it.
     */
    {
        char sqlquery[PATH_MAX];

        mystrftime(cnt, sqlquery, sizeof(sqlquery), cnt->conf.sql_query,
                   &cnt->current_image->timestamp_tm, filename, sqltype);

#ifdef HAVE_MYSQL
        if (!strcmp(cnt->conf.database_type, "mysql")) {
            if (mysql_query(cnt->database, sqlquery) != 0) {
                int error_code = mysql_errno(cnt->database);

                MOTION_LOG(ERR, TYPE_DB, SHOW_ERRNO, "%s: Mysql query failed %s error code %d",
                           mysql_error(cnt->database), error_code);
                /* Try to reconnect ONCE if fails continue and discard this sql query */
                if (error_code >= 2000) {
                    // Close connection before start a new connection
                    mysql_close(cnt->database);

                    cnt->database = mymalloc(sizeof(MYSQL));
                    mysql_init(cnt->database);

                    if (!mysql_real_connect(cnt->database, cnt->conf.database_host,
                                            cnt->conf.database_user, cnt->conf.database_password,
                                            cnt->conf.database_dbname, 0, NULL, 0)) {
                        MOTION_LOG(ALR, TYPE_DB, NO_ERRNO, "%s: Cannot reconnect to MySQL"
                                   " database %s on host %s with user %s MySQL error was %s",
                                   cnt->conf.database_dbname,
                                   cnt->conf.database_host, cnt->conf.database_user,
                                   mysql_error(cnt->database));
                    } else {
                        MOTION_LOG(INF, TYPE_DB, NO_ERRNO, "%s: Re-Connection to Mysql database '%s' Succeed",
                                   cnt->conf.database_dbname);
                        if (mysql_query(cnt->database, sqlquery) != 0) {
                            int error_my = mysql_errno(cnt->database);
                            MOTION_LOG(ERR, TYPE_DB, SHOW_ERRNO, "%s: after re-connection Mysql query failed %s error code %d",
                                       mysql_error(cnt->database), error_my);
                        }
                    }
                }
            }
        }
#endif /* HAVE_MYSQL */

#ifdef HAVE_PGSQL
        if (!strcmp(cnt->conf.database_type, "postgresql")) {
            PGresult *res;

            res = PQexec(cnt->database_pg, sqlquery);

            if (PQstatus(cnt->database_pg) == CONNECTION_BAD) {

                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO, "%s: Connection to PostgreSQL database '%s' failed: %s",
                           cnt->conf.database_dbname, PQerrorMessage(cnt->database_pg));

                // This function will close the connection to the server and attempt to reestablish a new connection to the same server,
                // using all the same parameters previously used. This may be useful for error recovery if a working connection is lost
                PQreset(cnt->database_pg);

                if (PQstatus(cnt->database_pg) == CONNECTION_BAD) {
                    MOTION_LOG(ERR, TYPE_DB, NO_ERRNO, "%s: Re-Connection to PostgreSQL database '%s' failed: %s",
                               cnt->conf.database_dbname, PQerrorMessage(cnt->database_pg));
                } else {
                    MOTION_LOG(INF, TYPE_DB, NO_ERRNO, "%s: Re-Connection to PostgreSQL database '%s' Succeed",
                               cnt->conf.database_dbname);
                }

            } else if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                MOTION_LOG(ERR, TYPE_DB, SHOW_ERRNO, "%s: PGSQL query [%s] failed", sqlquery);
                PQclear(res);
            }
        }
#endif /* HAVE_PGSQL */

#ifdef HAVE_SQLITE3
        if ((!strcmp(cnt->conf.database_type, "sqlite3")) && (cnt->conf.sqlite3_db)) {
            int res;
            char *errmsg = 0;
            res = sqlite3_exec(cnt->database_sqlite3, sqlquery, NULL, 0, &errmsg);
            if (res != SQLITE_OK ) {
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO, "%s: SQLite error was %s",
                           errmsg);
                sqlite3_free(errmsg);
            }
        }
#endif /* HAVE_SQLITE3 */
    }
}

#endif /* defined HAVE_MYSQL || defined HAVE_PGSQL || defined(HAVE_SQLITE3) */

static void on_area_command(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    if (cnt->conf.on_area_detected)
        exec_command(cnt, cnt->conf.on_area_detected, NULL, 0);
}

static void on_event_start_command(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    if (cnt->conf.on_event_start)
        exec_command(cnt, cnt->conf.on_event_start, NULL, 0);
}

static void on_event_end_command(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    if (cnt->conf.on_event_end)
        exec_command(cnt, cnt->conf.on_event_end, NULL, 0);
}

static void event_stop_stream(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    if ((cnt->conf.stream_port) && (cnt->stream.socket != -1))
        stream_stop(cnt);
}

static void event_stream_put(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *img, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *tm ATTRIBUTE_UNUSED)
{
    if (cnt->conf.stream_port)
        stream_put(cnt, img);
}

#ifdef HAVE_SDL
static void event_sdl_put(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *img, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *tm ATTRIBUTE_UNUSED)
{
    sdl_put(img, cnt->imgs.width, cnt->imgs.height);
}
#endif


#if defined(HAVE_LINUX_VIDEODEV_H) && !defined(WITHOUT_V4L) && !defined(BSD)
static void event_vid_putpipe(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *img, char *dummy ATTRIBUTE_UNUSED, void *devpipe,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    if (*(int *)devpipe >= 0) {
        if (vid_putpipe(*(int *)devpipe, img, cnt->imgs.size) == -1)
            MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "%s: Failed to put image into video pipe");
    }
}
#endif /* !WITHOUT_V4L && !BSD */

const char *imageext(struct context *cnt)
{
    if (cnt->imgs.picture_type == IMAGE_TYPE_PPM)
        return "ppm";

    return "jpg";
}

static void event_image_detect(struct context *cnt,
        motion_event type ATTRIBUTE_UNUSED,
        unsigned char *newimg, char *dummy1 ATTRIBUTE_UNUSED,
        void *dummy2 ATTRIBUTE_UNUSED, struct tm *currenttime_tm)
{
    char fullfilename[PATH_MAX];
    char filename[PATH_MAX];

    if (cnt->new_img & NEWIMG_ON) {
        const char *imagepath;

        /*
         *  conf.imagepath would normally be defined but if someone deleted it by control interface
         *  it is better to revert to the default than fail
         */
        if (cnt->conf.imagepath)
            imagepath = cnt->conf.imagepath;
        else
            imagepath = DEF_IMAGEPATH;

        mystrftime(cnt, filename, sizeof(filename), imagepath, currenttime_tm, NULL, 0);
        snprintf(fullfilename, PATH_MAX, "%s/%s.%s", cnt->conf.filepath, filename, imageext(cnt));

        put_picture(cnt, fullfilename, newimg, FTYPE_IMAGE);
    }
}

static void event_imagem_detect(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *newimg ATTRIBUTE_UNUSED, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *currenttime_tm)
{
    struct config *conf = &cnt->conf;
    char fullfilenamem[PATH_MAX];
    char filename[PATH_MAX];
    char filenamem[PATH_MAX];

    if (conf->motion_img) {
        const char *imagepath;

        /*
         *  conf.imagepath would normally be defined but if someone deleted it by control interface
         *  it is better to revert to the default than fail
         */
        if (cnt->conf.imagepath)
            imagepath = cnt->conf.imagepath;
        else
            imagepath = DEF_IMAGEPATH;

        mystrftime(cnt, filename, sizeof(filename), imagepath, currenttime_tm, NULL, 0);
        /* motion images gets same name as normal images plus an appended 'm' */
        snprintf(filenamem, PATH_MAX, "%sm", filename);
        snprintf(fullfilenamem, PATH_MAX, "%s/%s.%s", cnt->conf.filepath, filenamem, imageext(cnt));

        put_picture(cnt, fullfilenamem, cnt->imgs.out, FTYPE_IMAGE_MOTION);
    }
}

static void event_image_snapshot(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *img, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *currenttime_tm)
{
    char fullfilename[PATH_MAX];

    if (strcmp(cnt->conf.snappath, "lastsnap")) {
        char filename[PATH_MAX];
        char filepath[PATH_MAX];
        char linkpath[PATH_MAX];
        const char *snappath;
        /*
         *  conf.snappath would normally be defined but if someone deleted it by control interface
         *  it is better to revert to the default than fail
         */
        if (cnt->conf.snappath)
            snappath = cnt->conf.snappath;
        else
            snappath = DEF_SNAPPATH;

        mystrftime(cnt, filepath, sizeof(filepath), snappath, currenttime_tm, NULL, 0);
        snprintf(filename, PATH_MAX, "%s.%s", filepath, imageext(cnt));
        snprintf(fullfilename, PATH_MAX, "%s/%s", cnt->conf.filepath, filename);
        put_picture(cnt, fullfilename, img, FTYPE_IMAGE_SNAPSHOT);

        /*
         *  Update symbolic link *after* image has been written so that
         *  the link always points to a valid file.
         */
        snprintf(linkpath, PATH_MAX, "%s/lastsnap.%s", cnt->conf.filepath, imageext(cnt));
        remove(linkpath);

        if (symlink(filename, linkpath)) {
            MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "%s: Could not create symbolic link [%s]",
                       filename);
            return;
        }
    } else {
        snprintf(fullfilename, PATH_MAX, "%s/lastsnap.%s", cnt->conf.filepath, imageext(cnt));
        remove(fullfilename);
        put_picture(cnt, fullfilename, img, FTYPE_IMAGE_SNAPSHOT);
    }

    cnt->snapshot = 0;
}

static void event_camera_lost(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *img ATTRIBUTE_UNUSED, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *currenttime_tm ATTRIBUTE_UNUSED)
{
    if (cnt->conf.on_camera_lost)
        exec_command(cnt, cnt->conf.on_camera_lost, NULL, 0);
}

static void on_movie_end_command(struct context *cnt,
                                 motion_event type ATTRIBUTE_UNUSED,
                                 unsigned char *dummy ATTRIBUTE_UNUSED, char *filename,
                                 void *arg, struct tm *tm ATTRIBUTE_UNUSED)
{
    int filetype = (unsigned long) arg;

    if ((filetype & FTYPE_MPEG_ANY) && cnt->conf.on_movie_end)
        exec_command(cnt, cnt->conf.on_movie_end, filename, filetype);
}

static void event_extpipe_end(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *dummy ATTRIBUTE_UNUSED, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *tm ATTRIBUTE_UNUSED)
{
    if (cnt->extpipe_open) {
        cnt->extpipe_open = 0;
        fflush(cnt->extpipe);
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "%s: CLOSING: extpipe file desc %d, error state %d",
                   fileno(cnt->extpipe), ferror(cnt->extpipe));
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "%s: pclose return: %d",
                   pclose(cnt->extpipe));
        event(cnt, EVENT_FILECLOSE, NULL, cnt->extpipefilename, (void *)FTYPE_MPEG, NULL);
    }
}

static void event_create_extpipe(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *dummy ATTRIBUTE_UNUSED, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *currenttime_tm)
{
    if ((cnt->conf.useextpipe) && (cnt->conf.extpipe)) {
        char stamp[PATH_MAX] = "";
        const char *moviepath;
        FILE *fd_dummy = NULL;

        /*
         *  conf.mpegpath would normally be defined but if someone deleted it by control interface
         *  it is better to revert to the default than fail
         */
        if (cnt->conf.moviepath) {
            moviepath = cnt->conf.moviepath;
        } else {
            moviepath = DEF_MOVIEPATH;
            MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "%s: moviepath: %s",
                       moviepath);
        }

        mystrftime(cnt, stamp, sizeof(stamp), moviepath, currenttime_tm, NULL, 0);
        snprintf(cnt->extpipefilename, PATH_MAX - 4, "%s/%s", cnt->conf.filepath, stamp);

        /* Open a dummy file to check if path is correct */
        fd_dummy = myfopen(cnt->extpipefilename, "w", 0);

        /* TODO: trigger some warning instead of only log an error message */
        if (fd_dummy == NULL) {
            /* Permission denied */
            if (errno ==  EACCES) {
                MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "%s: error opening file %s ..."
                           "check access rights to target directory",
                           cnt->extpipefilename);
                return ;
            } else {
                MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "%s: error opening file %s",
                           cnt->extpipefilename);
                return ;
            }

        }

        myfclose(fd_dummy);
        unlink(cnt->extpipefilename);

        mystrftime(cnt, stamp, sizeof(stamp), cnt->conf.extpipe, currenttime_tm, cnt->extpipefilename, 0);

        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "%s: pipe: %s",
                   stamp);
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "%s: cnt->moviefps: %d",
                   cnt->movie_fps);

        event(cnt, EVENT_FILECREATE, NULL, cnt->extpipefilename, (void *)FTYPE_MPEG, NULL);
        cnt->extpipe = popen(stamp, "w");

        if (cnt->extpipe == NULL) {
            MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "%s: popen failed");
            return;
        }

        setbuf(cnt->extpipe, NULL);
        cnt->extpipe_open = 1;
    }
}

static void event_extpipe_put(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *img, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *tm ATTRIBUTE_UNUSED)
{
    /* Check use_extpipe enabled and ext_pipe not NULL */
    if ((cnt->conf.useextpipe) && (cnt->extpipe != NULL)) {
        MOTION_LOG(DBG, TYPE_EVENTS, NO_ERRNO, "%s:");

        /* Check that is open */
        if ((cnt->extpipe_open) && (fileno(cnt->extpipe) > 0)) {
            if (!fwrite(img, cnt->imgs.size, 1, cnt->extpipe))
                MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "%s: Error writting in pipe , state error %d",
                           ferror(cnt->extpipe));
        } else {
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, "%s: pipe %s not created or closed already ",
                       cnt->extpipe);
        }
    }
}


static void event_new_video(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *dummy ATTRIBUTE_UNUSED, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *tm ATTRIBUTE_UNUSED)
{
    cnt->movie_last_shot = -1;

    cnt->movie_fps = cnt->lastrate;

    MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "%s FPS %d",
               cnt->movie_fps);

    if (cnt->movie_fps > 30)
        cnt->movie_fps = 30;
    else if (cnt->movie_fps < 2)
        cnt->movie_fps = 2;
}

#ifdef HAVE_FFMPEG

static void grey2yuv420p(unsigned char *u, unsigned char *v, int width, int height)
{
    memset(u, 128, width * height / 4);
    memset(v, 128, width * height / 4);
}


static void event_ffmpeg_newfile(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *img, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *currenttime_tm)
{
    int width = cnt->imgs.width;
    int height = cnt->imgs.height;
    unsigned char *convbuf, *y, *u, *v;
    char stamp[PATH_MAX];
    const char *moviepath;

    if (!cnt->conf.ffmpeg_output && !cnt->conf.ffmpeg_output_debug)
        return;

    /*
     *  conf.mpegpath would normally be defined but if someone deleted it by control interface
     *  it is better to revert to the default than fail
     */
    if (cnt->conf.moviepath)
        moviepath = cnt->conf.moviepath;
    else
        moviepath = DEF_MOVIEPATH;

    mystrftime(cnt, stamp, sizeof(stamp), moviepath, currenttime_tm, NULL, 0);

    /*
     *  motion movies get the same name as normal movies plus an appended 'm'
     *  PATH_MAX - 4 to allow for .mpg to be appended without overflow
     */
    snprintf(cnt->motionfilename, PATH_MAX - 4, "%s/%sm", cnt->conf.filepath, stamp);
    snprintf(cnt->newfilename, PATH_MAX - 4, "%s/%s", cnt->conf.filepath, stamp);

    if (cnt->conf.ffmpeg_output) {
        if (cnt->imgs.type == VIDEO_PALETTE_GREY) {
            convbuf = mymalloc((width * height) / 2);
            y = img;
            u = convbuf;
            v = convbuf + (width * height) / 4;
            grey2yuv420p(u, v, width, height);
        } else {
            convbuf = NULL;
            y = img;
            u = img + width * height;
            v = u + (width * height) / 4;
        }

        if ((cnt->ffmpeg_output =
            ffmpeg_open((char *)cnt->conf.ffmpeg_video_codec, cnt->newfilename, y, u, v,
                         cnt->imgs.width, cnt->imgs.height, cnt->movie_fps, cnt->conf.ffmpeg_bps,
                         cnt->conf.ffmpeg_vbr,TIMELAPSE_NONE)) == NULL) {
            MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "%s: ffopen_open error creating (new) file [%s]",
                       cnt->newfilename);
            cnt->finish = 1;
            return;
        }

        ((struct ffmpeg *)cnt->ffmpeg_output)->udata = convbuf;
        event(cnt, EVENT_FILECREATE, NULL, cnt->newfilename, (void *)FTYPE_MPEG, NULL);
    }

    if (cnt->conf.ffmpeg_output_debug) {
        if (cnt->imgs.type == VIDEO_PALETTE_GREY) {
            convbuf = mymalloc((width * height) / 2);
            y = cnt->imgs.out;
            u = convbuf;
            v = convbuf + (width * height) / 4;
            grey2yuv420p(u, v, width, height);
        } else {
            y = cnt->imgs.out;
            u = cnt->imgs.out + width *height;
            v = u + (width * height) / 4;
            convbuf = NULL;
        }

        if ((cnt->ffmpeg_output_debug =
            ffmpeg_open((char *)cnt->conf.ffmpeg_video_codec, cnt->motionfilename, y, u, v,
                         cnt->imgs.width, cnt->imgs.height, cnt->movie_fps, cnt->conf.ffmpeg_bps,
                         cnt->conf.ffmpeg_vbr,TIMELAPSE_NONE)) == NULL) {
            MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "%s: ffopen_open error creating (motion) file [%s]",
                       cnt->motionfilename);
            cnt->finish = 1;
            return;
        }

        cnt->ffmpeg_output_debug->udata = convbuf;
        event(cnt, EVENT_FILECREATE, NULL, cnt->motionfilename, (void *)FTYPE_MPEG_MOTION, NULL);
    }
}

static void event_ffmpeg_timelapse(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED, unsigned char *img,
            char *dummy1 ATTRIBUTE_UNUSED, void *dummy2 ATTRIBUTE_UNUSED,
            struct tm *currenttime_tm)
{
    int width = cnt->imgs.width;
    int height = cnt->imgs.height;
    unsigned char *convbuf, *y, *u, *v;

    if (!cnt->ffmpeg_timelapse) {
        char tmp[PATH_MAX];
        const char *timepath;

        /*
         *  conf.timepath would normally be defined but if someone deleted it by control interface
         *  it is better to revert to the default than fail
         */
        if (cnt->conf.timepath)
            timepath = cnt->conf.timepath;
        else
            timepath = DEF_TIMEPATH;

        mystrftime(cnt, tmp, sizeof(tmp), timepath, currenttime_tm, NULL, 0);

        /* PATH_MAX - 4 to allow for .mpg to be appended without overflow */
        snprintf(cnt->timelapsefilename, PATH_MAX - 4, "%s/%s", cnt->conf.filepath, tmp);

        if (cnt->imgs.type == VIDEO_PALETTE_GREY) {
            convbuf = mymalloc((width * height) / 2);
            y = img;
            u = convbuf;
            v = convbuf + (width * height) / 4;
            grey2yuv420p(u, v, width, height);
        } else {
            convbuf = NULL;
            y = img;
            u = img + width * height;
            v = u + (width * height) / 4;
        }

        if (strcmp(cnt->conf.ffmpeg_video_codec,"swf") == 0) {
            MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "%s: Timelapse using swf codec.");
            MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "%s: Events will be appended to file");
            cnt->ffmpeg_timelapse =
                ffmpeg_open("swf",cnt->timelapsefilename, y, u, v
                        ,cnt->imgs.width, cnt->imgs.height, 24
                        ,cnt->conf.ffmpeg_bps,cnt->conf.ffmpeg_vbr,TIMELAPSE_APPEND);
        } else {
            MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "%s: Timelapse using mpeg4 codec.");
            MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "%s: Events will be trigger new files");
            cnt->ffmpeg_timelapse =
                ffmpeg_open("mpeg4",cnt->timelapsefilename, y, u, v
                        ,cnt->imgs.width, cnt->imgs.height, 1
                        ,cnt->conf.ffmpeg_bps,cnt->conf.ffmpeg_vbr,TIMELAPSE_NEW);
        }

        if (cnt->ffmpeg_timelapse == NULL){
            MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "%s: ffopen_open error creating "
                       "(timelapse) file [%s]", cnt->timelapsefilename);
            cnt->finish = 1;
            return;
        }

        cnt->ffmpeg_timelapse->udata = convbuf;
        event(cnt, EVENT_FILECREATE, NULL, cnt->timelapsefilename, (void *)FTYPE_MPEG_TIMELAPSE, NULL);
    }

    y = img;

    if (cnt->imgs.type == VIDEO_PALETTE_GREY)
        u = cnt->ffmpeg_timelapse->udata;
    else
        u = img + width * height;

    v = u + (width * height) / 4;

    if (ffmpeg_put_other_image(cnt->ffmpeg_timelapse, y, u, v) == -1) {
        cnt->finish = 1;
        cnt->restart = 0;
    }

}

static void event_ffmpeg_put(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *img, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *tm ATTRIBUTE_UNUSED)
{
    if (cnt->ffmpeg_output) {
        int width = cnt->imgs.width;
        int height = cnt->imgs.height;
        unsigned char *y = img;
        unsigned char *u, *v;

        if (cnt->imgs.type == VIDEO_PALETTE_GREY)
            u = cnt->ffmpeg_timelapse->udata;
        else
            u = y + (width * height);

        v = u + (width * height) / 4;
        if (ffmpeg_put_other_image(cnt->ffmpeg_output, y, u, v) == -1) {
            cnt->finish = 1;
            cnt->restart = 0;
        }
    }

    if (cnt->ffmpeg_output_debug) {
        if (ffmpeg_put_image(cnt->ffmpeg_output_debug) == -1) {
            cnt->finish = 1;
            cnt->restart = 0;
        }
    }
}

static void event_ffmpeg_closefile(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct tm *tm ATTRIBUTE_UNUSED)
{

    if (cnt->ffmpeg_output) {
        free(cnt->ffmpeg_output->udata);

        ffmpeg_close(cnt->ffmpeg_output);
        cnt->ffmpeg_output = NULL;

        event(cnt, EVENT_FILECLOSE, NULL, cnt->newfilename, (void *)FTYPE_MPEG, NULL);
    }

    if (cnt->ffmpeg_output_debug) {
        free(cnt->ffmpeg_output_debug->udata);

        ffmpeg_close(cnt->ffmpeg_output_debug);
        cnt->ffmpeg_output_debug = NULL;

        event(cnt, EVENT_FILECLOSE, NULL, cnt->motionfilename, (void *)FTYPE_MPEG_MOTION, NULL);
    }
}

static void event_ffmpeg_timelapseend(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            unsigned char *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    if (cnt->ffmpeg_timelapse) {
        free(cnt->ffmpeg_timelapse->udata);

        ffmpeg_close(cnt->ffmpeg_timelapse);
        cnt->ffmpeg_timelapse = NULL;

        event(cnt, EVENT_FILECLOSE, NULL, cnt->timelapsefilename, (void *)FTYPE_MPEG_TIMELAPSE, NULL);
    }
}

#endif /* HAVE_FFMPEG */


/*
 * Starting point for all events
 */

struct event_handlers {
    motion_event type;
    event_handler handler;
};

struct event_handlers event_handlers[] = {
#if defined(HAVE_MYSQL) || defined(HAVE_PGSQL) || defined(HAVE_SQLITE3)
    {
    EVENT_FILECREATE,
    event_sqlnewfile
    },
#endif
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
#ifdef HAVE_SDL
    {
    EVENT_SDL_PUT,
    event_sdl_put
    },
#endif
#if defined(HAVE_LINUX_VIDEODEV_H) && !defined(WITHOUT_V4L) && !defined(BSD)
    {
    EVENT_IMAGE,
    event_vid_putpipe
    },
    {
    EVENT_IMAGEM,
    event_vid_putpipe
    },
#endif /* !WITHOUT_V4L && !BSD */
    {
    EVENT_STREAM,
    event_stream_put
    },
    {
    EVENT_FIRSTMOTION,
    event_new_video
    },
#ifdef HAVE_FFMPEG
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
#endif /* HAVE_FFMPEG */
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
    EVENT_STOP,
    event_stop_stream
    },
    {0, NULL}
};


/**
 * event
 *   defined with the following parameters:
 *      - Type as defined in event.h (EVENT_...)
 *      - The global context struct cnt
 *      - image - A pointer to unsigned char as used for images
 *      - filename - A pointer to typically a string for a file path
 *      - eventdata - A void pointer that can be cast to anything. E.g. FTYPE_...
 *      - tm - A tm struct that carries a full time structure
 * The split between unsigned images and signed filenames was introduced in 3.2.2
 * as a code reading friendly solution to avoid a stream of compiler warnings in gcc 4.0.
 */
void event(struct context *cnt, motion_event type, unsigned char *image,
           char *filename, void *eventdata, struct tm *tm)
{
    int i=-1;

    while (event_handlers[++i].handler) {
        if (type == event_handlers[i].type)
            event_handlers[i].handler(cnt, type, image, filename, eventdata,
                                      tm);
    }
}
