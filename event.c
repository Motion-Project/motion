/*
    event.c

    Generalised event handling for motion

    Copyright Jeroen Vreeken, 2002
    This software is distributed under the GNU Public License Version 2
    see also the file 'COPYING'.
*/
#include "netcam_rtsp.h"
#include "ffmpeg.h"
#include "picture.h"   /* already includes motion.h */
#include "event.h"
#include "video_loopback.h"
#include "video_common.h"

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
    "EVENT_CAMERA_FOUND",
    "EVENT_FFMPEG_PUT",
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
        for (i = getdtablesize() - 1; i > 2; i--)
            close(i);

        execl("/bin/sh", "sh", "-c", stamp, " &", NULL);

        /* if above function succeeds the program never reach here */
        MOTION_LOG(ALR, TYPE_EVENTS, SHOW_ERRNO, "Unable to start external command '%s'",
                   stamp);

        exit(1);
    }

    MOTION_LOG(DBG, TYPE_EVENTS, NO_ERRNO, "Executing external command '%s'",
               stamp);
}

/*
 * Event handlers
 */

static void event_newfile(struct context *cnt ATTRIBUTE_UNUSED,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *dummy ATTRIBUTE_UNUSED, char *filename, void *ftype,
            struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "File of type %ld saved to: %s",
               (unsigned long)ftype, filename);
}


static void event_beep(struct context *cnt, motion_event type ATTRIBUTE_UNUSED,
            struct image_data *dummy ATTRIBUTE_UNUSED,
            char *filename ATTRIBUTE_UNUSED,
            void *ftype ATTRIBUTE_UNUSED,
            struct timeval *tv1 ATTRIBUTE_UNUSED)
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
            struct image_data *dummy ATTRIBUTE_UNUSED,
            char *filename, void *arg, struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    int filetype = (unsigned long)arg;

    if ((filetype & FTYPE_IMAGE_ANY) != 0 && cnt->conf.on_picture_save)
        exec_command(cnt, cnt->conf.on_picture_save, filename, filetype);

    if ((filetype & FTYPE_MPEG_ANY) != 0 && cnt->conf.on_movie_start)
        exec_command(cnt, cnt->conf.on_movie_start, filename, filetype);
}

static void on_motion_detected_command(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    if (cnt->conf.on_motion_detected)
        exec_command(cnt, cnt->conf.on_motion_detected, NULL, 0);
}

#if defined(HAVE_MYSQL) || defined(HAVE_PGSQL) || defined(HAVE_SQLITE3)

static void do_sql_query(char *sqlquery, struct context *cnt, int save_id)
{
#ifdef HAVE_MYSQL
    if (!strcmp(cnt->conf.database_type, "mysql")) {
        if (mysql_query(cnt->database, sqlquery) != 0) {
            int error_code = mysql_errno(cnt->database);

            MOTION_LOG(ERR, TYPE_DB, SHOW_ERRNO, "Mysql query failed %s error code %d",
                       mysql_error(cnt->database), error_code);
            /* Try to reconnect ONCE if fails continue and discard this sql query */
            if (error_code >= 2000) {
                // Close connection before start a new connection
                mysql_close(cnt->database);

                cnt->database = (MYSQL *) mymalloc(sizeof(MYSQL));
                mysql_init(cnt->database);

                if (!mysql_real_connect(cnt->database, cnt->conf.database_host,
                                        cnt->conf.database_user, cnt->conf.database_password,
                                        cnt->conf.database_dbname, 0, NULL, 0)) {
                    MOTION_LOG(ALR, TYPE_DB, NO_ERRNO, "Cannot reconnect to MySQL"
                               " database %s on host %s with user %s MySQL error was %s",
                               cnt->conf.database_dbname,
                               cnt->conf.database_host, cnt->conf.database_user,
                               mysql_error(cnt->database));
                } else {
                    MOTION_LOG(INF, TYPE_DB, NO_ERRNO, "Re-Connection to Mysql database '%s' Succeed",
                               cnt->conf.database_dbname);
                    if (mysql_query(cnt->database, sqlquery) != 0) {
                        int error_my = mysql_errno(cnt->database);
                        MOTION_LOG(ERR, TYPE_DB, SHOW_ERRNO, "after re-connection Mysql query failed %s error code %d",
                                   mysql_error(cnt->database), error_my);
                    }
                }
            }
        }
        if (save_id) {
            cnt->database_event_id = (unsigned long long) mysql_insert_id(cnt->database);
        }
    }
#endif /* HAVE_MYSQL */


#ifdef HAVE_PGSQL
    if (!strcmp(cnt->conf.database_type, "postgresql")) {
        PGresult *res;

        res = PQexec(cnt->database_pg, sqlquery);

        if (PQstatus(cnt->database_pg) == CONNECTION_BAD) {

            MOTION_LOG(ERR, TYPE_DB, NO_ERRNO, "Connection to PostgreSQL database '%s' failed: %s",
                       cnt->conf.database_dbname, PQerrorMessage(cnt->database_pg));

	    // This function will close the connection to the server and attempt to reestablish a new connection to the same server,
	    // using all the same parameters previously used. This may be useful for error recovery if a working connection is lost
            PQreset(cnt->database_pg);

            if (PQstatus(cnt->database_pg) == CONNECTION_BAD) {
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO, "Re-Connection to PostgreSQL database '%s' failed: %s",
                           cnt->conf.database_dbname, PQerrorMessage(cnt->database_pg));
            } else {
                MOTION_LOG(INF, TYPE_DB, NO_ERRNO, "Re-Connection to PostgreSQL database '%s' Succeed",
                           cnt->conf.database_dbname);
            }

        } else if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            MOTION_LOG(ERR, TYPE_DB, SHOW_ERRNO, "PGSQL query [%s] failed", sqlquery);
            PQclear(res);
        }
        if (save_id) {
            //ToDO:  Find the equivalent option for pgsql
            cnt->database_event_id = 0;
        }
    }
#endif /* HAVE_PGSQL */

#ifdef HAVE_SQLITE3
    if ((!strcmp(cnt->conf.database_type, "sqlite3")) && (cnt->conf.database_dbname)) {
        int res;
        char *errmsg = 0;
        res = sqlite3_exec(cnt->database_sqlite3, sqlquery, NULL, 0, &errmsg);
        if (res != SQLITE_OK ) {
            MOTION_LOG(ERR, TYPE_DB, NO_ERRNO, "SQLite error was %s",
                       errmsg);
            sqlite3_free(errmsg);
        }
        if (save_id) {
            //ToDO:  Find the equivalent option for sqlite3
            cnt->database_event_id = 0;
        }

    }
#endif /* HAVE_SQLITE3 */
}

static void event_sqlfirstmotion(struct context *cnt, motion_event type  ATTRIBUTE_UNUSED,
                                 struct image_data *dummy1 ATTRIBUTE_UNUSED,
                                 char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
                                 struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    /* Only log the file types we want */
    if (!(cnt->conf.database_type)) {
        return;
    }

    /*
     * We place the code in a block so we only spend time making space in memory
     * for the sqlquery and timestr when we actually need it.
     */
    {
        char sqlquery[PATH_MAX];

        mystrftime(cnt, sqlquery, sizeof(sqlquery), cnt->conf.sql_query_start,
                   &cnt->current_image->timestamp_tv, NULL, 0);

        do_sql_query(sqlquery, cnt, 1);
    }
}

static void event_sqlnewfile(struct context *cnt, motion_event type  ATTRIBUTE_UNUSED,
            struct image_data *dummy ATTRIBUTE_UNUSED,
            char *filename, void *arg, struct timeval *tv1 ATTRIBUTE_UNUSED)
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
                   &cnt->current_image->timestamp_tv, filename, sqltype);

        do_sql_query(sqlquery, cnt, 0);
    }
}

#endif /* defined HAVE_MYSQL || defined HAVE_PGSQL || defined(HAVE_SQLITE3) */

static void on_area_command(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    if (cnt->conf.on_area_detected)
        exec_command(cnt, cnt->conf.on_area_detected, NULL, 0);
}

static void on_event_start_command(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    if (cnt->conf.on_event_start)
        exec_command(cnt, cnt->conf.on_event_start, NULL, 0);
}

static void on_event_end_command(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    if (cnt->conf.on_event_end)
        exec_command(cnt, cnt->conf.on_event_end, NULL, 0);
}

static void event_stop_stream(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    if ((cnt->conf.stream_port) && (cnt->stream.socket != -1))
        stream_stop(&cnt->stream);

    if ((cnt->conf.substream_port) && (cnt->substream.socket != -1))
        stream_stop(&cnt->substream);
}

static void event_stream_put(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *img_data, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    if (cnt->conf.stream_port)
        stream_put(cnt, &cnt->stream, &cnt->stream_count, img_data->image_norm, 0);

    if (cnt->conf.substream_port)
        stream_put(cnt, &cnt->substream, &cnt->substream_count, img_data->image_norm, 1);
}


#if defined(HAVE_V4L2) && !defined(__FreeBSD__)
static void event_vlp_putpipe(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *img_data, char *dummy ATTRIBUTE_UNUSED, void *devpipe,
            struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    if (*(int *)devpipe >= 0) {
        if (vlp_putpipe(*(int *)devpipe, img_data->image_norm, cnt->imgs.size_norm) == -1)
            MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "Failed to put image into video pipe");
    }
}
#endif /* defined(HAVE_V4L2) && !__FreeBSD__  */

const char *imageext(struct context *cnt)
{
    if (cnt->imgs.picture_type == IMAGE_TYPE_PPM)
        return "ppm";

    if (cnt->imgs.picture_type == IMAGE_TYPE_WEBP)
        return "webp";

    return "jpg";
}

static void event_image_detect(struct context *cnt,
        motion_event type ATTRIBUTE_UNUSED,
        struct image_data *img_data, char *dummy1 ATTRIBUTE_UNUSED,
        void *dummy2 ATTRIBUTE_UNUSED, struct timeval *currenttime_tv)
{
    char fullfilename[PATH_MAX];
    char filename[PATH_MAX];
    int  passthrough;

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

        mystrftime(cnt, filename, sizeof(filename), imagepath, currenttime_tv, NULL, 0);
        snprintf(fullfilename, PATH_MAX, "%s/%s.%s", cnt->conf.filepath, filename, imageext(cnt));

        passthrough = util_check_passthrough(cnt);
        if ((cnt->imgs.size_high > 0) && (!passthrough)) {
            put_picture(cnt, fullfilename,img_data->image_high, FTYPE_IMAGE);
        } else {
            put_picture(cnt, fullfilename,img_data->image_norm, FTYPE_IMAGE);
        }

    }
}

static void event_imagem_detect(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *img_data ATTRIBUTE_UNUSED, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct timeval *currenttime_tv)
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

        mystrftime(cnt, filename, sizeof(filename), imagepath, currenttime_tv, NULL, 0);

        /* motion images gets same name as normal images plus an appended 'm' */
        snprintf(filenamem, PATH_MAX, "%sm", filename);
        snprintf(fullfilenamem, PATH_MAX, "%s/%s.%s", cnt->conf.filepath, filenamem, imageext(cnt));

        put_picture(cnt, fullfilenamem, cnt->imgs.img_motion.image_norm, FTYPE_IMAGE_MOTION);
    }
}

static void event_image_snapshot(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *img_data, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct timeval *currenttime_tv)
{
    char fullfilename[PATH_MAX];
    char filename[PATH_MAX];
    char filepath[PATH_MAX];
    int offset = 0;
    int len = strlen(cnt->conf.snappath);

    if (len >= 9)
        offset = len - 8;

    if (strcmp(cnt->conf.snappath+offset, "lastsnap")) {
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

        mystrftime(cnt, filepath, sizeof(filepath), snappath, currenttime_tv, NULL, 0);
        snprintf(filename, PATH_MAX, "%s.%s", filepath, imageext(cnt));
        snprintf(fullfilename, PATH_MAX, "%s/%s", cnt->conf.filepath, filename);
        put_picture(cnt, fullfilename, img_data->image_norm, FTYPE_IMAGE_SNAPSHOT);

        /*
         *  Update symbolic link *after* image has been written so that
         *  the link always points to a valid file.
         */
        snprintf(linkpath, PATH_MAX, "%s/lastsnap.%s", cnt->conf.filepath, imageext(cnt));
        remove(linkpath);

        if (symlink(filename, linkpath)) {
            MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "Could not create symbolic link [%s]",
                       filename);
            return;
        }
    } else {
        mystrftime(cnt, filepath, sizeof(filepath), cnt->conf.snappath, currenttime_tv, NULL, 0);
        snprintf(filename, PATH_MAX, "%s.%s", filepath, imageext(cnt));
        snprintf(fullfilename, PATH_MAX, "%s/%s", cnt->conf.filepath, filename);
        remove(fullfilename);
        put_picture(cnt, fullfilename, img_data->image_norm, FTYPE_IMAGE_SNAPSHOT);
    }

    cnt->snapshot = 0;
}

static void event_camera_lost(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *img_data ATTRIBUTE_UNUSED, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    if (cnt->conf.on_camera_lost)
        exec_command(cnt, cnt->conf.on_camera_lost, NULL, 0);
}

static void event_camera_found(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *img_data ATTRIBUTE_UNUSED, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    if (cnt->conf.on_camera_found)
        exec_command(cnt, cnt->conf.on_camera_found, NULL, 0);
}

static void on_movie_end_command(struct context *cnt,
                                 motion_event type ATTRIBUTE_UNUSED,
                                 struct image_data *dummy ATTRIBUTE_UNUSED, char *filename,
                                 void *arg, struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    int filetype = (unsigned long) arg;

    if ((filetype & FTYPE_MPEG_ANY) && cnt->conf.on_movie_end)
        exec_command(cnt, cnt->conf.on_movie_end, filename, filetype);
}

static void event_extpipe_end(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *dummy ATTRIBUTE_UNUSED, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    if (cnt->extpipe_open) {
        cnt->extpipe_open = 0;
        fflush(cnt->extpipe);
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "CLOSING: extpipe file desc %d, error state %d",
                   fileno(cnt->extpipe), ferror(cnt->extpipe));
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "pclose return: %d",
                   pclose(cnt->extpipe));
        event(cnt, EVENT_FILECLOSE, NULL, cnt->extpipefilename, (void *)FTYPE_MPEG, NULL);
    }
}

static void event_create_extpipe(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *dummy ATTRIBUTE_UNUSED, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct timeval *currenttime_tv)
{
    if ((cnt->conf.useextpipe) && (cnt->conf.extpipe)) {
        char stamp[PATH_MAX] = "";
        const char *moviepath;

        /*
         *  conf.mpegpath would normally be defined but if someone deleted it by control interface
         *  it is better to revert to the default than fail
         */
        if (cnt->conf.moviepath) {
            moviepath = cnt->conf.moviepath;
        } else {
            moviepath = DEF_MOVIEPATH;
            MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "moviepath: %s",
                       moviepath);
        }

        mystrftime(cnt, stamp, sizeof(stamp), moviepath, currenttime_tv, NULL, 0);
        snprintf(cnt->extpipefilename, PATH_MAX - 4, "%s/%s", cnt->conf.filepath, stamp);

        if (access(cnt->conf.filepath, W_OK)!= 0) {
            /* Permission denied */
            if (errno ==  EACCES) {
                MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "no write access to target directory %s",
                           cnt->conf.filepath);
                return ;
            /* Path not found - create it */
            } else if (errno ==  ENOENT) {
                MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "path not found, trying to create it %s ...",
                           cnt->conf.filepath);
                if (create_path(cnt->extpipefilename) == -1)
                    return ;
            }
            else {
                MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "error accesing path %s", cnt->conf.filepath);
                return ;
            }
        }

        /* Always create any path specified as file name */
        if (create_path(cnt->extpipefilename) == -1)
            return ;

        mystrftime(cnt, stamp, sizeof(stamp), cnt->conf.extpipe, currenttime_tv, cnt->extpipefilename, 0);

        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "pipe: %s", stamp);

        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "cnt->moviefps: %d", cnt->movie_fps);

        event(cnt, EVENT_FILECREATE, NULL, cnt->extpipefilename, (void *)FTYPE_MPEG, NULL);
        cnt->extpipe = popen(stamp, "w");

        if (cnt->extpipe == NULL) {
            MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "popen failed");
            return;
        }

        setbuf(cnt->extpipe, NULL);
        cnt->extpipe_open = 1;
    }
}

static void event_extpipe_put(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *img_data, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    int passthrough;

    /* Check use_extpipe enabled and ext_pipe not NULL */
    if ((cnt->conf.useextpipe) && (cnt->extpipe != NULL)) {
        MOTION_LOG(DBG, TYPE_EVENTS, NO_ERRNO, "Using extpipe");
        passthrough = util_check_passthrough(cnt);
        /* Check that is open */
        if ((cnt->extpipe_open) && (fileno(cnt->extpipe) > 0)) {
            if ((cnt->imgs.size_high > 0) && (!passthrough)){
                if (!fwrite(img_data->image_high, cnt->imgs.size_high, 1, cnt->extpipe))
                    MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "Error writing in pipe , state error %d",
                           ferror(cnt->extpipe));
            } else {
                if (!fwrite(img_data->image_norm, cnt->imgs.size_norm, 1, cnt->extpipe))
                    MOTION_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, "Error writing in pipe , state error %d",
                           ferror(cnt->extpipe));
           }
        } else {
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, "pipe %s not created or closed already ",
                       cnt->conf.extpipe);
        }
    }
}


static void event_new_video(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *dummy ATTRIBUTE_UNUSED, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    cnt->movie_last_shot = -1;

    cnt->movie_fps = cnt->lastrate;

    MOTION_LOG(INF, TYPE_EVENTS, NO_ERRNO, "Source FPS %d", cnt->movie_fps);

    if (cnt->movie_fps < 2) cnt->movie_fps = 2;

}


static void event_ffmpeg_newfile(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *dummy0 ATTRIBUTE_UNUSED,
            char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED,
            struct timeval *currenttime_tv)
{
    char stamp[PATH_MAX];
    const char *moviepath;
    const char *codec;
    long codenbr;
    int retcd;

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

    mystrftime(cnt, stamp, sizeof(stamp), moviepath, currenttime_tv, NULL, 0);

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
    codec = cnt->conf.ffmpeg_video_codec;
    if (strcmp(codec, "ogg") == 0) {
        MOTION_LOG(WRN, TYPE_ENCODER, NO_ERRNO, "The ogg container is no longer supported.  Changing to mpeg4");
        codec = "mpeg4";
    }
    if (strcmp(codec, "test") == 0) {
        MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO, "Running test of the various output formats.");
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
        snprintf(cnt->motionfilename, PATH_MAX - 4, "%s/%s_%sm", cnt->conf.filepath, codec, stamp);
        snprintf(cnt->newfilename, PATH_MAX - 4, "%s/%s_%s", cnt->conf.filepath, codec, stamp);
    } else {
        snprintf(cnt->motionfilename, PATH_MAX - 4, "%s/%sm", cnt->conf.filepath, stamp);
        snprintf(cnt->newfilename, PATH_MAX - 4, "%s/%s", cnt->conf.filepath, stamp);
    }
    if (cnt->conf.ffmpeg_output) {
        cnt->ffmpeg_output = mymalloc(sizeof(struct ffmpeg));
        if (cnt->imgs.size_high > 0){
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
        cnt->ffmpeg_output->bps = cnt->conf.ffmpeg_bps;
        cnt->ffmpeg_output->filename = cnt->newfilename;
        cnt->ffmpeg_output->vbr = cnt->conf.ffmpeg_vbr;
        cnt->ffmpeg_output->start_time.tv_sec = currenttime_tv->tv_sec;
        cnt->ffmpeg_output->start_time.tv_usec = currenttime_tv->tv_usec;
        cnt->ffmpeg_output->last_pts = -1;
        cnt->ffmpeg_output->base_pts = 0;
        cnt->ffmpeg_output->gop_cnt = 0;
        cnt->ffmpeg_output->codec_name = codec;
        if (strcmp(cnt->conf.ffmpeg_video_codec, "test") == 0) {
            cnt->ffmpeg_output->test_mode = 1;
        } else {
            cnt->ffmpeg_output->test_mode = 0;
        }
        cnt->ffmpeg_output->motion_images = 0;
        cnt->ffmpeg_output->passthrough =util_check_passthrough(cnt);


        retcd = ffmpeg_open(cnt->ffmpeg_output);
        if (retcd < 0){
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, "Error opening context for movie output.");
            free(cnt->ffmpeg_output);
            cnt->ffmpeg_output=NULL;
            return;
        }
        event(cnt, EVENT_FILECREATE, NULL, cnt->newfilename, (void *)FTYPE_MPEG, NULL);
    }

    if (cnt->conf.ffmpeg_output_debug) {
        cnt->ffmpeg_output_debug = mymalloc(sizeof(struct ffmpeg));
        cnt->ffmpeg_output_debug->width  = cnt->imgs.width;
        cnt->ffmpeg_output_debug->height = cnt->imgs.height;
        cnt->ffmpeg_output_debug->rtsp_data = NULL;
        cnt->ffmpeg_output_debug->tlapse = TIMELAPSE_NONE;
        cnt->ffmpeg_output_debug->fps = cnt->movie_fps;
        cnt->ffmpeg_output_debug->bps = cnt->conf.ffmpeg_bps;
        cnt->ffmpeg_output_debug->filename = cnt->motionfilename;
        cnt->ffmpeg_output_debug->vbr = cnt->conf.ffmpeg_vbr;
        cnt->ffmpeg_output_debug->start_time.tv_sec = currenttime_tv->tv_sec;
        cnt->ffmpeg_output_debug->start_time.tv_usec = currenttime_tv->tv_usec;
        cnt->ffmpeg_output_debug->last_pts = -1;
        cnt->ffmpeg_output_debug->base_pts = 0;
        cnt->ffmpeg_output_debug->gop_cnt = 0;
        cnt->ffmpeg_output_debug->codec_name = codec;
        if (strcmp(cnt->conf.ffmpeg_video_codec, "test") == 0) {
            cnt->ffmpeg_output_debug->test_mode = TRUE;
        } else {
            cnt->ffmpeg_output_debug->test_mode = FALSE;
        }
        cnt->ffmpeg_output_debug->motion_images = TRUE;
        cnt->ffmpeg_output_debug->passthrough = FALSE;
        cnt->ffmpeg_output_debug->high_resolution = FALSE;
        cnt->ffmpeg_output_debug->rtsp_data = NULL;

        retcd = ffmpeg_open(cnt->ffmpeg_output_debug);
        if (retcd < 0){
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, "ffopen_open error creating (motion) file [%s]", cnt->motionfilename);
            free(cnt->ffmpeg_output_debug);
            cnt->ffmpeg_output_debug = NULL;
            return;
        }
    }
}

static void event_ffmpeg_timelapse(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED, struct image_data *img_data,
            char *dummy1 ATTRIBUTE_UNUSED, void *dummy2 ATTRIBUTE_UNUSED,
            struct timeval *currenttime_tv)
{
    int retcd;
    int passthrough;

    if (!cnt->ffmpeg_timelapse) {
        char tmp[PATH_MAX];
        const char *timepath;
        const char *codec_mpg = "mpg";
        const char *codec_mpeg = "mpeg4";

        /*
         *  conf.timepath would normally be defined but if someone deleted it by control interface
         *  it is better to revert to the default than fail
         */
        if (cnt->conf.timepath)
            timepath = cnt->conf.timepath;
        else
            timepath = DEF_TIMEPATH;

        mystrftime(cnt, tmp, sizeof(tmp), timepath, currenttime_tv, NULL, 0);

        /* PATH_MAX - 4 to allow for .mpg to be appended without overflow */
        snprintf(cnt->timelapsefilename, PATH_MAX - 4, "%s/%s", cnt->conf.filepath, tmp);
        passthrough = util_check_passthrough(cnt);
        cnt->ffmpeg_timelapse = mymalloc(sizeof(struct ffmpeg));
        if ((cnt->imgs.size_high > 0) && (!passthrough)){
            cnt->ffmpeg_timelapse->width  = cnt->imgs.width_high;
            cnt->ffmpeg_timelapse->height = cnt->imgs.height_high;
            cnt->ffmpeg_timelapse->high_resolution = TRUE;
        } else {
            cnt->ffmpeg_timelapse->width  = cnt->imgs.width;
            cnt->ffmpeg_timelapse->height = cnt->imgs.height;
            cnt->ffmpeg_timelapse->high_resolution = FALSE;
        }
        cnt->ffmpeg_timelapse->fps = cnt->conf.timelapse_fps;
        cnt->ffmpeg_timelapse->bps = cnt->conf.ffmpeg_bps;
        cnt->ffmpeg_timelapse->filename = cnt->timelapsefilename;
        cnt->ffmpeg_timelapse->vbr = cnt->conf.ffmpeg_vbr;
        cnt->ffmpeg_timelapse->start_time.tv_sec = currenttime_tv->tv_sec;
        cnt->ffmpeg_timelapse->start_time.tv_usec = currenttime_tv->tv_usec;
        cnt->ffmpeg_timelapse->last_pts = -1;
        cnt->ffmpeg_timelapse->base_pts = 0;
        cnt->ffmpeg_timelapse->test_mode = FALSE;
        cnt->ffmpeg_timelapse->gop_cnt = 0;
        cnt->ffmpeg_timelapse->motion_images = FALSE;
        cnt->ffmpeg_timelapse->passthrough = FALSE;
        cnt->ffmpeg_timelapse->rtsp_data = NULL;

        if ((strcmp(cnt->conf.timelapse_codec,"mpg") == 0) ||
            (strcmp(cnt->conf.timelapse_codec,"swf") == 0) ){

            if (strcmp(cnt->conf.timelapse_codec,"swf") == 0) {
                MOTION_LOG(WRN, TYPE_EVENTS, NO_ERRNO,
                    "The swf container for timelapse no longer supported.  Using mpg container.");
            }

            MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "Timelapse using mpg codec.");
            MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "Events will be appended to file");

            cnt->ffmpeg_timelapse->tlapse = TIMELAPSE_APPEND;
            cnt->ffmpeg_timelapse->codec_name = codec_mpg;
            retcd = ffmpeg_open(cnt->ffmpeg_timelapse);
        } else {
            MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "Timelapse using mpeg4 codec.");
            MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, "Events will be trigger new files");

            cnt->ffmpeg_timelapse->tlapse = TIMELAPSE_NEW;
            cnt->ffmpeg_timelapse->codec_name = codec_mpeg;
            retcd = ffmpeg_open(cnt->ffmpeg_timelapse);
        }

        if (retcd < 0){
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, "ffopen_open error creating (timelapse) file [%s]", cnt->timelapsefilename);
            free(cnt->ffmpeg_timelapse);
            cnt->ffmpeg_timelapse = NULL;
            return;
        }
        event(cnt, EVENT_FILECREATE, NULL, cnt->timelapsefilename, (void *)FTYPE_MPEG_TIMELAPSE, NULL);
    }

    if (ffmpeg_put_image(cnt->ffmpeg_timelapse, img_data, currenttime_tv) == -1) {
        MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, "Error encoding image");
    }

}

static void event_ffmpeg_put(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *img_data, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct timeval *currenttime_tv)
{
    if (cnt->ffmpeg_output) {
        if (ffmpeg_put_image(cnt->ffmpeg_output, img_data, currenttime_tv) == -1){
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, "Error encoding image");
        }
    }
    if (cnt->ffmpeg_output_debug) {
        if (ffmpeg_put_image(cnt->ffmpeg_output_debug, &cnt->imgs.img_motion, currenttime_tv) == -1) {
            MOTION_LOG(ERR, TYPE_EVENTS, NO_ERRNO, "Error encoding image");
        }
    }
}

static void event_ffmpeg_closefile(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct timeval *tv1 ATTRIBUTE_UNUSED)
{

    if (cnt->ffmpeg_output) {
        ffmpeg_close(cnt->ffmpeg_output);
        free(cnt->ffmpeg_output);
        cnt->ffmpeg_output = NULL;
        event(cnt, EVENT_FILECLOSE, NULL, cnt->newfilename, (void *)FTYPE_MPEG, NULL);
    }

    if (cnt->ffmpeg_output_debug) {
        ffmpeg_close(cnt->ffmpeg_output_debug);
        free(cnt->ffmpeg_output_debug);
        cnt->ffmpeg_output_debug = NULL;
        event(cnt, EVENT_FILECLOSE, NULL, cnt->motionfilename, (void *)FTYPE_MPEG_MOTION, NULL);
    }

}

static void event_ffmpeg_timelapseend(struct context *cnt,
            motion_event type ATTRIBUTE_UNUSED,
            struct image_data *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct timeval *tv1 ATTRIBUTE_UNUSED)
{
    if (cnt->ffmpeg_timelapse) {
        ffmpeg_close(cnt->ffmpeg_timelapse);
        free(cnt->ffmpeg_timelapse);
        cnt->ffmpeg_timelapse = NULL;
        event(cnt, EVENT_FILECLOSE, NULL, cnt->timelapsefilename, (void *)FTYPE_MPEG_TIMELAPSE, NULL);
    }
}



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
#if defined(HAVE_MYSQL) || defined(HAVE_PGSQL) || defined(HAVE_SQLITE3)
    {
    EVENT_FIRSTMOTION,
    event_sqlfirstmotion
    },
#endif
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
#if defined(HAVE_V4L2) && !defined(__FreeBSD__)
    {
    EVENT_IMAGE,
    event_vlp_putpipe
    },
    {
    EVENT_IMAGEM,
    event_vlp_putpipe
    },
#endif /* defined(HAVE_V4L2) && !__FreeBSD__  */
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
 *      - img_data - A pointer to a image_data context used for images
 *      - filename - A pointer to typically a string for a file path
 *      - eventdata - A void pointer that can be cast to anything. E.g. FTYPE_...
 *      - tm - A tm struct that carries a full time structure
 * The split between unsigned images and signed filenames was introduced in 3.2.2
 * as a code reading friendly solution to avoid a stream of compiler warnings in gcc 4.0.
 */
void event(struct context *cnt, motion_event type, struct image_data *img_data,
           char *filename, void *eventdata, struct timeval *tv1)
{
    int i=-1;

    while (event_handlers[++i].handler) {
        if (type == event_handlers[i].type)
            event_handlers[i].handler(cnt, type, img_data, filename, eventdata, tv1);
    }
}
