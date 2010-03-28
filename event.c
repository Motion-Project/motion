/*
    event.c

    Generalised event handling for motion

    Copyright Jeroen Vreeken, 2002
    This software is distributed under the GNU Public License Version 2
    see also the file 'COPYING'.

*/

#include "ffmpeg.h"    /* must be first to avoid 'shadow' warning */
#include "picture.h"    /* already includes motion.h */
#include "event.h"
#if !defined(BSD) 
#include "video.h"
#endif

/*
 *    Various functions (most doing the actual action)
 */

/* Execute 'command' with 'arg' as its argument.
 * if !arg command is started with no arguments
 * Before we call execl we need to close all the file handles
 * that the fork inherited from the parent in order not to pass
 * the open handles on to the shell
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
        motion_log(LOG_ERR, 1, "Unable to start external command '%s'", stamp);

        exit(1);
    } else if (cnt->conf.setup_mode) {
        motion_log(-1, 0, "Executing external command '%s'", stamp);
    }    
}

/* 
 *    Event handlers
 */

static void event_newfile(struct context *cnt ATTRIBUTE_UNUSED,
            int type ATTRIBUTE_UNUSED, unsigned char *dummy ATTRIBUTE_UNUSED,
            char *filename, void *ftype, struct tm *tm ATTRIBUTE_UNUSED)
{
    motion_log(-1, 0, "File of type %ld saved to: %s", (unsigned long)ftype, filename);
}


static void event_beep(struct context *cnt, int type ATTRIBUTE_UNUSED,
            unsigned char *dummy ATTRIBUTE_UNUSED,
            char *filename ATTRIBUTE_UNUSED,
            void *ftype ATTRIBUTE_UNUSED,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    if (!cnt->conf.quiet)
        printf("\a");
}

/* on_picture_save_command handles both on_picture_save and on_movie_start
 * If arg = FTYPE_IMAGE_ANY on_picture_save script is executed
 * If arg = FTYPE_MPEG_ANY on_movie_start script is executed
 * The scripts are executed with the filename of picture or movie appended
 * to the config parameter.
 */
static void on_picture_save_command(struct context *cnt,
            int type ATTRIBUTE_UNUSED, unsigned char *dummy ATTRIBUTE_UNUSED,
            char *filename, void *arg, struct tm *tm ATTRIBUTE_UNUSED)
{
    int filetype = (unsigned long)arg;    

    if ((filetype & FTYPE_IMAGE_ANY) != 0 && cnt->conf.on_picture_save)
        exec_command(cnt, cnt->conf.on_picture_save, filename, filetype);

    if ((filetype & FTYPE_MPEG_ANY) != 0 && cnt->conf.on_movie_start)
        exec_command(cnt, cnt->conf.on_movie_start, filename, filetype);
}

static void on_motion_detected_command(struct context *cnt,
            int type ATTRIBUTE_UNUSED, unsigned char *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    if (cnt->conf.on_motion_detected)
        exec_command(cnt, cnt->conf.on_motion_detected, NULL, 0);
}

#if defined(HAVE_MYSQL) || defined(HAVE_PGSQL)

static void event_sqlnewfile(struct context *cnt, int type  ATTRIBUTE_UNUSED,
            unsigned char *dummy ATTRIBUTE_UNUSED,
            char *filename, void *arg, struct tm *tm ATTRIBUTE_UNUSED)
{
    int sqltype = (unsigned long)arg;

    /* Only log the file types we want */
    if (!(cnt->conf.mysql_db || cnt->conf.pgsql_db) || (sqltype & cnt->sql_mask) == 0) 
        return;

    /* We place the code in a block so we only spend time making space in memory
     * for the sqlquery and timestr when we actually need it.
     */
    {
        char sqlquery[PATH_MAX];
    
        mystrftime(cnt, sqlquery, sizeof(sqlquery), cnt->conf.sql_query, 
                   &cnt->current_image->timestamp_tm, filename, sqltype);
        
#ifdef HAVE_MYSQL
        if (cnt->conf.mysql_db) {
            int ret;

            ret = mysql_query(cnt->database, sqlquery);

            if (ret != 0) {
                int error_code = mysql_errno(cnt->database);
                
                motion_log(LOG_ERR, 1, "Mysql query failed %s error code %d",
                           mysql_error(cnt->database), error_code);
                /* Try to reconnect ONCE if fails continue and discard this sql query */
                if (error_code >= 2000) {
                    cnt->database = (MYSQL *) mymalloc(sizeof(MYSQL));
                    mysql_init(cnt->database);

                    if (!mysql_real_connect(cnt->database, cnt->conf.mysql_host, 
                         cnt->conf.mysql_user, cnt->conf.mysql_password, 
                         cnt->conf.mysql_db, 0, NULL, 0)) {
                        motion_log(LOG_ERR, 0, "Cannot reconnect to MySQL database %s on host %s with user %s", 
                                   cnt->conf.mysql_db, cnt->conf.mysql_host, cnt->conf.mysql_user);
                        motion_log(LOG_ERR, 0, "MySQL error was %s", mysql_error(cnt->database));
                    } else { 
                        mysql_query(cnt->database, sqlquery);
                    }    
                }    
                
            }    
        }
#endif /* HAVE_MYSQL */

#ifdef HAVE_PGSQL
        if (cnt->conf.pgsql_db) {
            PGresult *res;

            res = PQexec(cnt->database_pg, sqlquery);

            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                motion_log(LOG_ERR, 1, "PGSQL query failed");
                PQclear(res);
            }
        }
#endif /* HAVE_PGSQL */

    }
}

#endif /* defined HAVE_MYSQL || defined HAVE_PGSQL */

static void on_area_command(struct context *cnt, int type ATTRIBUTE_UNUSED,
            unsigned char *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    if (cnt->conf.on_area_detected)
        exec_command(cnt, cnt->conf.on_area_detected, NULL, 0);
}

static void on_event_start_command(struct context *cnt, int type ATTRIBUTE_UNUSED,
            unsigned char *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    if (cnt->conf.on_event_start)
        exec_command(cnt, cnt->conf.on_event_start, NULL, 0);
}

static void on_event_end_command(struct context *cnt, int type ATTRIBUTE_UNUSED,
            unsigned char *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    if (cnt->conf.on_event_end)
        exec_command(cnt, cnt->conf.on_event_end, NULL, 0);
}

static void event_stop_webcam(struct context *cnt, int type ATTRIBUTE_UNUSED,
            unsigned char *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    if ((cnt->conf.webcam_port) && (cnt->webcam.socket != -1))
        webcam_stop(cnt);
    
}

static void event_webcam_put(struct context *cnt, int type ATTRIBUTE_UNUSED,
            unsigned char *img, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *tm ATTRIBUTE_UNUSED)
{
    if (cnt->conf.webcam_port)
        webcam_put(cnt, img);
}

#if !defined(WITHOUT_V4L) && !defined(BSD)
static void event_vid_putpipe(struct context *cnt, int type ATTRIBUTE_UNUSED,
            unsigned char *img, char *dummy ATTRIBUTE_UNUSED, void *devpipe,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    if (*(int *)devpipe >= 0) {
        if (vid_putpipe(*(int *)devpipe, img, cnt->imgs.size) == -1)
            motion_log(LOG_ERR, 1, "Failed to put image into video pipe");
    }
}
#endif /* WITHOUT_V4L && !BSD */


const char *imageext(struct context *cnt)
{
    if (cnt->conf.ppm)
        return "ppm";
    return "jpg";
}

static void event_image_detect(struct context *cnt, int type ATTRIBUTE_UNUSED,
        unsigned char *newimg, char *dummy1 ATTRIBUTE_UNUSED,
        void *dummy2 ATTRIBUTE_UNUSED, struct tm *currenttime_tm)
{
    char fullfilename[PATH_MAX];
    char filename[PATH_MAX];

    if (cnt->new_img & NEWIMG_ON) {
        const char *jpegpath;

        /* conf.jpegpath would normally be defined but if someone deleted it by control interface
           it is better to revert to the default than fail */
        if (cnt->conf.jpegpath)
            jpegpath = cnt->conf.jpegpath;
        else
            jpegpath = DEF_JPEGPATH;
            
        mystrftime(cnt, filename, sizeof(filename), jpegpath, currenttime_tm, NULL, 0);
        snprintf(fullfilename, PATH_MAX, "%s/%s.%s", cnt->conf.filepath, filename, imageext(cnt));
        put_picture(cnt, fullfilename, newimg, FTYPE_IMAGE);
    }
}

static void event_imagem_detect(struct context *cnt, int type ATTRIBUTE_UNUSED,
            unsigned char *newimg ATTRIBUTE_UNUSED, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *currenttime_tm)
{
    struct config *conf=&cnt->conf;
    char fullfilenamem[PATH_MAX];
    char filename[PATH_MAX];
    char filenamem[PATH_MAX];

    if (conf->motion_img) {
        const char *jpegpath;

        /* conf.jpegpath would normally be defined but if someone deleted it by control interface
           it is better to revert to the default than fail */
        if (cnt->conf.jpegpath)
            jpegpath = cnt->conf.jpegpath;
        else
            jpegpath = DEF_JPEGPATH;
            
        mystrftime(cnt, filename, sizeof(filename), jpegpath, currenttime_tm, NULL, 0);
        /* motion images gets same name as normal images plus an appended 'm' */
        snprintf(filenamem, PATH_MAX, "%sm", filename);
        snprintf(fullfilenamem, PATH_MAX, "%s/%s.%s", cnt->conf.filepath, filenamem, imageext(cnt));

        put_picture(cnt, fullfilenamem, cnt->imgs.out, FTYPE_IMAGE_MOTION);
    }
}

static void event_image_snapshot(struct context *cnt, int type ATTRIBUTE_UNUSED,
            unsigned char *img, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *currenttime_tm)
{
    char fullfilename[PATH_MAX];

    if (strcmp(cnt->conf.snappath, "lastsnap")) {
        char filename[PATH_MAX];
        char filepath[PATH_MAX];
        char linkpath[PATH_MAX];
        const char *snappath;
        /* conf.snappath would normally be defined but if someone deleted it by control interface
           it is better to revert to the default than fail */
        if (cnt->conf.snappath)
            snappath = cnt->conf.snappath;
        else
            snappath = DEF_SNAPPATH;
            
        mystrftime(cnt, filepath, sizeof(filepath), snappath, currenttime_tm, NULL, 0);
        snprintf(filename, PATH_MAX, "%s.%s", filepath, imageext(cnt));
        snprintf(fullfilename, PATH_MAX, "%s/%s", cnt->conf.filepath, filename);
        put_picture(cnt, fullfilename, img, FTYPE_IMAGE_SNAPSHOT);

        /* Update symbolic link *after* image has been written so that
           the link always points to a valid file. */
        snprintf(linkpath, PATH_MAX, "%s/lastsnap.%s", cnt->conf.filepath, imageext(cnt));
        remove(linkpath);

        if (symlink(filename, linkpath)) {
            motion_log(LOG_ERR, 1, "Could not create symbolic link [%s]", filename);
            return;
        }
    } else {
        snprintf(fullfilename, PATH_MAX, "%s/lastsnap.%s", cnt->conf.filepath, imageext(cnt));
        remove(fullfilename);
        put_picture(cnt, fullfilename, img, FTYPE_IMAGE_SNAPSHOT);
    }

    cnt->snapshot = 0;
}

static void event_camera_lost(struct context *cnt, int type ATTRIBUTE_UNUSED,
            unsigned char *img ATTRIBUTE_UNUSED, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *currenttime_tm ATTRIBUTE_UNUSED)
{
    if (cnt->conf.on_camera_lost)
        exec_command(cnt, cnt->conf.on_camera_lost, NULL, 0);
}

#ifdef HAVE_FFMPEG
static void grey2yuv420p(unsigned char *u, unsigned char *v, int width, int height)
{
    memset(u, 128, width * height / 4);
    memset(v, 128, width * height / 4);
}

static void on_movie_end_command(struct context *cnt, int type ATTRIBUTE_UNUSED,
                                 unsigned char *dummy ATTRIBUTE_UNUSED, char *filename,
                                 void *arg, struct tm *tm ATTRIBUTE_UNUSED)
{
    int filetype = (unsigned long) arg;

    if ((filetype & FTYPE_MPEG_ANY) && cnt->conf.on_movie_end)
        exec_command(cnt, cnt->conf.on_movie_end, filename, filetype);
}

static void event_ffmpeg_newfile(struct context *cnt, int type ATTRIBUTE_UNUSED,
            unsigned char *img, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *currenttime_tm)
{
    int width = cnt->imgs.width;
    int height = cnt->imgs.height;
    unsigned char *convbuf, *y, *u, *v;
    int fps = 0;
    char stamp[PATH_MAX];
    const char *mpegpath;

    if (!cnt->conf.ffmpeg_cap_new && !cnt->conf.ffmpeg_cap_motion)
        return;
        
    /* conf.mpegpath would normally be defined but if someone deleted it by control interface
       it is better to revert to the default than fail */
    if (cnt->conf.mpegpath)
        mpegpath = cnt->conf.mpegpath;
    else
        mpegpath = DEF_MPEGPATH;

    mystrftime(cnt, stamp, sizeof(stamp), mpegpath, currenttime_tm, NULL, 0);

    /* motion mpegs get the same name as normal mpegs plus an appended 'm' */
    /* PATH_MAX - 4 to allow for .mpg to be appended without overflow */
    snprintf(cnt->motionfilename, PATH_MAX - 4, "%s/%sm", cnt->conf.filepath, stamp);
    snprintf(cnt->newfilename, PATH_MAX - 4, "%s/%s", cnt->conf.filepath, stamp);

    if (cnt->conf.ffmpeg_cap_new) {
        if (cnt->imgs.type == VIDEO_PALETTE_GREY) {
            convbuf=mymalloc((width * height) / 2);
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

        fps = cnt->lastrate;

        if (debug_level >= CAMERA_DEBUG) 
            motion_log(LOG_DEBUG, 0, "%s FPS %d",__FUNCTION__, fps);

        if (fps > 30)
            fps = 30;
        else if (fps < 2)
            fps = 2;

        if ((cnt->ffmpeg_new =
             ffmpeg_open((char *)cnt->conf.ffmpeg_video_codec, cnt->newfilename, y, u, v,
                         cnt->imgs.width, cnt->imgs.height, fps, cnt->conf.ffmpeg_bps,
                         cnt->conf.ffmpeg_vbr)) == NULL) {
            motion_log(LOG_ERR, 1, "ffopen_open error creating (new) file [%s]",cnt->newfilename);
            cnt->finish = 1;
            return;
        }
        ((struct ffmpeg *)cnt->ffmpeg_new)->udata=convbuf;
        event(cnt, EVENT_FILECREATE, NULL, cnt->newfilename, (void *)FTYPE_MPEG, NULL);
    }

    if (cnt->conf.ffmpeg_cap_motion) {
        if (cnt->imgs.type == VIDEO_PALETTE_GREY) {
            convbuf = mymalloc((width * height) / 2);
            y = cnt->imgs.out;
            u = convbuf;
            v = convbuf + (width * height) / 4;
            grey2yuv420p(u, v, width, height);
        } else {
            y = cnt->imgs.out;
            u = cnt->imgs.out + width * height;
            v = u + (width * height) / 4;
            convbuf = NULL;
        }

        if (debug_level >= CAMERA_DEBUG) 
            motion_log(LOG_DEBUG, 0, "%s FPS %d", __FUNCTION__, fps);

        fps = cnt->lastrate;

        if (fps > 30)
            fps = 30;
        else if (fps < 2)
            fps = 2;

        if ((cnt->ffmpeg_motion =
             ffmpeg_open((char *)cnt->conf.ffmpeg_video_codec, cnt->motionfilename, y, u, v,
                         cnt->imgs.width, cnt->imgs.height, fps, cnt->conf.ffmpeg_bps,
                         cnt->conf.ffmpeg_vbr)) == NULL) {
            motion_log(LOG_ERR, 1, "ffopen_open error creating (motion) file [%s]", cnt->motionfilename);
            cnt->finish = 1;
            return;
        }
        cnt->ffmpeg_motion->udata = convbuf;
        event(cnt, EVENT_FILECREATE, NULL, cnt->motionfilename, (void *)FTYPE_MPEG_MOTION, NULL);
    }
}

static void event_ffmpeg_timelapse(struct context *cnt,
            int type ATTRIBUTE_UNUSED, unsigned char *img,
            char *dummy1 ATTRIBUTE_UNUSED, void *dummy2 ATTRIBUTE_UNUSED,
            struct tm *currenttime_tm)
{
    int width = cnt->imgs.width;
    int height = cnt->imgs.height;
    unsigned char *convbuf, *y, *u, *v;

    if (!cnt->ffmpeg_timelapse) {
        char tmp[PATH_MAX];
        const char *timepath;

        /* conf.timepath would normally be defined but if someone deleted it by control interface
           it is better to revert to the default than fail */
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
            v = convbuf+(width * height) / 4;
            grey2yuv420p(u, v, width, height);
        } else {
            convbuf = NULL;
            y = img;
            u = img + width * height;
            v = u + (width * height) / 4;
        }
        
        if ((cnt->ffmpeg_timelapse =
             ffmpeg_open((char *)TIMELAPSE_CODEC, cnt->timelapsefilename, y, u, v,
                         cnt->imgs.width, cnt->imgs.height, 24, cnt->conf.ffmpeg_bps,
                         cnt->conf.ffmpeg_vbr)) == NULL) {
            motion_log(LOG_ERR, 1, "ffopen_open error creating (timelapse) file [%s]", cnt->timelapsefilename);
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
    ffmpeg_put_other_image(cnt->ffmpeg_timelapse, y, u, v);
    
}

static void event_ffmpeg_put(struct context *cnt, int type ATTRIBUTE_UNUSED,
            unsigned char *img, char *dummy1 ATTRIBUTE_UNUSED,
            void *dummy2 ATTRIBUTE_UNUSED, struct tm *tm ATTRIBUTE_UNUSED)
{
    if (cnt->ffmpeg_new) {
        int width=cnt->imgs.width;
        int height=cnt->imgs.height;
        unsigned char *y = img;
        unsigned char *u, *v;
        
        if (cnt->imgs.type == VIDEO_PALETTE_GREY)
            u = cnt->ffmpeg_timelapse->udata;
        else
            u = y + (width * height);
        
        v = u + (width * height) / 4;
        ffmpeg_put_other_image(cnt->ffmpeg_new, y, u, v);
    }
    
    if (cnt->ffmpeg_motion) 
        ffmpeg_put_image(cnt->ffmpeg_motion);
    
}

static void event_ffmpeg_closefile(struct context *cnt,
            int type ATTRIBUTE_UNUSED, unsigned char *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    
    if (cnt->ffmpeg_new) {
        if (cnt->ffmpeg_new->udata)
            free(cnt->ffmpeg_new->udata);
        ffmpeg_close(cnt->ffmpeg_new);
        cnt->ffmpeg_new = NULL;

        event(cnt, EVENT_FILECLOSE, NULL, cnt->newfilename, (void *)FTYPE_MPEG, NULL);
    }

    if (cnt->ffmpeg_motion) {
        if (cnt->ffmpeg_motion->udata)
            free(cnt->ffmpeg_motion->udata);

        ffmpeg_close(cnt->ffmpeg_motion);
        cnt->ffmpeg_motion = NULL;

        event(cnt, EVENT_FILECLOSE, NULL, cnt->motionfilename, (void *)FTYPE_MPEG_MOTION, NULL);
    }
}

static void event_ffmpeg_timelapseend(struct context *cnt,
            int type ATTRIBUTE_UNUSED, unsigned char *dummy1 ATTRIBUTE_UNUSED,
            char *dummy2 ATTRIBUTE_UNUSED, void *dummy3 ATTRIBUTE_UNUSED,
            struct tm *tm ATTRIBUTE_UNUSED)
{
    if (cnt->ffmpeg_timelapse) {
        if (cnt->ffmpeg_timelapse->udata)
            free(cnt->ffmpeg_timelapse->udata);

        ffmpeg_close(cnt->ffmpeg_timelapse);
        cnt->ffmpeg_timelapse = NULL;

        event(cnt, EVENT_FILECLOSE, NULL, cnt->timelapsefilename, (void *)FTYPE_MPEG_TIMELAPSE, NULL);
    }
}

#endif /* HAVE_FFMPEG */


/*  
 *    Starting point for all events
 */

struct event_handlers {
    int type;
    event_handler handler;
};

struct event_handlers event_handlers[] = {
#if defined(HAVE_MYSQL) || defined(HAVE_PGSQL) 
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
#if !defined(WITHOUT_V4L) && !defined(BSD)
    {
    EVENT_IMAGE | EVENT_IMAGEM,
    event_vid_putpipe
    },
#endif /* WITHOUT_V4L && !BSD */
    {
    EVENT_WEBCAM,
    event_webcam_put
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
#endif /* HAVE_FFMPEG */
    {
    EVENT_CAMERA_LOST,
    event_camera_lost
    },
    {
    EVENT_STOP,
    event_stop_webcam
    },
    {0, NULL}
};


/* The event functions are defined with the following parameters:
 * - Type as defined in event.h (EVENT_...)
 * - The global context struct cnt
 * - image - A pointer to unsigned char as used for images
 * - filename - A pointer to typically a string for a file path
 * - eventdata - A void pointer that can be cast to anything. E.g. FTYPE_...
 * - tm - A tm struct that carries a full time structure
 * The split between unsigned images and signed filenames was introduced in 3.2.2
 * as a code reading friendly solution to avoid a stream of compiler warnings in gcc 4.0.
 */
void event(struct context *cnt, int type, unsigned char *image, char *filename, void *eventdata, struct tm *tm)
{
    int i = -1;

    while (event_handlers[++i].handler) {
        if (type & event_handlers[i].type)
            event_handlers[i].handler(cnt, type, image, filename, eventdata, tm);
    }
}
