/*    motion.c
 *
 *    Detect changes in a video stream.
 *    Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */
#include "ffmpeg.h"
#include "motion.h"

#if (defined(BSD) && !defined(PWCBSD))
#include "video_freebsd.h"
#else
#include "video.h"
#endif /* BSD */

#include "conf.h"
#include "alg.h"
#include "track.h"
#include "event.h"
#include "picture.h"
#include "rotate.h"

/* Forward declarations */
static int motion_init(struct context *cnt);
static void motion_cleanup(struct context *cnt);


/**
 * tls_key_threadnr
 *
 *   TLS key for storing thread number in thread-local storage.
 */
pthread_key_t tls_key_threadnr;

/**
 * global_lock
 *
 *   Protects any global variables (like 'threads_running') during updates,
 *   to prevent problems with multiple threads updating at the same time.
 */
//pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_lock;

/**
 * cnt_list
 *
 *   List of context structures, one for each main Motion thread.
 */
struct context **cnt_list = NULL;

/**
 * threads_running
 *
 *   Keeps track of number of Motion threads currently running. Also used
 *   by 'main' to know when all threads have exited.
 */
volatile int threads_running = 0;

/*
 * debug_level is for developers, normally used to control which
 * types of messages get output.
 */
unsigned short int debug_level;

/* Set this when we want main to end or restart
 */
volatile unsigned short int finish = 0;

/**
 * restart
 *
 *   Differentiates between a quit and a restart. When all threads have
 *   finished running, 'main' checks if 'restart' is true and if so starts
 *   up again (instead of just quitting).
 */
unsigned short int restart = 0;

/**
 * image_ring_resize
 *
 * This routine is called from motion_loop to resize the image precapture ringbuffer
 * NOTE: This function clears all images in the old ring buffer
 
 * Parameters:
 *
 *      cnt      Pointer to the motion context structure
 *      new_size The new size of the ring buffer
 *
 * Returns:     nothing
 */
static void image_ring_resize(struct context *cnt, int new_size)
{
    /* Only resize if :
     * Not in an event and
     * decreasing at last position in new buffer
     * increasing at last position in old buffer 
     * e.g. at end of smallest buffer */
    if (cnt->event_nr != cnt->prev_event) {
        int smallest;
        
        if (new_size < cnt->imgs.image_ring_size) { /* Decreasing */
            smallest = new_size;
        } else { /* Increasing */
            smallest = cnt->imgs.image_ring_size;
        }

        if (cnt->imgs.image_ring_in == smallest - 1 || smallest == 0) {
            motion_log(LOG_INFO, 0, "Resizing pre_capture buffer to %d items", new_size);

            /* Create memory for new ring buffer */
            struct image_data *tmp;
            tmp = mymalloc(new_size * sizeof(struct image_data));

            /* Copy all information from old to new 
             * Smallest is 0 at initial init */
            if (smallest > 0) 
                memcpy(tmp, cnt->imgs.image_ring, sizeof(struct image_data) * smallest);
            

            /* In the new buffers, allocate image memory */
            {
                int i;
                for(i = smallest; i < new_size; i++) {
                    tmp[i].image = mymalloc(cnt->imgs.size);
                    memset(tmp[i].image, 0x80, cnt->imgs.size);  /* initialize to grey */
                }
            }
            
            /* Free the old ring */
            free(cnt->imgs.image_ring);

            /* Point to the new ring */
            cnt->imgs.image_ring = tmp;

            cnt->imgs.image_ring_size = new_size;
        }
    }
}

/**
 * image_ring_destroy
 *
 * This routine is called when we want to free the ring
 *
 * Parameters:
 *
 *      cnt      Pointer to the motion context structure
 *
 * Returns:     nothing
 */
static void image_ring_destroy(struct context *cnt)
{
    unsigned short int i;
    
    /* Exit if don't have any ring */
    if (cnt->imgs.image_ring == NULL)
        return;

    /* Free all image buffers */
    for (i = 0; i < cnt->imgs.image_ring_size; i++) 
        free(cnt->imgs.image_ring[i].image);
    
    
    /* Free the ring */
    free(cnt->imgs.image_ring);

    cnt->imgs.image_ring = NULL;
    cnt->imgs.image_ring_size = 0;
}

/**
 * image_save_as_preview
 *
 * This routine is called when we detect motion and want to save a image in the preview buffer
 *
 * Parameters:
 *
 *      cnt      Pointer to the motion context structure
 *      img      Pointer to the image_data structure we want to set as preview image
 *
 * Returns:     nothing
 */
static void image_save_as_preview(struct context *cnt, struct image_data *img)
{
    void * image;
    /* Save preview image pointer */
    image = cnt->imgs.preview_image.image;
    /* Copy all info */
    memcpy(&cnt->imgs.preview_image.image, img, sizeof(struct image_data));
    /* restore image pointer */
    cnt->imgs.preview_image.image = image;

    /* Copy image */
    memcpy(cnt->imgs.preview_image.image, img->image, cnt->imgs.size);

    /* If we set output_all to yes and during the event
     * there is no image with motion, diffs is 0, we are not going to save the preview event */
    if (cnt->imgs.preview_image.diffs == 0)
        cnt->imgs.preview_image.diffs = 1;

    /* If we have locate on it is already done */
    if (cnt->locate == LOCATE_PREVIEW) 
        alg_draw_location(&img->location, &cnt->imgs, cnt->imgs.width, 
                          cnt->imgs.preview_image.image, LOCATE_NORMAL);
    
}

/**
 * context_init
 *
 *   Initializes a context struct with the default values for all the
 *   variables.
 *
 * Parameters:
 *
 *   cnt - the context struct to destroy
 *
 * Returns: nothing
 */
static void context_init (struct context *cnt)
{
    /*
    * We first clear the entire structure to zero, then fill in any
    * values which have non-zero default values.  Note that this
    * assumes that a NULL address pointer has a value of binary 0
    * (this is also assumed at other places within the code, i.e.
    * there are instances of "if (ptr)").  Just for possible future
    * changes to this assumption, any pointers which are intended
    * to be initialised to NULL are listed within a comment.
    */

    memset(cnt, 0, sizeof(struct context));
    cnt->noise = 255;
    cnt->lastrate = 25;

    memcpy(&cnt->track, &track_template, sizeof(struct trackoptions));
    cnt->pipe = -1;
    cnt->mpipe = -1;

}

/**
 * context_destroy
 *
 *   Destroys a context struct by freeing allocated memory, calling the
 *   appropriate cleanup functions and finally freeing the struct itself.
 *
 * Parameters:
 *
 *   cnt - the context struct to destroy
 *
 * Returns: nothing
 */
static void context_destroy(struct context *cnt)
{
    unsigned short int j;

    /* Free memory allocated for config parameters */
    for (j = 0; config_params[j].param_name != NULL; j++) {
        if (config_params[j].copy == copy_string) {
            void **val;
            val = (void *)((char *)cnt+(int)config_params[j].conf_value);
            if (*val) {
                free(*val);
                *val = NULL;
            }
        }
    }

    free(cnt);
}

/**
 * sig_handler
 *
 *  Our SIGNAL-Handler. We need this to handle alarms and external signals.
 */
static void sig_handler(int signo)
{
    short int i;

    switch(signo) {
    case SIGALRM:
        /* Somebody (maybe we ourself) wants us to make a snapshot
         * This feature triggers snapshots on ALL threads that have
         * snapshot_interval different from 0.
         */
        if (cnt_list) {
            i = -1;
            while (cnt_list[++i]) {
                if (cnt_list[i]->conf.snapshot_interval) 
                    cnt_list[i]->snapshot = 1;
                    
            }
        }
        break;
    case SIGUSR1:
        /* Ouch! We have been hit from the outside! Someone wants us to
           make a movie! */
        if (cnt_list) {
            i = -1;
            while (cnt_list[++i])
                cnt_list[i]->makemovie = 1;
        }
        break;
    case SIGHUP:
        restart = 1;
        /* Fall through, as the value of 'restart' is the only difference
         * between SIGHUP and the ones below.
         */
    case SIGINT:
    case SIGQUIT:
    case SIGTERM:
        /* Somebody wants us to quit! We should better finish the actual
           movie and end up! */
        if (cnt_list) {
            i = -1;
            while (cnt_list[++i]) {
                cnt_list[i]->makemovie = 1;
                cnt_list[i]->finish = 1;
                /* don't restart thread when it ends, 
                 * all threads restarts if global restart is set 
                 */
                cnt_list[i]->restart = 0;
            }
        }
        /* Set flag we want to quit main check threads loop
         * if restart is set (above) we start up again */
        finish = 1;
        break;
    case SIGSEGV:
        exit(0);
    }
}

/**
 * sigchild_handler
 *
 *   This function is a POSIX compliant replacement of the commonly used
 *   signal(SIGCHLD, SIG_IGN).
 */
static void sigchild_handler(int signo ATTRIBUTE_UNUSED)
{
#ifdef WNOHANG
    while (waitpid(-1, NULL, WNOHANG) > 0) {};
#endif /* WNOHANG */
    return;
}

/**
 *  motion_remove_pid
 *
 *  This function remove the process id file ( pid file ) before motion exit.
 *
 */
static void motion_remove_pid(void)
{
    if ((cnt_list[0]->daemon) && (cnt_list[0]->conf.pid_file) && (restart == 0)) {
        if (!unlink(cnt_list[0]->conf.pid_file)) 
            motion_log(LOG_INFO, 0, "Removed process id file (pid file).");
        else 
            motion_log(LOG_INFO, 1, "Error removing pid file");
    }
}

/**
 * motion_detected
 *
 *   Called from 'motion_loop' when motion is detected
 *   Can be called when no motion if output_all is set!
 *
 * Parameters:
 *
 *   cnt      - current thread's context struct
 *   dev      - video device file descriptor
 *   img      - pointer to the captured image_data with detected motion
 */
static void motion_detected(struct context *cnt, int dev, struct image_data *img)
{
    struct config *conf = &cnt->conf;
    struct images *imgs = &cnt->imgs;
    struct coord *location = &img->location;

    /* Draw location */
    if (cnt->locate == LOCATE_ON)
        alg_draw_location(location, imgs, imgs->width, img->image, LOCATE_BOTH);

    /* Calculate how centric motion is if configured preview center*/
    if (cnt->new_img & NEWIMG_CENTER) {
        unsigned int distX = abs((imgs->width/2) - location->x);
        unsigned int distY = abs((imgs->height/2) - location->y);
        img->cent_dist = distX*distX + distY*distY;
    }


    /* Do things only if we have got minimum_motion_frames */
    if (img->flags & IMAGE_TRIGGER) {
        /* Take action if this is a new event and we have a trigger image */
        if (cnt->event_nr != cnt->prev_event) {
            /* Reset prev_event number to current event and save event time
             * in both time_t and struct tm format.
             */
            cnt->prev_event = cnt->event_nr;
            cnt->eventtime = img->timestamp;
            localtime_r(&cnt->eventtime, cnt->eventtime_tm);

            /* Since this is a new event we create the event_text_string used for
             * the %C conversion specifier. We may already need it for
             * on_motion_detected_commend so it must be done now.
             */
            mystrftime(cnt, cnt->text_event_string, sizeof(cnt->text_event_string),
                       cnt->conf.text_event, cnt->eventtime_tm, NULL, 0);

            /* EVENT_FIRSTMOTION triggers on_event_start_command and event_ffmpeg_newfile */
            event(cnt, EVENT_FIRSTMOTION, img->image, NULL, NULL, &img->timestamp_tm);

            if (cnt->conf.setup_mode)
                motion_log(-1, 0, "Motion detected - starting event %d", cnt->event_nr);

            /* always save first motion frame as preview-shot, may be changed to an other one later */
            if (cnt->new_img & (NEWIMG_FIRST | NEWIMG_BEST | NEWIMG_CENTER)) 
                image_save_as_preview(cnt, img);
            
        }

        /* EVENT_MOTION triggers event_beep and on_motion_detected_command */
        event(cnt, EVENT_MOTION, NULL, NULL, NULL, &img->timestamp_tm);
    }

    /* Limit framerate */
    if (img->shot < conf->frame_limit) {
        /* If config option webcam_motion is enabled, send the latest motion detected image
         * to the webcam but only if it is not the first shot within a second. This is to
         * avoid double frames since we already have sent a frame to the webcam.
         * We also disable this in setup_mode.
         */
        if (conf->webcam_motion && !conf->setup_mode && img->shot != 1) 
            event(cnt, EVENT_WEBCAM, img->image, NULL, NULL, &img->timestamp_tm);
        

        /* Save motion jpeg, if configured */
        /* Output the image_out (motion) picture. */
        if (conf->motion_img) 
            event(cnt, EVENT_IMAGEM_DETECTED, NULL, NULL, NULL, &img->timestamp_tm);
        
    }

    if (cnt->track.type) 
        cnt->moved = track_move(cnt, dev, location, imgs, 0);
    
}

/**
 * process_image_ring
 *
 *   Called from 'motion_loop' to save images / send images to movie
 *
 * Parameters:
 *
 *   cnt        - current thread's context struct
 *   max_images - Max number of images to process
 *                Set to IMAGE_BUFFER_FLUSH to send/save all images in buffer
 */
#define IMAGE_BUFFER_FLUSH ((unsigned int)-1)
static void process_image_ring(struct context *cnt, unsigned int max_images)
{
    /* we are going to send an event, in the events there is still
     * some code that use cnt->current_image
     * so set it temporary to our image */
    struct image_data *saved_current_image = cnt->current_image;

    /* If image is flaged to be saved and not saved yet, process it */
    do {
        /* Check if we should save/send this image, breakout if not */
        if ((cnt->imgs.image_ring[cnt->imgs.image_ring_out].flags & 
            (IMAGE_SAVE | IMAGE_SAVED)) != IMAGE_SAVE)
            break;

        /* Set inte global cotext that we are working with this image */
        cnt->current_image = &cnt->imgs.image_ring[cnt->imgs.image_ring_out];

        if (cnt->imgs.image_ring[cnt->imgs.image_ring_out].shot < cnt->conf.frame_limit) {
            /* Output the picture to jpegs and ffmpeg */
            event(cnt, EVENT_IMAGE_DETECTED,
                  cnt->imgs.image_ring[cnt->imgs.image_ring_out].image, NULL, NULL, 
                  &cnt->imgs.image_ring[cnt->imgs.image_ring_out].timestamp_tm);
        }

        /* Mark the image as saved */
        cnt->imgs.image_ring[cnt->imgs.image_ring_out].flags |= IMAGE_SAVED;

        /* Store it as a preview image, only if it have motion */
        if (cnt->imgs.image_ring[cnt->imgs.image_ring_out].flags & IMAGE_MOTION) {
        
            /* Check for most significant preview-shot when output_normal=best */
            if (cnt->new_img & NEWIMG_BEST) {
                if (cnt->imgs.image_ring[cnt->imgs.image_ring_out].diffs > cnt->imgs.preview_image.diffs) 
                    image_save_as_preview(cnt, &cnt->imgs.image_ring[cnt->imgs.image_ring_out]);
            }

            /* Check for most significant preview-shot when output_normal=center */
            if (cnt->new_img & NEWIMG_CENTER) {
                if (cnt->imgs.image_ring[cnt->imgs.image_ring_out].cent_dist < cnt->imgs.preview_image.cent_dist) 
                    image_save_as_preview(cnt, &cnt->imgs.image_ring[cnt->imgs.image_ring_out]);
            }
        }

        /* Increment to image after last sended */
        if (++cnt->imgs.image_ring_out >= cnt->imgs.image_ring_size)
            cnt->imgs.image_ring_out = 0;

        if (max_images != IMAGE_BUFFER_FLUSH) {
            max_images--;
            /* breakout if we have done max_images */
            if (max_images == 0)
                break;
        }

        /* loop until out and in is same e.g. buffer empty */
    } while (cnt->imgs.image_ring_out != cnt->imgs.image_ring_in);

    /* restore global context values */
    cnt->current_image = saved_current_image;
}

/**
 * motion_init
 *
 * This routine is called from motion_loop (the main thread of the program) to do
 * all of the initialisation required before starting the actual run.
 *
 * Parameters:
 *
 *      cnt     Pointer to the motion context structure
 *
 * Returns:     0 OK
 *             -1 Fatal error, open loopback error
 *             -2 Fatal error, open SQL database error
 */
static int motion_init(struct context *cnt)
{
    int i;
    FILE *picture;

    /* Store thread number in TLS. */
    pthread_setspecific(tls_key_threadnr, (void *)((unsigned long)cnt->threadnr));

    cnt->currenttime_tm = mymalloc(sizeof(struct tm));
    cnt->eventtime_tm = mymalloc(sizeof(struct tm));
    /* Init frame time */
    cnt->currenttime = time(NULL);
    localtime_r(&cnt->currenttime, cnt->currenttime_tm);

    cnt->smartmask_speed = 0;

    /* We initialize cnt->event_nr to 1 and cnt->prev_event to 0 (not really needed) so
     * that certain code below does not run until motion has been detected the first time */
    cnt->event_nr = 1;
    cnt->prev_event = 0;
    cnt->lightswitch_framecounter = 0;
    cnt->detecting_motion = 0;
    cnt->makemovie = 0;

    motion_log(LOG_DEBUG, 0, "Thread %d started", (unsigned long)pthread_getspecific(tls_key_threadnr));

    if (!cnt->conf.filepath)
        cnt->conf.filepath = strdup(".");

    /* set the device settings */
    cnt->video_dev = vid_start(cnt);

    /* We failed to get an initial image from a camera
     * So we need to guess height and width based on the config
     * file options.
     */
    if (cnt->video_dev < 0) {
        motion_log(LOG_ERR, 0, "Could not fetch initial image from camera");
        motion_log(LOG_ERR, 0, "Motion continues using width and height from config file(s)");
        cnt->imgs.width = cnt->conf.width;
        cnt->imgs.height = cnt->conf.height;
        cnt->imgs.size = cnt->conf.width * cnt->conf.height * 3 / 2;
        cnt->imgs.motionsize = cnt->conf.width * cnt->conf.height;
        cnt->imgs.type = VIDEO_PALETTE_YUV420P;
    }

    image_ring_resize(cnt, 1); /* Create a initial precapture ring buffer with 1 frame */

    cnt->imgs.ref = mymalloc(cnt->imgs.size);
    cnt->imgs.out = mymalloc(cnt->imgs.size);
    memset(cnt->imgs.out, 0, cnt->imgs.size);
    /* contains the moving objects of ref. frame */
    cnt->imgs.ref_dyn = mymalloc(cnt->imgs.motionsize * sizeof(cnt->imgs.ref_dyn));
    cnt->imgs.image_virgin = mymalloc(cnt->imgs.size);
    cnt->imgs.smartmask = mymalloc(cnt->imgs.motionsize);
    cnt->imgs.smartmask_final = mymalloc(cnt->imgs.motionsize);
    cnt->imgs.smartmask_buffer = mymalloc(cnt->imgs.motionsize * sizeof(cnt->imgs.smartmask_buffer));
    cnt->imgs.labels = mymalloc(cnt->imgs.motionsize * sizeof(cnt->imgs.labels));
    cnt->imgs.labelsize = mymalloc((cnt->imgs.motionsize/2+1) * sizeof(cnt->imgs.labelsize));

    /* allocate buffer here for preview buffer */
    cnt->imgs.preview_image.image = mymalloc(cnt->imgs.size);

    /* Allocate a buffer for temp. usage in some places */
    /* Only despeckle & bayer2rgb24() for now for now... */
    cnt->imgs.common_buffer = mymalloc(3 * cnt->imgs.width * cnt->imgs.height);

    /* Now is a good time to init rotation data. Since vid_start has been
     * called, we know that we have imgs.width and imgs.height. When capturing
     * from a V4L device, these are copied from the corresponding conf values
     * in vid_start. When capturing from a netcam, they get set in netcam_start,
     * which is called from vid_start.
     *
     * rotate_init will set cap_width and cap_height in cnt->rotate_data.
     */
    rotate_init(cnt); /* rotate_deinit is called in main */

    /* Capture first image, or we will get an alarm on start */
    if (cnt->video_dev > 0) {
        for (i = 0; i < 5; i++) {
            if (vid_next(cnt, cnt->imgs.image_virgin) == 0)
                break;
            SLEEP(2,0);
        }
        if (i >= 5) {
            memset(cnt->imgs.image_virgin, 0x80, cnt->imgs.size);       /* initialize to grey */
            draw_text(cnt->imgs.image_virgin, 10, 20, cnt->imgs.width,
                      "Error capturing first image", cnt->conf.text_double);
            motion_log(LOG_ERR, 0, "Error capturing first image");
        }
    }

    /* create a reference frame */
    alg_update_reference_frame(cnt, RESET_REF_FRAME);

#if !defined(WITHOUT_V4L) && !defined(BSD)
    /* open video loopback devices if enabled */
    if (cnt->conf.vidpipe) {
        if (cnt->conf.setup_mode)
            motion_log(-1, 0, "Opening video loopback device for normal pictures");

        /* vid_startpipe should get the output dimensions */
        cnt->pipe = vid_startpipe(cnt->conf.vidpipe, cnt->imgs.width, cnt->imgs.height, cnt->imgs.type);

        if (cnt->pipe < 0) {
            motion_log(LOG_ERR, 0, "Failed to open video loopback");
            return -1;
        }
    }
    if (cnt->conf.motionvidpipe) {
        if (cnt->conf.setup_mode)
            motion_log(-1, 0, "Opening video loopback device for motion pictures");

        /* vid_startpipe should get the output dimensions */
        cnt->mpipe = vid_startpipe(cnt->conf.motionvidpipe, cnt->imgs.width, cnt->imgs.height, cnt->imgs.type);

        if (cnt->mpipe < 0) {
            motion_log(LOG_ERR, 0, "Failed to open video loopback");
            return -1;
        }
    }
#endif /*WITHOUT_V4L && !BSD */

#ifdef HAVE_MYSQL
    if (cnt->conf.mysql_db) {
        cnt->database = (MYSQL *) mymalloc(sizeof(MYSQL));
        mysql_init(cnt->database);

        if (!mysql_real_connect(cnt->database, cnt->conf.mysql_host, cnt->conf.mysql_user,
            cnt->conf.mysql_password, cnt->conf.mysql_db, 0, NULL, 0)) {
            motion_log(LOG_ERR, 0, "Cannot connect to MySQL database %s on host %s with user %s",
                       cnt->conf.mysql_db, cnt->conf.mysql_host, cnt->conf.mysql_user);
            motion_log(LOG_ERR, 0, "MySQL error was %s", mysql_error(cnt->database));
            return -2;
        }
        #if (defined(MYSQL_VERSION_ID)) && (MYSQL_VERSION_ID > 50012)
        my_bool my_true = TRUE;
        mysql_options(cnt->database,MYSQL_OPT_RECONNECT,&my_true);
        #endif
    }
#endif /* HAVE_MYSQL */

#ifdef HAVE_PGSQL
    if (cnt->conf.pgsql_db) {
        char connstring[255];

        /* create the connection string.
           Quote the values so we can have null values (blank)*/
        snprintf(connstring, 255,
                 "dbname='%s' host='%s' user='%s' password='%s' port='%d'",
                 cnt->conf.pgsql_db, /* dbname */
                 (cnt->conf.pgsql_host ? cnt->conf.pgsql_host : ""), /* host (may be blank) */
                 (cnt->conf.pgsql_user ? cnt->conf.pgsql_user : ""), /* user (may be blank) */
                 (cnt->conf.pgsql_password ? cnt->conf.pgsql_password : ""), /* password (may be blank) */
                  cnt->conf.pgsql_port
        );

        cnt->database_pg = PQconnectdb(connstring);

        if (PQstatus(cnt->database_pg) == CONNECTION_BAD) {
            motion_log(LOG_ERR, 0, "Connection to PostgreSQL database '%s' failed: %s",
                       cnt->conf.pgsql_db, PQerrorMessage(cnt->database_pg));
            return -2;
        }
    }
#endif /* HAVE_PGSQL */


#if defined(HAVE_MYSQL) || defined(HAVE_PGSQL)
    /* Set the sql mask file according to the SQL config options*/

    cnt->sql_mask = cnt->conf.sql_log_image * (FTYPE_IMAGE + FTYPE_IMAGE_MOTION) +
                    cnt->conf.sql_log_snapshot * FTYPE_IMAGE_SNAPSHOT +
                    cnt->conf.sql_log_mpeg * (FTYPE_MPEG + FTYPE_MPEG_MOTION) +
                    cnt->conf.sql_log_timelapse * FTYPE_MPEG_TIMELAPSE;
#endif /* defined(HAVE_MYSQL) || defined(HAVE_PGSQL) */

    /* Load the mask file if any */
    if (cnt->conf.mask_file) {
        if ((picture = fopen(cnt->conf.mask_file, "r"))) {
            /* NOTE: The mask is expected to have the output dimensions. I.e., the mask
             * applies to the already rotated image, not the capture image. Thus, use
             * width and height from imgs.
             */
            cnt->imgs.mask = get_pgm(picture, cnt->imgs.width, cnt->imgs.height);
            fclose(picture);
        } else {
            motion_log(LOG_ERR, 1, "Error opening mask file %s", cnt->conf.mask_file);
            /* Try to write an empty mask file to make it easier
               for the user to edit it */
            put_fixed_mask(cnt, cnt->conf.mask_file);
        }

        if (!cnt->imgs.mask) {
            motion_log(LOG_ERR, 0, "Failed to read mask image. Mask feature disabled.");
        } else {
            if (cnt->conf.setup_mode)
                motion_log(-1, 0, "Maskfile \"%s\" loaded.",cnt->conf.mask_file);
        }

    } else {
        cnt->imgs.mask = NULL;
    }    

    /* Always initialize smart_mask - someone could turn it on later... */
    memset(cnt->imgs.smartmask, 0, cnt->imgs.motionsize);
    memset(cnt->imgs.smartmask_final, 255, cnt->imgs.motionsize);
    memset(cnt->imgs.smartmask_buffer, 0, cnt->imgs.motionsize*sizeof(cnt->imgs.smartmask_buffer));

    /* Set noise level */
    cnt->noise = cnt->conf.noise;

    /* Set threshold value */
    cnt->threshold = cnt->conf.max_changes;

    /* Initialize webcam server if webcam port is specified to not 0 */
    if (cnt->conf.webcam_port) {
        if (webcam_init(cnt) == -1) {
            motion_log(LOG_ERR, 1, "Problem enabling stream server in port %d", cnt->conf.webcam_port);
            cnt->finish = 1;
        } else {    
            motion_log(LOG_DEBUG, 0, "Started stream webcam server in port %d", cnt->conf.webcam_port);
        }    
    }

    /* Prevent first few frames from triggering motion... */
    cnt->moved = 8;
    /* 2 sec startup delay so FPS is calculated correct */
    cnt->startup_frames = cnt->conf.frame_limit * 2;

    return 0;
}

/**
 * motion_cleanup
 *
 * This routine is called from motion_loop when thread ends to
 * cleanup all memory etc. that motion_init did.
 *
 * Parameters:
 *
 *      cnt     Pointer to the motion context structure
 *
 * Returns:     nothing
 */
static void motion_cleanup(struct context *cnt)
{
    /* Stop webcam */
    event(cnt, EVENT_STOP, NULL, NULL, NULL, NULL);

    if (cnt->video_dev >= 0) {
        motion_log(LOG_DEBUG, 0, "Calling vid_close() from motion_cleanup");        
        vid_close(cnt);
    }

    if (cnt->imgs.out) {
        free(cnt->imgs.out);
        cnt->imgs.out = NULL;
    }

    if (cnt->imgs.ref) {
        free(cnt->imgs.ref);
        cnt->imgs.ref = NULL;
    }

    if (cnt->imgs.ref_dyn) {
        free(cnt->imgs.ref_dyn);
        cnt->imgs.ref_dyn = NULL;
    }

    if (cnt->imgs.image_virgin) {
        free(cnt->imgs.image_virgin);
        cnt->imgs.image_virgin = NULL;
    }

    if (cnt->imgs.labels) {
        free(cnt->imgs.labels);
        cnt->imgs.labels = NULL;
    }

    if (cnt->imgs.labelsize) {
        free(cnt->imgs.labelsize);
        cnt->imgs.labelsize = NULL;
    }

    if (cnt->imgs.smartmask) {
        free(cnt->imgs.smartmask);
        cnt->imgs.smartmask = NULL;
    }

    if (cnt->imgs.smartmask_final) {
        free(cnt->imgs.smartmask_final);
        cnt->imgs.smartmask_final = NULL;
    }

    if (cnt->imgs.smartmask_buffer) {
        free(cnt->imgs.smartmask_buffer);
        cnt->imgs.smartmask_buffer = NULL;
    }

    if (cnt->imgs.common_buffer) {
        free(cnt->imgs.common_buffer);
        cnt->imgs.common_buffer = NULL;
    }

    if (cnt->imgs.preview_image.image) {
        free(cnt->imgs.preview_image.image);
        cnt->imgs.preview_image.image = NULL;
    }

    image_ring_destroy(cnt); /* Cleanup the precapture ring buffer */

    rotate_deinit(cnt); /* cleanup image rotation data */

    if (cnt->pipe != -1) {
        close(cnt->pipe);
        cnt->pipe = -1;
    }

    if (cnt->mpipe != -1) {
        close(cnt->mpipe);
        cnt->mpipe = -1;
    }

    /* Cleanup the current time structure */
    if (cnt->currenttime_tm) {
        free(cnt->currenttime_tm);
        cnt->currenttime_tm = NULL;
    }

    /* Cleanup the event time structure */
    if (cnt->eventtime_tm) {
        free(cnt->eventtime_tm);
        cnt->eventtime_tm = NULL;
    }
}

/**
 * motion_loop
 *
 *   Thread function for the motion handling threads.
 *
 */
static void *motion_loop(void *arg)
{
    struct context *cnt = arg;
    int i, j, z = 0;
    time_t lastframetime = 0;
    int frame_buffer_size;
    unsigned short int ref_frame_limit = 0;
    int area_once = 0;
    int area_minx[9], area_miny[9], area_maxx[9], area_maxy[9];
    int smartmask_ratio = 0;
    int smartmask_count = 20;
    int smartmask_lastrate = 0;
    int olddiffs = 0;
    int previous_diffs = 0, previous_location_x = 0, previous_location_y = 0;
    unsigned short int text_size_factor;
    unsigned short int passflag = 0;
    long int *rolling_average_data = NULL;
    long int rolling_average_limit, required_frame_time, frame_delay, delay_time_nsec;
    int rolling_frame = 0;
    struct timeval tv1, tv2;
    unsigned long int rolling_average, elapsedtime;
    unsigned long long int timenow = 0, timebefore = 0;
    /* Return code used when calling vid_next */
    int vid_return_code = 0;       
    /* time in seconds to skip between capturing images */
    int minimum_frame_time_downcounter = cnt->conf.minimum_frame_time;
    /* Flag used to signal that we capture new image when we run the loop */
    unsigned short int get_image = 1;

    /* Next two variables are used for snapshot and timelapse feature
     * time_last_frame is set to 1 so that first coming timelapse or second=0
     * is acted upon.
     */
    unsigned long int time_last_frame=1, time_current_frame;

    cnt->running = 1;
    
    if (motion_init(cnt) < 0) 
        goto err;
    

    /* Initialize the double sized characters if needed. */
    if (cnt->conf.text_double)
        text_size_factor = 2;
    else
        text_size_factor = 1;

    /* Initialize area detection */
    area_minx[0] = area_minx[3] = area_minx[6] = 0;
    area_miny[0] = area_miny[1] = area_miny[2] = 0;

    area_minx[1] = area_minx[4] = area_minx[7] = cnt->imgs.width / 3;
    area_maxx[0] = area_maxx[3] = area_maxx[6] = cnt->imgs.width / 3;

    area_minx[2] = area_minx[5] = area_minx[8] = cnt->imgs.width / 3 * 2;
    area_maxx[1] = area_maxx[4] = area_maxx[7] = cnt->imgs.width / 3 * 2;

    area_miny[3] = area_miny[4] = area_miny[5] = cnt->imgs.height / 3;
    area_maxy[0] = area_maxy[1] = area_maxy[2] = cnt->imgs.height / 3;

    area_miny[6] = area_miny[7] = area_miny[8] = cnt->imgs.height / 3 * 2;
    area_maxy[3] = area_maxy[4] = area_maxy[5] = cnt->imgs.height / 3 * 2;

    area_maxx[2] = area_maxx[5] = area_maxx[8] = cnt->imgs.width;
    area_maxy[6] = area_maxy[7] = area_maxy[8] = cnt->imgs.height;
    
    /* Work out expected frame rate based on config setting */
    if (cnt->conf.frame_limit < 2) 
        cnt->conf.frame_limit = 2;

    required_frame_time = 1000000L / cnt->conf.frame_limit;

    frame_delay = required_frame_time;

    /*
     * Reserve enough space for a 10 second timing history buffer. Note that,
     * if there is any problem on the allocation, mymalloc does not return.
     */
    rolling_average_limit = 10 * cnt->conf.frame_limit;
    rolling_average_data = mymalloc(sizeof(rolling_average_data) * rolling_average_limit);

    /* Preset history buffer with expected frame rate */
    for (j = 0; j < rolling_average_limit; j++)
        rolling_average_data[j] = required_frame_time;

#ifdef __OpenBSD__
    /* 
     * FIXMARK 
     * Fixes zombie issue on OpenBSD 4.6
     */
    struct sigaction sig_handler_action;
    struct sigaction sigchild_action;
    setup_signals(&sig_handler_action, &sigchild_action);
#endif

    /* MAIN MOTION LOOP BEGINS HERE */
    /* Should go on forever... unless you bought vaporware :) */

    while (!cnt->finish || cnt->makemovie) {

    /***** MOTION LOOP - PREPARE FOR NEW FRAME SECTION *****/
        cnt->watchdog = WATCHDOG_TMO;

        /* Get current time and preserver last time for frame interval calc. */
        timebefore = timenow;
        gettimeofday(&tv1, NULL);
        timenow = tv1.tv_usec + 1000000L * tv1.tv_sec;

        /* since we don't have sanity checks done when options are set,
         * this sanity check must go in the main loop :(, before pre_captures
         * are attempted. */
        if (cnt->conf.minimum_motion_frames < 1)
            cnt->conf.minimum_motion_frames = 1;
        
        if (cnt->conf.pre_capture < 0)
            cnt->conf.pre_capture = 0;

        /* Check if our buffer is still the right size
         * If pre_capture or minimum_motion_frames has been changed
         * via the http remote control we need to re-size the ring buffer
         */
        frame_buffer_size = cnt->conf.pre_capture + cnt->conf.minimum_motion_frames;
        
        if (cnt->imgs.image_ring_size != frame_buffer_size) 
            image_ring_resize(cnt, frame_buffer_size);
        

        /* Get time for current frame */
        cnt->currenttime = time(NULL);

        /* localtime returns static data and is not threadsafe
         * so we use localtime_r which is reentrant and threadsafe
         */
        localtime_r(&cnt->currenttime, cnt->currenttime_tm);

        /* If we have started on a new second we reset the shots variable
         * lastrate is updated to be the number of the last frame. last rate
         * is used as the ffmpeg framerate when motion is detected.
         */
        if (lastframetime != cnt->currenttime) {
            cnt->lastrate = cnt->shots + 1;
            cnt->shots = -1;
            lastframetime = cnt->currenttime;
            if (cnt->conf.minimum_frame_time) {
                minimum_frame_time_downcounter--;
                if (minimum_frame_time_downcounter == 0)
                    get_image = 1;
            } else {
                get_image = 1;
            }    
        }


        /* Increase the shots variable for each frame captured within this second */
        cnt->shots++;

        if (cnt->startup_frames > 0)
            cnt->startup_frames--;

        if (get_image){
            if (cnt->conf.minimum_frame_time) {
                minimum_frame_time_downcounter = cnt->conf.minimum_frame_time;
                get_image = 0;
            }

            /* ring_buffer_in is pointing to current pos, update before put in a new image */
            if (++cnt->imgs.image_ring_in >= cnt->imgs.image_ring_size)
                cnt->imgs.image_ring_in = 0;

            /* Check if we have filled the ring buffer, throw away last image */
            if (cnt->imgs.image_ring_in == cnt->imgs.image_ring_out) {
                if (++cnt->imgs.image_ring_out >= cnt->imgs.image_ring_size)
                    cnt->imgs.image_ring_out = 0;
            }

            /* cnt->current_image points to position in ring where to store image, diffs etc. */
            cnt->current_image = &cnt->imgs.image_ring[cnt->imgs.image_ring_in];

            /* Init/clear current_image */
            {
                /* Store time with pre_captured image */
                cnt->current_image->timestamp = cnt->currenttime;
                localtime_r(&cnt->current_image->timestamp, &cnt->current_image->timestamp_tm);

                /* Store shot number with pre_captured image */
                cnt->current_image->shot = cnt->shots;

                /* set diffs to 0 now, will be written after we calculated diffs in new image */
                cnt->current_image->diffs = 0;

                /* Set flags to 0 */
                cnt->current_image->flags = 0;
                cnt->current_image->cent_dist = 0;

                /* Clear location data */
                memset(&cnt->current_image->location, 0, sizeof(cnt->current_image->location));
                cnt->current_image->total_labels = 0;
            }

        /***** MOTION LOOP - RETRY INITIALIZING SECTION *****/
            /* If a camera is not available we keep on retrying every 10 seconds
             * until it shows up.
             */
            if (cnt->video_dev < 0 &&
                cnt->currenttime % 10 == 0 && cnt->shots == 0) {
                motion_log(LOG_ERR, 0,
                           "Retrying until successful connection with camera");
                cnt->video_dev = vid_start(cnt);

                /* if the netcam has different dimensions than in the config file
                 * we need to restart Motion to re-allocate all the buffers
                 */
                if (cnt->imgs.width != cnt->conf.width || cnt->imgs.height != cnt->conf.height) {
                    motion_log(LOG_ERR, 0, "Camera has finally become available");
                    motion_log(LOG_ERR, 0, "Camera image has different width and height "
                                           "from what is in the config file. You should fix that");
                    motion_log(LOG_ERR, 0, "Restarting Motion thread to reinitialize all "
                                           "image buffers to new picture dimensions");
                    cnt->conf.width = cnt->imgs.width;
                    cnt->conf.height = cnt->imgs.height;
                    /* Break out of main loop terminating thread 
                     * watchdog will start us again */
                    break;
                }
            }


        /***** MOTION LOOP - IMAGE CAPTURE SECTION *****/

            /* Fetch next frame from camera
             * If vid_next returns 0 all is well and we got a new picture
             * Any non zero value is an error.
             * 0 = OK, valid picture
             * <0 = fatal error - leave the thread by breaking out of the main loop
             * >0 = non fatal error - copy last image or show grey image with message
             */
            if (cnt->video_dev >= 0)
                vid_return_code = vid_next(cnt, cnt->current_image->image);
            else
                vid_return_code = 1; /* Non fatal error */

            // VALID PICTURE
            if (vid_return_code == 0) {
                cnt->lost_connection = 0;
                cnt->connectionlosttime = 0;

                /* If all is well reset missing_frame_counter */
                if (cnt->missing_frame_counter >= MISSING_FRAMES_TIMEOUT * cnt->conf.frame_limit) 
                    /* If we previously logged starting a grey image, now log video re-start */
                    motion_log(LOG_ERR, 0, "Video signal re-acquired");
                    // event for re-acquired video signal can be called here
                

                cnt->missing_frame_counter = 0;

#ifdef HAVE_FFMPEG
                /* Deinterlace the image with ffmpeg, before the image is modified. */
                if (cnt->conf.ffmpeg_deinterlace) 
                    ffmpeg_deinterlace(cnt->current_image->image, cnt->imgs.width, cnt->imgs.height);
                
#endif

                /* save the newly captured still virgin image to a buffer
                 * which we will not alter with text and location graphics
                 */
                memcpy(cnt->imgs.image_virgin, cnt->current_image->image, cnt->imgs.size);

                /* If the camera is a netcam we let the camera decide the pace.
                 * Otherwise we will keep on adding duplicate frames.
                 * By resetting the timer the framerate becomes maximum the rate
                 * of the Netcam.
                 */
                if (cnt->conf.netcam_url) {
                    gettimeofday(&tv1, NULL);
                    timenow = tv1.tv_usec + 1000000L * tv1.tv_sec;
                }
            // FATAL ERROR - leave the thread by breaking out of the main loop    
            } else if (vid_return_code < 0) {
                /* Fatal error - Close video device */
                motion_log(LOG_ERR, 0, "Video device fatal error - Closing video device");
                vid_close(cnt);
                /* Use virgin image, if we are not able to open it again next loop
                 * a gray image with message is applied
                 * flag lost_connection
                 */
                memcpy(cnt->current_image->image, cnt->imgs.image_virgin, cnt->imgs.size);
                cnt->lost_connection = 1;
            /* NO FATAL ERROR -  
            *        copy last image or show grey image with message 
            *        flag on lost_connection if :
            *               vid_return_code == NETCAM_RESTART_ERROR  
            *        cnt->video_dev < 0
            *        cnt->missing_frame_counter > (MISSING_FRAMES_TIMEOUT * cnt->conf.frame_limit)
            */            
            } else { 

                if (debug_level >= CAMERA_VERBOSE)
                    motion_log(-1, 0, "vid_return_code %d", vid_return_code);

                /* Netcams that change dimensions while Motion is running will
                 * require that Motion restarts to reinitialize all the many
                 * buffers inside Motion. It will be a mess to try and recover any
                 * other way
                 */
                if (vid_return_code == NETCAM_RESTART_ERROR) {
                    motion_log(LOG_ERR, 0, "Restarting Motion thread to reinitialize all "
                                           "image buffers");
                    /* Break out of main loop terminating thread 
                     * watchdog will start us again 
                     * Set lost_connection flag on */

                    cnt->lost_connection = 1;
                    break;
                }

                /* First missed frame - store timestamp 
                 * Don't reset time when thread restarts*/
                if (cnt->connectionlosttime == 0)
                    cnt->connectionlosttime = cnt->currenttime;

                /* Increase missing_frame_counter
                 * The first MISSING_FRAMES_TIMEOUT seconds we copy previous virgin image
                 * After MISSING_FRAMES_TIMEOUT seconds we put a grey error image in the buffer
                 * If we still have not yet received the initial image from a camera
                 * we go straight for the grey error image.
                 */
                ++cnt->missing_frame_counter;
                if (cnt->video_dev >= 0 &&
                    cnt->missing_frame_counter < (MISSING_FRAMES_TIMEOUT * cnt->conf.frame_limit)) {
                    memcpy(cnt->current_image->image, cnt->imgs.image_virgin, cnt->imgs.size);
                } else {
                    const char *tmpin;
                    char tmpout[80];
                    struct tm tmptime;
                    cnt->lost_connection = 1;
        
                    if (cnt->video_dev >= 0)
                        tmpin = "CONNECTION TO CAMERA LOST\\nSINCE %Y-%m-%d %T";
                    else
                        tmpin = "UNABLE TO OPEN VIDEO DEVICE\\nSINCE %Y-%m-%d %T";

                    localtime_r(&cnt->connectionlosttime, &tmptime);
                    memset(cnt->current_image->image, 0x80, cnt->imgs.size);
                    mystrftime(cnt, tmpout, sizeof(tmpout), tmpin, &tmptime, NULL, 0);
                    draw_text(cnt->current_image->image, 10, 20 * text_size_factor, cnt->imgs.width,
                              tmpout, cnt->conf.text_double);

                    /* Write error message only once */
                    if (cnt->missing_frame_counter == MISSING_FRAMES_TIMEOUT * cnt->conf.frame_limit) {
                        motion_log(LOG_ERR, 0, "Video signal lost - Adding grey image");
                        // Event for lost video signal can be called from here
                        event(cnt, EVENT_CAMERA_LOST, NULL, NULL,
                              NULL, cnt->currenttime_tm);
                    }

                    /* If we don't get a valid frame for a long time, try to close/reopen device 
                     * Only try this when a device is open */
                    if ((cnt->video_dev > 0) && 
                        (cnt->missing_frame_counter == (MISSING_FRAMES_TIMEOUT * 4) * cnt->conf.frame_limit)) {
                        motion_log(LOG_ERR, 0, "Video signal still lost - Trying to close video device");
                        vid_close(cnt);
                    }
                }
            }

        /***** MOTION LOOP - MOTION DETECTION SECTION *****/

            /* The actual motion detection takes place in the following
             * diffs is the number of pixels detected as changed
             * Make a differences picture in image_out
             *
             * alg_diff_standard is the slower full feature motion detection algorithm
             * alg_diff first calls a fast detection algorithm which only looks at a
             *   fraction of the pixels. If this detects possible motion alg_diff_standard
             *   is called.
             */
            if (cnt->threshold && !cnt->pause) {
                /* if we've already detected motion and we want to see if there's
                 * still motion, don't bother trying the fast one first. IF there's
                 * motion, the alg_diff will trigger alg_diff_standard
                 * anyway
                 */
                if (cnt->detecting_motion || cnt->conf.setup_mode)
                    cnt->current_image->diffs = alg_diff_standard(cnt, cnt->imgs.image_virgin);
                else
                    cnt->current_image->diffs = alg_diff(cnt, cnt->imgs.image_virgin);

                /* Lightswitch feature - has light intensity changed?
                 * This can happen due to change of light conditions or due to a sudden change of the camera
                 * sensitivity. If alg_lightswitch detects lightswitch we suspend motion detection the next
                 * 5 frames to allow the camera to settle.
                 * Don't check if we have lost connection, we detect "Lost signal" frame as lightswitch
                 */
                if (cnt->conf.lightswitch && !cnt->lost_connection) {
                    if (alg_lightswitch(cnt, cnt->current_image->diffs)) {
                        if (cnt->conf.setup_mode)
                            motion_log(-1, 0, "Lightswitch detected");

                        if (cnt->moved < 5)
                            cnt->moved = 5;

                        cnt->current_image->diffs = 0;
                        alg_update_reference_frame(cnt, RESET_REF_FRAME);
                    }
                }

                /* Switchfilter feature tries to detect a change in the video signal
                 * from one camera to the next. This is normally used in the Round
                 * Robin feature. The algorithm is not very safe.
                 * The algorithm takes a little time so we only call it when needed
                 * ie. when feature is enabled and diffs>threshold.
                 * We do not suspend motion detection like we did for lightswitch
                 * because with Round Robin this is controlled by roundrobin_skip.
                 */
                if (cnt->conf.switchfilter && cnt->current_image->diffs > cnt->threshold) {
                    cnt->current_image->diffs = alg_switchfilter(cnt, cnt->current_image->diffs, 
                                                                 cnt->current_image->image);
                    
                    if (cnt->current_image->diffs <= cnt->threshold) {
                        cnt->current_image->diffs = 0;

                        if (cnt->conf.setup_mode)
                            motion_log(-1, 0, "Switchfilter detected");
                    }
                }

                /* Despeckle feature
                 * First we run (as given by the despeckle option iterations
                 * of erode and dilate algorithms.
                 * Finally we run the labelling feature.
                 * All this is done in the alg_despeckle code.
                 */
                cnt->current_image->total_labels = 0;
                cnt->imgs.largest_label = 0;
                olddiffs = 0;

                if (cnt->conf.despeckle && cnt->current_image->diffs > 0) {
                    olddiffs = cnt->current_image->diffs;
                    cnt->current_image->diffs = alg_despeckle(cnt, olddiffs);
                } else if (cnt->imgs.labelsize_max) {
                    cnt->imgs.labelsize_max = 0; /* Disable labeling if enabled */
                }    

            } else if (!cnt->conf.setup_mode) {
                cnt->current_image->diffs = 0;
            }

            /* Manipulate smart_mask sensitivity (only every smartmask_ratio seconds) */
            if (cnt->smartmask_speed && (cnt->event_nr != cnt->prev_event)) {
                if (!--smartmask_count) {
                    alg_tune_smartmask(cnt);
                    smartmask_count = smartmask_ratio;
                }
            }

            /* cnt->moved is set by the tracking code when camera has been asked to move.
             * When camera is moving we do not want motion to detect motion or we will
             * get our camera chasing itself like crazy and we will get motion detected
             * which is not really motion. So we pretend there is no motion by setting
             * cnt->diffs = 0.
             * We also pretend to have a moving camera when we start Motion and when light
             * switch has been detected to allow camera to settle.
             */
            if (cnt->moved) {
                cnt->moved--;
                cnt->current_image->diffs = 0;
            }

        /***** MOTION LOOP - TUNING SECTION *****/

            /* if noise tuning was selected, do it now. but only when
             * no frames have been recorded and only once per second
             */
            if (cnt->conf.noise_tune && cnt->shots == 0) {
                if (!cnt->detecting_motion && (cnt->current_image->diffs <= cnt->threshold))
                    alg_noise_tune(cnt, cnt->imgs.image_virgin);
            }

            /* if we are not noise tuning lets make sure that remote controlled
             * changes of noise_level are used.
             */
            if (!cnt->conf.noise_tune)
                cnt->noise = cnt->conf.noise;

            /* threshold tuning if enabled
             * if we are not threshold tuning lets make sure that remote controlled
             * changes of threshold are used.
             */
            if (cnt->conf.threshold_tune)
                alg_threshold_tune(cnt, cnt->current_image->diffs, cnt->detecting_motion);
            else
                cnt->threshold = cnt->conf.max_changes;

            /* If motion is detected (cnt->current_image->diffs > cnt->threshold) and before we add text to the pictures
               we find the center and size coordinates of the motion to be used for text overlays and later
               for adding the locate rectangle */
            if (cnt->current_image->diffs > cnt->threshold)
                 alg_locate_center_size(&cnt->imgs, cnt->imgs.width, cnt->imgs.height, 
                                        &cnt->current_image->location);

            /* Update reference frame. */
            /* micro-lighswitch: e.g. neighbors cat switched on the motion sensitive *
             * frontdoor illumination. Updates are rate-limited to 3 per second at   *
             * framerates above 5fps to save CPU resources and to keep sensitivity   *
             * at a constant level.                                                  *
             */
            ref_frame_limit++;
            if (ref_frame_limit >= (cnt->lastrate / 3)) {
                ref_frame_limit = 0;

                if ((cnt->current_image->diffs > cnt->threshold) && 
                    (cnt->lightswitch_framecounter < (cnt->lastrate * 2)) && /* two seconds window */
                    ((abs(previous_diffs - cnt->current_image->diffs)) < (previous_diffs / 15)) &&
                    ((abs(cnt->current_image->location.x - previous_location_x)) <= (cnt->imgs.width / 150)) &&
                    ((abs(cnt->current_image->location.y - previous_location_y)) <= (cnt->imgs.height / 150))) {
                    alg_update_reference_frame(cnt, RESET_REF_FRAME);
                    cnt->current_image->diffs = 0;
                    cnt->lightswitch_framecounter = 0;
                    if (cnt->conf.setup_mode)
                        motion_log(-1, 0, "micro-lightswitch!");

                } else {
                    alg_update_reference_frame(cnt, UPDATE_REF_FRAME);
                }

                previous_diffs = cnt->current_image->diffs;
                previous_location_x = cnt->current_image->location.x;
                previous_location_y = cnt->current_image->location.y;
            }

        /***** MOTION LOOP - TEXT AND GRAPHICS OVERLAY SECTION *****/

            /* Some overlays on top of the motion image
             * Note that these now modifies the cnt->imgs.out so this buffer
             * can no longer be used for motion detection features until next
             * picture frame is captured.
             */

            /* Smartmask overlay */
            if (cnt->smartmask_speed && (cnt->conf.motion_img || cnt->conf.ffmpeg_cap_motion || cnt->conf.setup_mode))
                overlay_smartmask(cnt, cnt->imgs.out);

            /* Largest labels overlay */
            if (cnt->imgs.largest_label && (cnt->conf.motion_img || cnt->conf.ffmpeg_cap_motion || cnt->conf.setup_mode))
                overlay_largest_label(cnt, cnt->imgs.out);


            /* Fixed mask overlay */
            if (cnt->imgs.mask && (cnt->conf.motion_img || cnt->conf.ffmpeg_cap_motion || cnt->conf.setup_mode))
                overlay_fixed_mask(cnt, cnt->imgs.out);

            /* Initialize the double sized characters if needed. */
            if (cnt->conf.text_double && text_size_factor == 1) {
                text_size_factor = 2;
            /* If text_double is set to off, then reset the scaling text_size_factor. */
            } else if (!cnt->conf.text_double && text_size_factor == 2) {
                text_size_factor = 1;
            }

            /* Add changed pixels in upper right corner of the pictures */
            if (cnt->conf.text_changes) {
                char tmp[15];

                if (!cnt->pause)
                    sprintf(tmp, "%d", cnt->current_image->diffs);
                else
                    sprintf(tmp, "-");

                draw_text(cnt->current_image->image, cnt->imgs.width - 10, 10, cnt->imgs.width, 
                          tmp, cnt->conf.text_double);
            }

            /* Add changed pixels to motion-images (for webcam) in setup_mode
               and always overlay smartmask (not only when motion is detected) */
            if (cnt->conf.setup_mode) {
                char tmp[PATH_MAX];
                sprintf(tmp, "D:%5d L:%3d N:%3d", cnt->current_image->diffs, cnt->current_image->total_labels, cnt->noise);
                draw_text(cnt->imgs.out, cnt->imgs.width - 10, cnt->imgs.height - 30 * text_size_factor,
                          cnt->imgs.width, tmp, cnt->conf.text_double);
                sprintf(tmp, "THREAD %d SETUP", cnt->threadnr);
                draw_text(cnt->imgs.out, cnt->imgs.width - 10, cnt->imgs.height - 10 * text_size_factor,
                          cnt->imgs.width, tmp, cnt->conf.text_double);
            }

            /* Add text in lower left corner of the pictures */
            if (cnt->conf.text_left) {
                char tmp[PATH_MAX];
                mystrftime(cnt, tmp, sizeof(tmp), cnt->conf.text_left, &cnt->current_image->timestamp_tm, NULL, 0);
                draw_text(cnt->current_image->image, 10, cnt->imgs.height - 10 * text_size_factor, cnt->imgs.width,
                          tmp, cnt->conf.text_double);
            }

            /* Add text in lower right corner of the pictures */
            if (cnt->conf.text_right) {
                char tmp[PATH_MAX];
                mystrftime(cnt, tmp, sizeof(tmp), cnt->conf.text_right, &cnt->current_image->timestamp_tm, NULL, 0);
                draw_text(cnt->current_image->image, cnt->imgs.width - 10, cnt->imgs.height - 10 * text_size_factor,
                          cnt->imgs.width, tmp, cnt->conf.text_double);
            }


        /***** MOTION LOOP - ACTIONS AND EVENT CONTROL SECTION *****/

            if (cnt->current_image->diffs > cnt->threshold) {
                /* flag this image, it have motion */
                cnt->current_image->flags |= IMAGE_MOTION;
                cnt->lightswitch_framecounter++; /* micro lightswitch */
            } else {
                cnt->lightswitch_framecounter = 0;
            }    

            /* If motion has been detected we take action and start saving
             * pictures and movies etc by calling motion_detected().
             * Is output_all enabled we always call motion_detected()
             * If post_capture is enabled we also take care of this in the this
             * code section.
             */
            if (cnt->conf.output_all && (cnt->startup_frames == 0)) {
                cnt->detecting_motion = 1;
                /* Setup the postcap counter */
                cnt->postcap = cnt->conf.post_capture;
                cnt->current_image->flags |= (IMAGE_TRIGGER | IMAGE_SAVE);
                motion_detected(cnt, cnt->video_dev, cnt->current_image);
            } else if ((cnt->current_image->flags & IMAGE_MOTION) && (cnt->startup_frames == 0)) {
                /* Did we detect motion (like the cat just walked in :) )?
                 * If so, ensure the motion is sustained if minimum_motion_frames
                 */

                /* Count how many frames with motion there is in the last minimum_motion_frames in precap buffer */
                int frame_count = 0;
                int pos = cnt->imgs.image_ring_in;

                for(i = 0; i < cnt->conf.minimum_motion_frames; i++) {
                    if (cnt->imgs.image_ring[pos].flags & IMAGE_MOTION)
                        frame_count++;

                    if (pos == 0) 
                        pos = cnt->imgs.image_ring_size-1;
                    else 
                        pos--;
                    
                }

                if (frame_count >= cnt->conf.minimum_motion_frames) {
                    cnt->current_image->flags |= (IMAGE_TRIGGER | IMAGE_SAVE);
                    cnt->detecting_motion = 1;
                    /* Setup the postcap counter */
                    cnt->postcap = cnt->conf.post_capture;
                    /* Mark all images in image_ring to be saved */
                    for(i = 0; i < cnt->imgs.image_ring_size; i++) 
                        cnt->imgs.image_ring[i].flags |= IMAGE_SAVE;
                    
                } else if (cnt->postcap) { /* we have motion in this frame, but not enought frames for trigger. Check postcap */
                    cnt->current_image->flags |= (IMAGE_POSTCAP | IMAGE_SAVE);
                    cnt->postcap--;
                } else {
                    cnt->current_image->flags |= IMAGE_PRECAP;
                }

                /* Always call motion_detected when we have a motion image */
                motion_detected(cnt, cnt->video_dev, cnt->current_image);
            } else if (cnt->postcap) {
                /* No motion, doing postcap */
                cnt->current_image->flags |= (IMAGE_POSTCAP | IMAGE_SAVE);
                cnt->postcap--;
            } else {
                /* Done with postcap, so just have the image in the precap buffer */
                cnt->current_image->flags |= IMAGE_PRECAP;
                cnt->detecting_motion = 0;
            }

            /* Update last frame saved time, so we can end event after gap time */
            if (cnt->current_image->flags & IMAGE_SAVE) 
                cnt->lasttime = cnt->current_image->timestamp;
            

            /* Simple hack to recognize motion in a specific area */
            /* Do we need a new coversion specifier as well?? */
            if ((cnt->conf.area_detect) && (cnt->event_nr != area_once) && 
                (cnt->current_image->flags & IMAGE_TRIGGER)) {

                j = strlen(cnt->conf.area_detect);

                for (i = 0; i < j; i++) {
                    z = cnt->conf.area_detect[i] - 49; /* 1 becomes 0 */

                    if ((z >= 0) && (z < 9)) {
                        if (cnt->current_image->location.x > area_minx[z] &&
                            cnt->current_image->location.x < area_maxx[z] &&
                            cnt->current_image->location.y > area_miny[z] &&
                            cnt->current_image->location.y < area_maxy[z]) {
                            event(cnt, EVENT_AREA_DETECTED, NULL, NULL,
                                  NULL, cnt->currenttime_tm);
                            area_once = cnt->event_nr; /* Fire script only once per event */
                            
                            if (cnt->conf.setup_mode)
                                motion_log(-1, 0, "Motion in area %d detected.\n", z+1);

                            break;
                        }
                    }
                }
            }
            
            /* Is the mpeg movie to long? Then make movies
             * First test for max mpegtime
             */
            if (cnt->conf.maxmpegtime && cnt->event_nr == cnt->prev_event)
                if (cnt->currenttime - cnt->eventtime >= cnt->conf.maxmpegtime)
                    cnt->makemovie = 1;

            /* Now test for quiet longer than 'gap' OR make movie as decided in
             * previous statement.
             */
            if (((cnt->currenttime - cnt->lasttime >= cnt->conf.gap) && cnt->conf.gap > 0) || cnt->makemovie) {
                if (cnt->event_nr == cnt->prev_event || cnt->makemovie) {

                    /* Flush image buffer */
                    process_image_ring(cnt, IMAGE_BUFFER_FLUSH);

                    /* Save preview_shot here at the end of event */
                    if (cnt->imgs.preview_image.diffs) {
                        preview_save(cnt);
                        cnt->imgs.preview_image.diffs = 0;
                    }

                    event(cnt, EVENT_ENDMOTION, NULL, NULL, NULL, cnt->currenttime_tm);

                    /* if tracking is enabled we center our camera so it does not
                     * point to a place where it will miss the next action
                     */
                    if (cnt->track.type)
                        cnt->moved = track_center(cnt, cnt->video_dev, 0, 0, 0);

                    if (cnt->conf.setup_mode)
                        motion_log(-1, 0, "End of event %d", cnt->event_nr);

                    cnt->makemovie = 0;
                    /* Reset post capture */
                    cnt->postcap = 0;

                    /* Finally we increase the event number */
                    cnt->event_nr++;
                    cnt->lightswitch_framecounter = 0;

                    /* And we unset the text_event_string to avoid that buffered
                     * images get a timestamp from previous event.
                     */
                    cnt->text_event_string[0] = '\0';
                }
            }

            /* Save/send to movie some images */
            process_image_ring(cnt, 2);

        /***** MOTION LOOP - SETUP MODE CONSOLE OUTPUT SECTION *****/

            /* If setup_mode enabled output some numbers to console */
            if (cnt->conf.setup_mode) {
                char msg[1024] = "\0";
                char part[100];

                if (cnt->conf.despeckle) {
                    snprintf(part, 99, "Raw changes: %5d - changes after '%s': %5d",
                             olddiffs, cnt->conf.despeckle, cnt->current_image->diffs);
                    strcat(msg, part);

                    if (strchr(cnt->conf.despeckle, 'l')){
                        sprintf(part, " - labels: %3d", cnt->current_image->total_labels);
                        strcat(msg, part);
                    }
                } else {
                    sprintf(part, "Changes: %5d", cnt->current_image->diffs);
                    strcat(msg, part);
                }

                if (cnt->conf.noise_tune) {
                    sprintf(part, " - noise level: %2d", cnt->noise);
                    strcat(msg, part);
                }

                if (cnt->conf.threshold_tune) {
                    sprintf(part, " - threshold: %d", cnt->threshold);
                    strcat(msg, part);
                }

                motion_log(-1, 0, "%s", msg);
            }

        } /* get_image end */

    /***** MOTION LOOP - SNAPSHOT FEATURE SECTION *****/

        /* Did we get triggered to make a snapshot from control http? Then shoot a snap
         * If snapshot_interval is not zero and time since epoch MOD snapshot_interval = 0 then snap
         * We actually allow the time to run over the interval in case we have a delay
         * from slow camera.
         * Note: Negative value means SIGALRM snaps are enabled
         * httpd-control snaps are always enabled.
         */

        /* time_current_frame is used both for snapshot and timelapse features */
        time_current_frame = cnt->currenttime;

        if ((cnt->conf.snapshot_interval > 0 && cnt->shots == 0 &&
            time_current_frame % cnt->conf.snapshot_interval <= time_last_frame % cnt->conf.snapshot_interval) ||
            cnt->snapshot) {
            event(cnt, EVENT_IMAGE_SNAPSHOT, cnt->current_image->image, NULL, NULL, 
                  &cnt->current_image->timestamp_tm);
            cnt->snapshot = 0;
        }


    /***** MOTION LOOP - TIMELAPSE FEATURE SECTION *****/

#ifdef HAVE_FFMPEG



        if (cnt->conf.timelapse) {

            /* Check to see if we should start a new timelapse file. We start one when
             * we are on the first shot, and and the seconds are zero. We must use the seconds
             * to prevent the timelapse file from getting reset multiple times during the minute.
             */
            if (cnt->current_image->timestamp_tm.tm_min == 0 &&
                (time_current_frame % 60 < time_last_frame % 60) &&
                cnt->shots == 0) {

                if (strcasecmp(cnt->conf.timelapse_mode,"manual") == 0) {
                    ;/* No action */

                /* If we are daily, raise timelapseend event at midnight */
                } else if (strcasecmp(cnt->conf.timelapse_mode, "daily") == 0) {
                    if (cnt->current_image->timestamp_tm.tm_hour == 0)
                        event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, 
                              &cnt->current_image->timestamp_tm);

                /* handle the hourly case */
                } else if (strcasecmp(cnt->conf.timelapse_mode, "hourly") == 0) {
                    event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, 
                          &cnt->current_image->timestamp_tm);

                /* If we are weekly-sunday, raise timelapseend event at midnight on sunday */
                } else if (strcasecmp(cnt->conf.timelapse_mode, "weekly-sunday") == 0) {
                    if (cnt->current_image->timestamp_tm.tm_wday == 0 && cnt->current_image->timestamp_tm.tm_hour == 0)
                        event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cnt->current_image->timestamp_tm);

                /* If we are weekly-monday, raise timelapseend event at midnight on monday */    
                } else if (strcasecmp(cnt->conf.timelapse_mode, "weekly-monday") == 0) {
                    if (cnt->current_image->timestamp_tm.tm_wday == 1 && cnt->current_image->timestamp_tm.tm_hour == 0)
                        event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cnt->current_image->timestamp_tm);

                /* If we are monthly, raise timelapseend event at midnight on first day of month */    
                } else if (strcasecmp(cnt->conf.timelapse_mode, "monthly") == 0) {
                    if (cnt->current_image->timestamp_tm.tm_mday == 1 && cnt->current_image->timestamp_tm.tm_hour == 0)
                        event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cnt->current_image->timestamp_tm);

                /* If invalid we report in syslog once and continue in manual mode */    
                } else {
                    motion_log(LOG_ERR, 0, "Invalid timelapse_mode argument '%s'",
                               cnt->conf.timelapse_mode);
                    motion_log(LOG_ERR, 0, "Defaulting to manual timelapse mode");
                    conf_cmdparse(&cnt, (char *)"ffmpeg_timelapse_mode",(char *)"manual");
                }
            }

            /* If ffmpeg timelapse is enabled and time since epoch MOD ffmpeg_timelaps = 0
             * add a timelapse frame to the timelapse mpeg.
             */
            if (cnt->shots == 0 &&
                time_current_frame % cnt->conf.timelapse <= time_last_frame % cnt->conf.timelapse)
                event(cnt, EVENT_TIMELAPSE, cnt->current_image->image, NULL, NULL, 
                      &cnt->current_image->timestamp_tm);
        } else if (cnt->ffmpeg_timelapse) {
        /* if timelapse mpeg is in progress but conf.timelapse is zero then close timelapse file
         * This is an important feature that allows manual roll-over of timelapse file using the http
         * remote control via a cron job.
         */
            event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, cnt->currenttime_tm);
        }

#endif /* HAVE_FFMPEG */

        time_last_frame = time_current_frame;


    /***** MOTION LOOP - VIDEO LOOPBACK SECTION *****/

        /* feed last image and motion image to video device pipes and the webcam clients
         * In setup mode we send the special setup mode image to both webcam and vloopback pipe
         * In normal mode we feed the latest image to vloopback device and we send
         * the image to the webcam. We always send the first image in a second to the webcam.
         * Other image are sent only when the config option webcam_motion is off
         * The result is that with webcam_motion on the webcam stream is normally at the minimal
         * 1 frame per second but the minute motion is detected the motion_detected() function
         * sends all detected pictures to the webcam except the 1st per second which is already sent.
         */
        if (cnt->conf.setup_mode) {
            event(cnt, EVENT_IMAGE, cnt->imgs.out, NULL, &cnt->pipe, cnt->currenttime_tm);
            event(cnt, EVENT_WEBCAM, cnt->imgs.out, NULL, NULL, cnt->currenttime_tm);
        } else {
            event(cnt, EVENT_IMAGE, cnt->current_image->image, NULL, &cnt->pipe, 
                  &cnt->current_image->timestamp_tm);

            if (!cnt->conf.webcam_motion || cnt->shots == 1)
                event(cnt, EVENT_WEBCAM, cnt->current_image->image, NULL, NULL, 
                      &cnt->current_image->timestamp_tm);
        }

        event(cnt, EVENT_IMAGEM, cnt->imgs.out, NULL, &cnt->mpipe, cnt->currenttime_tm);


    /***** MOTION LOOP - ONCE PER SECOND PARAMETER UPDATE SECTION *****/

        /* Check for some config parameter changes but only every second */
        if (cnt->shots == 0){
            if (strcasecmp(cnt->conf.output_normal, "on") == 0)
                cnt->new_img = NEWIMG_ON;
            else if (strcasecmp(cnt->conf.output_normal, "first") == 0)
                cnt->new_img = NEWIMG_FIRST;
            else if (strcasecmp(cnt->conf.output_normal, "best") == 0)
                cnt->new_img = NEWIMG_BEST;
            else if (strcasecmp(cnt->conf.output_normal, "center") == 0)
                cnt->new_img = NEWIMG_CENTER;
            else
                cnt->new_img = NEWIMG_OFF;

            if (strcasecmp(cnt->conf.locate, "on") == 0)
                cnt->locate = LOCATE_ON;
            else if (strcasecmp(cnt->conf.locate, "preview") == 0)
                cnt->locate = LOCATE_PREVIEW;
            else
                cnt->locate = LOCATE_OFF;

            /* Sanity check for smart_mask_speed, silly value disables smart mask */
            if (cnt->conf.smart_mask_speed < 0 || cnt->conf.smart_mask_speed > 10)
                cnt->conf.smart_mask_speed = 0;

            /* Has someone changed smart_mask_speed or framerate? */
            if (cnt->conf.smart_mask_speed != cnt->smartmask_speed || smartmask_lastrate != cnt->lastrate){
                if (cnt->conf.smart_mask_speed == 0){
                    memset(cnt->imgs.smartmask, 0, cnt->imgs.motionsize);
                    memset(cnt->imgs.smartmask_final, 255, cnt->imgs.motionsize);
                }
                smartmask_lastrate = cnt->lastrate;
                cnt->smartmask_speed = cnt->conf.smart_mask_speed;
                /* Decay delay - based on smart_mask_speed (framerate independent)
                   This is always 5*smartmask_speed seconds */
                smartmask_ratio = 5 * cnt->lastrate * (11 - cnt->smartmask_speed);
            }

#if defined(HAVE_MYSQL) || defined(HAVE_PGSQL)
            /* Set the sql mask file according to the SQL config options
             * We update it for every frame in case the config was updated
             * via remote control.
             */
            cnt->sql_mask = cnt->conf.sql_log_image * (FTYPE_IMAGE + FTYPE_IMAGE_MOTION) +
                            cnt->conf.sql_log_snapshot * FTYPE_IMAGE_SNAPSHOT +
                            cnt->conf.sql_log_mpeg * (FTYPE_MPEG + FTYPE_MPEG_MOTION) +
                            cnt->conf.sql_log_timelapse * FTYPE_MPEG_TIMELAPSE;
#endif /* defined(HAVE_MYSQL) || defined(HAVE_PGSQL) */

        }


    /***** MOTION LOOP - FRAMERATE TIMING AND SLEEPING SECTION *****/


        /* Work out expected frame rate based on config setting which may
           have changed from http-control */
        if (cnt->conf.frame_limit)
            required_frame_time = 1000000L / cnt->conf.frame_limit;
        else
            required_frame_time = 0;

        /* Get latest time to calculate time taken to process video data */
        gettimeofday(&tv2, NULL);
        elapsedtime = (tv2.tv_usec + 1000000L * tv2.tv_sec) - timenow;

        /* Update history buffer but ignore first pass as timebefore
           variable will be inaccurate
         */
        if (passflag)
            rolling_average_data[rolling_frame] = timenow-timebefore;
        else
            passflag = 1;

        rolling_frame++;
        if (rolling_frame >= rolling_average_limit)
            rolling_frame = 0;

        /* Calculate 10 second average and use deviation in delay calculation */
        rolling_average = 0L;

        for (j = 0; j < rolling_average_limit; j++)
            rolling_average += rolling_average_data[j];

        rolling_average /= rolling_average_limit;
        frame_delay = required_frame_time-elapsedtime - (rolling_average - required_frame_time);

        if (frame_delay > 0) {
            /* Apply delay to meet frame time */
            if (frame_delay > required_frame_time)
                frame_delay = required_frame_time;

            /* Delay time in nanoseconds for SLEEP */
            delay_time_nsec = frame_delay * 1000;

            if (delay_time_nsec > 999999999)
                delay_time_nsec = 999999999;

            /* SLEEP as defined in motion.h  A safe sleep using nanosleep */
            SLEEP(0, delay_time_nsec);
        }
    }

    /* END OF MOTION MAIN LOOP
     * If code continues here it is because the thread is exiting or restarting
     */
err:
    if (rolling_average_data)
        free(rolling_average_data);

    cnt->lost_connection = 1;
    motion_log(-1, 0, "Thread exiting");

    motion_cleanup(cnt);

    pthread_mutex_lock(&global_lock);
    threads_running--;
    pthread_mutex_unlock(&global_lock);

    if (!cnt->restart)
        cnt->watchdog=WATCHDOG_OFF;

    cnt->running = 0;
    cnt->finish = 0;

    pthread_exit(NULL);
}

/**
 * become_daemon
 *
 *   Turns Motion into a daemon through forking. The parent process (i.e. the
 *   one initially calling this function) will exit inside this function, while
 *   control will be returned to the child process. Standard input/output are
 *   released properly, and the current directory is set to / in order to not
 *   lock up any file system.
 *
 * Parameters:
 *
 *   cnt - current thread's context struct
 *
 * Returns: nothing
 */
static void become_daemon(void)
{
    int i;
    FILE *pidf = NULL;    
    struct sigaction sig_ign_action;

    /* Setup sig_ign_action */
#ifdef SA_RESTART
    sig_ign_action.sa_flags = SA_RESTART;
#else
    sig_ign_action.sa_flags = 0;
#endif
    sig_ign_action.sa_handler = SIG_IGN;
    sigemptyset(&sig_ign_action.sa_mask);

    /* fork */
    if (fork()) {
        motion_log(-1, 0, "Motion going to daemon mode");
        exit(0);
    }
    
    /* Create the pid file if defined, if failed exit
     * If we fail we report it. If we succeed we postpone the log entry till
     * later when we have closed stdout. Otherwise Motion hangs in the terminal waiting
     * for an enter.
     */
    if (cnt_list[0]->conf.pid_file) {
        pidf = fopen(cnt_list[0]->conf.pid_file, "w+");
    
        if (pidf ) {
            (void)fprintf(pidf, "%d\n", getpid());
            fclose(pidf);
        } else {
            motion_log(LOG_ERR, 1, "Exit motion, cannot create process id file (pid file) %s",
                       cnt_list[0]->conf.pid_file);    
            exit(0);    
        }
    }

    /* changing dir to root enables people to unmount a disk
       without having to stop Motion */
    if (chdir("/")) {
        motion_log(LOG_ERR, 1, "Could not change directory");
    }

#if (defined(BSD))
    setpgrp(0, getpid());
#else
    setpgrp();
#endif /* BSD */

    
    if ((i = open("/dev/tty", O_RDWR)) >= 0) {
        ioctl(i, TIOCNOTTY, NULL);
        close(i);
    }

    setsid();
    i = open("/dev/null", O_RDONLY);

    if (i != -1) {
        dup2(i, STDIN_FILENO);
        close(i);
    }

    i = open("/dev/null", O_WRONLY);

    if (i != -1) {
        dup2(i, STDOUT_FILENO);
        dup2(i, STDERR_FILENO);
        close(i);
    }
    
    /* Now it is safe to add the PID creation to the logs */
    if (pidf )
        motion_log(LOG_INFO, 0, "Created process id file %s. Process ID is %d",
                   cnt_list[0]->conf.pid_file, getpid());
    
    sigaction(SIGTTOU, &sig_ign_action, NULL);
    sigaction(SIGTTIN, &sig_ign_action, NULL);
    sigaction(SIGTSTP, &sig_ign_action, NULL);
}

/**
 * cntlist_create
 *
 *   Sets up the 'cnt_list' variable by allocating room for (and actually
 *   allocating) one context struct. Also loads the configuration from
 *   the config file(s).
 *
 * Parameters:
 *   argc - size of argv
 *   argv - command-line options, passed initially from 'main'
 *
 * Returns: nothing
 */
static void cntlist_create(int argc, char *argv[])
{
    /* cnt_list is an array of pointers to the context structures cnt for each thread.
     * First we reserve room for a pointer to thread 0's context structure
     * and a NULL pointer which indicates that end of the array of pointers to
     * thread context structures.
     */
    cnt_list = mymalloc(sizeof(struct context *) * 2);

    /* Now we reserve room for thread 0's context structure and let cnt_list[0] point to it */
    cnt_list[0] = mymalloc(sizeof(struct context));

    /* Populate context structure with start/default values */
    context_init(cnt_list[0]);

    /* cnt_list[1] pointing to zero indicates no more thread context structures - they get added later */
    cnt_list[1] = NULL;

    /* Command line arguments are being pointed to from cnt_list[0] and we call conf_load which loads
     * the config options from motion.conf, thread config files and the command line.
     */
    cnt_list[0]->conf.argv = argv;
    cnt_list[0]->conf.argc = argc;
    cnt_list = conf_load(cnt_list);
}


/**
 * motion_shutdown
 *
 *   Responsible for performing cleanup when Motion is shut down or restarted,
 *   including freeing memory for all the context structs as well as for the
 *   context struct list itself.
 *
 * Parameters: none
 *
 * Returns:    nothing
 */
static void motion_shutdown(void)
{
    int i = -1;

    motion_remove_pid();
    
    while (cnt_list[++i])
        context_destroy(cnt_list[i]);
    
    free(cnt_list);
    cnt_list = NULL;
#ifndef WITHOUT_V4L
    vid_cleanup();
#endif
}

/**
 * motion_startup
 *
 *   Responsible for initializing stuff when Motion starts up or is restarted,
 *   including daemon initialization and creating the context struct list.
 *
 * Parameters:
 *
 *   daemonize - non-zero to do daemon init (if the config parameters says so),
 *               or 0 to skip it
 *   argc      - size of argv
 *   argv      - command-line options, passed initially from 'main'
 *
 * Returns: nothing
 */
static void motion_startup(int daemonize, int argc, char *argv[])
{
    /* Initialize our global mutex */
    pthread_mutex_init(&global_lock, NULL);

    /* Create the list of context structures and load the
     * configuration.
     */
    cntlist_create(argc, argv);

    motion_log(LOG_INFO, 0, "Motion "VERSION" Started");

    initialize_chars();

    if (daemonize) {
        /* If daemon mode is requested, and we're not going into setup mode,
         * become daemon.
         */
        if (cnt_list[0]->daemon && cnt_list[0]->conf.setup_mode == 0) {
            become_daemon();
            motion_log(LOG_INFO, 0, "Motion running as daemon process");
        }
    }

#ifndef WITHOUT_V4L
    vid_init();
#endif
}

/**
 * setup_signals
 *
 *   Attaches handlers to a number of signals that Motion need to catch.
 *
 * Parameters: sigaction structs for signals in general and SIGCHLD.
 *
 * Returns:    nothing
 */
static void setup_signals(struct sigaction *sig_handler_action, struct sigaction *sigchild_action)
{
#ifdef SA_NOCLDWAIT
    sigchild_action->sa_flags = SA_NOCLDWAIT;
#else
    sigchild_action->sa_flags = 0;
#endif
    sigchild_action->sa_handler = sigchild_handler;
    sigemptyset(&sigchild_action->sa_mask);
#ifdef SA_RESTART
    sig_handler_action->sa_flags = SA_RESTART;
#else
    sig_handler_action->sa_flags = 0;
#endif
    sig_handler_action->sa_handler = sig_handler;
    sigemptyset(&sig_handler_action->sa_mask);

    /* Enable automatic zombie reaping */
    sigaction(SIGCHLD, sigchild_action, NULL);
    sigaction(SIGPIPE, sigchild_action, NULL);
    sigaction(SIGALRM, sig_handler_action, NULL);
    sigaction(SIGHUP,  sig_handler_action, NULL);
    sigaction(SIGINT,  sig_handler_action, NULL);
    sigaction(SIGQUIT, sig_handler_action, NULL);
    sigaction(SIGTERM, sig_handler_action, NULL);
    sigaction(SIGUSR1, sig_handler_action, NULL);
}

/**
 * start_motion_thread
 *
 *   Called from main when start a motion thread
 *
 * Parameters:
 *
 *   cnt - Thread context pointer
 *   thread_attr - pointer to thread attributes
 *
 * Returns: nothing
 */
static void start_motion_thread(struct context *cnt, pthread_attr_t *thread_attr)
{
    int i;

    /* Check the webcam port number for conflicts.
     * First we check for conflict with the control port.
     * Second we check for that two threads does not use the same port number
     * for the webcam. If a duplicate port is found the webcam feature gets disabled (port =0)
     * for this thread and a warning is written to console and syslog.
     */

    if (cnt->conf.webcam_port != 0) {
        /* Compare against the control port. */
        if (cnt_list[0]->conf.control_port == cnt->conf.webcam_port) {
            motion_log(LOG_ERR, 0,
                       "Webcam port number %d for thread %d conflicts with the control port",
                       cnt->conf.webcam_port, cnt->threadnr);
            motion_log(LOG_ERR, 0, "Webcam feature for thread %d is disabled.", cnt->threadnr);
            cnt->conf.webcam_port = 0;
        }

        /* Compare against webcam ports of other threads. */
        for (i = 1; cnt_list[i]; i++) {
            if (cnt_list[i] == cnt)
                continue;

            if (cnt_list[i]->conf.webcam_port == cnt->conf.webcam_port) {
                motion_log(LOG_ERR, 0,
                           "Webcam port number %d for thread %d conflicts with thread %d",
                           cnt->conf.webcam_port, cnt->threadnr, cnt_list[i]->threadnr);
                motion_log(LOG_ERR, 0,
                           "Webcam feature for thread %d is disabled.", cnt->threadnr);
                cnt->conf.webcam_port = 0;
            }
        }
    }

    /* Update how many threads we have running. This is done within a
     * mutex lock to prevent multiple simultaneous updates to
     * 'threads_running'.
     */
    pthread_mutex_lock(&global_lock);
    threads_running++;
    pthread_mutex_unlock(&global_lock);

    /* Set a flag that we want this thread running */
    cnt->restart = 1;

    /* Give the thread WATCHDOG_TMO seconds to start */
    cnt->watchdog = WATCHDOG_TMO;

    /* Create the actual thread. Use 'motion_loop' as the thread
     * function.
     */
    pthread_create(&cnt->thread_id, thread_attr, &motion_loop, cnt);
}

/**
 * main
 *
 *   Main entry point of Motion. Launches all the motion threads and contains
 *   the logic for starting up, restarting and cleaning up everything.
 *
 * Parameters:
 *
 *   argc - size of argv
 *   argv - command-line options
 *
 * Returns: Motion exit status = 0 always
 */
int main (int argc, char **argv)
{
    int i;
    pthread_attr_t thread_attr;
    pthread_t thread_id;

    /* Setup signals and do some initialization. 1 in the call to
     * 'motion_startup' means that Motion will become a daemon if so has been
     * requested, and argc and argc are necessary for reading the command
     * line options.
     */
    struct sigaction sig_handler_action;
    struct sigaction sigchild_action;
    setup_signals(&sig_handler_action, &sigchild_action);

    motion_startup(1, argc, argv);

#ifdef HAVE_FFMPEG
    /* FFMpeg initialization is only performed if FFMpeg support was found
     * and not disabled during the configure phase.
     */
    ffmpeg_init();
#endif /* HAVE_FFMPEG */

    /* In setup mode, Motion is very communicative towards the user, which
     * allows the user to experiment with the config parameters in order to
     * optimize motion detection and stuff.
     */
    if (cnt_list[0]->conf.setup_mode)
        motion_log(-1, 0, "Motion running in setup mode.");

    /* Create and a thread attribute for the threads we spawn later on.
     * PTHREAD_CREATE_DETACHED means to create threads detached, i.e.
     * their termination cannot be synchronized through 'pthread_join'.
     */
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

    /* Create the TLS key for thread number. */
    pthread_key_create(&tls_key_threadnr, NULL);

    do {
        if (restart) {
            /* Handle the restart situation. Currently the approach is to
             * cleanup everything, and then initialize everything again
             * (including re-reading the config file(s)).
             */
            motion_shutdown();
            restart = 0; /* only one reset for now */
            motion_log(LOG_INFO,0,"motion restarted");
#ifndef WITHOUT_V4L
            SLEEP(5,0); // maybe some cameras needs less time
#endif
            motion_startup(0, argc, argv); /* 0 = skip daemon init */
        }


        /* Start the motion threads. First 'cnt_list' item is global if 'thread'
         * option is used, so start at 1 then and 0 otherwise.
         */
        for (i = cnt_list[1] != NULL ? 1 : 0; cnt_list[i]; i++) {
            /* If i is 0 it means no thread files and we then set the thread number to 1 */
            cnt_list[i]->threadnr = i ? i : 1;

            if (strcmp(cnt_list[i]->conf_filename,"") )
                motion_log(LOG_INFO, 0, "Thread %d is from %s", cnt_list[i]->threadnr, cnt_list[i]->conf_filename );

            if (cnt_list[0]->conf.setup_mode) {
                motion_log(-1, 0, "Thread %d is device: %s input %d", cnt_list[i]->threadnr,
                           cnt_list[i]->conf.netcam_url ? cnt_list[i]->conf.netcam_url : cnt_list[i]->conf.video_device,
                           cnt_list[i]->conf.netcam_url ? -1 : cnt_list[i]->conf.input
                          );
            }

            if (cnt_list[0]->conf.setup_mode)
                motion_log(LOG_ERR, 0, "Webcam port %d", cnt_list[i]->conf.webcam_port);

            start_motion_thread(cnt_list[i], &thread_attr);
        }

        /* Create a thread for the control interface if requested. Create it
         * detached and with 'motion_web_control' as the thread function.
         */
        if (cnt_list[0]->conf.control_port)
            pthread_create(&thread_id, &thread_attr, &motion_web_control, cnt_list);

        if (cnt_list[0]->conf.setup_mode)
            motion_log(-1, 0,"Waiting for threads to finish, pid: %d", getpid());

        /* Crude way of waiting for all threads to finish - check the thread
         * counter (because we cannot do join on the detached threads).
         */
        while (1) {
            SLEEP(1,0);

            /* Calculate how many threads runnig or wants to run
             * if zero and we want to finish, break out
             */
            int motion_threads_running = 0;
            for (i = (cnt_list[1] != NULL ? 1 : 0); cnt_list[i]; i++) {
                if (cnt_list[i]->running || cnt_list[i]->restart)
                    motion_threads_running++;
            }

            if (((motion_threads_running == 0 ) && finish ) || 
                 ((motion_threads_running == 0 ) && (threads_running == 0)) ){
                if (debug_level >= CAMERA_DEBUG){
                         motion_log(LOG_INFO, 0, "DEBUG-1 threads_running %d motion_threads_running %d , finish %d",
                                            threads_running, motion_threads_running, finish); 
                }
                break;
            }

            for (i = (cnt_list[1] != NULL ? 1 : 0); cnt_list[i]; i++) {
                /* Check if threads wants to be restarted */
                if ((!cnt_list[i]->running) && (cnt_list[i]->restart) ) {
                    motion_log(LOG_INFO, 0, "Motion thread %d restart", cnt_list[i]->threadnr);
                    start_motion_thread(cnt_list[i], &thread_attr);
                }
                if (cnt_list[i]->watchdog > WATCHDOG_OFF) {
                    cnt_list[i]->watchdog--;
                    if (cnt_list[i]->watchdog == 0) {
                        motion_log(LOG_ERR, 0, "Thread %d - Watchdog timeout, trying to do a graceful restart",
                                                  cnt_list[i]->threadnr);
                        cnt_list[i]->finish = 1;
                    }
                    if (cnt_list[i]->watchdog == -60) {
                        motion_log(LOG_ERR, 0, "Thread %d - Watchdog timeout, did NOT restart graceful," 
                                               "killing it!", cnt_list[i]->threadnr);
                        pthread_cancel(cnt_list[i]->thread_id);
                        pthread_mutex_lock(&global_lock);
                        threads_running--;
                        pthread_mutex_unlock(&global_lock);
                        motion_cleanup(cnt_list[i]);
                        cnt_list[i]->running = 0;
                        cnt_list[i]->finish = 0;
                    }
                }
            }

            if (debug_level >= CAMERA_DEBUG){
                motion_log(LOG_INFO, 0, "DEBUG-2 threads_running %d motion_threads_running %d , finish %d",
                                        threads_running, motion_threads_running, finish); 
            }
        }
        /* Reset end main loop flag */
        finish = 0;

        if (cnt_list[0]->conf.setup_mode)
            motion_log(LOG_DEBUG, 0, "Threads finished");

        /* Rest for a while if we're supposed to restart. */
        if (restart)
            SLEEP(2,0);

    } while (restart); /* loop if we're supposed to restart */

    // Be sure that http control exits fine
    cnt_list[0]->finish = 1;
    SLEEP(1,0);
    motion_log(LOG_INFO, 0, "Motion terminating");

    /* Perform final cleanup. */
    pthread_key_delete(tls_key_threadnr);
    pthread_attr_destroy(&thread_attr);
    pthread_mutex_destroy(&global_lock);
    motion_shutdown();

    return 0;
}

/**
 * mymalloc
 *
 *   Allocates some memory and checks if that succeeded or not. If it failed,
 *   do some errorlogging and bail out.
 *
 *   NOTE: Kenneth Lavrsen changed printing of size_t types so instead of using
 *   conversion specifier %zd I changed it to %llu and casted the size_t
 *   variable to unsigned long long. The reason for this nonsense is that older
 *   versions of gcc like 2.95 uses %Zd and does not understand %zd. So to avoid
 *   this mess I used a more generic way. Long long should have enough bits for
 *   64-bit machines with large memory areas.
 *
 * Parameters:
 *
 *   nbytes - no. of bytes to allocate
 *
 * Returns: a pointer to the allocated memory
 */
void * mymalloc(size_t nbytes)
{
    void *dummy = malloc(nbytes);
    if (!dummy) {
        motion_log(LOG_EMERG, 1, "Could not allocate %llu bytes of memory!", (unsigned long long)nbytes);
        motion_remove_pid();
        exit(1);
    }

    return dummy;
}

/**
 * myrealloc
 *
 *   Re-allocate (i.e., resize) some memory and check if that succeeded or not.
 *   If it failed, do some errorlogging and bail out. If the new memory size
 *   is 0, the memory is freed.
 *
 * Parameters:
 *
 *   ptr  - pointer to the memory to resize/reallocate
 *   size - new memory size
 *   desc - name of the calling function
 *
 * Returns: a pointer to the reallocated memory, or NULL if the memory was
 *          freed
 */
void *myrealloc(void *ptr, size_t size, const char *desc)
{
    void *dummy = NULL;

    if (size == 0) {
        free(ptr);
        motion_log(LOG_WARNING, 0,
                   "Warning! Function %s tries to resize memoryblock at %p to 0 bytes!",
                   desc, ptr);
    } else {
        dummy = realloc(ptr, size);
        if (!dummy) {
            motion_log(LOG_EMERG, 0,
                       "Could not resize memory-block at offset %p to %llu bytes (function %s)!",
                       ptr, (unsigned long long)size, desc);
            motion_remove_pid();
            exit(1);
        }
    }

    return dummy;
}

/**
 * create_path
 *
 *   This function creates a whole path, like mkdir -p. Example paths:
 *      this/is/an/example/
 *      /this/is/an/example/
 *   Warning: a path *must* end with a slash!
 *
 * Parameters:
 *
 *   cnt  - current thread's context structure (for logging)
 *   path - the path to create
 *
 * Returns: 0 on success, -1 on failure
 */
int create_path(const char *path)
{
    char *start;
    mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

    if (path[0] == '/')
        start = strchr(path + 1, '/');
    else
        start = strchr(path, '/');

    while (start) {
        char *buffer = strdup(path);
        buffer[start-path] = 0x00;

        if (mkdir(buffer, mode) == -1 && errno != EEXIST) {
            motion_log(LOG_ERR, 1, "Problem creating directory %s", buffer);
            free(buffer);
            return -1;
        }

        free(buffer);

        start = strchr(start + 1, '/');
    }

    return 0;
}

/**
 * myfopen
 *
 *   This function opens a file, if that failed because of an ENOENT error
 *   (which is: path does not exist), the path is created and then things are
 *   tried again. This is faster then trying to create that path over and over
 *   again. If someone removes the path after it was created, myfopen will
 *   recreate the path automatically.
 *
 * Parameters:
 *
 *   path - path to the file to open
 *   mode - open mode
 *
 * Returns: the file stream object
 */
FILE * myfopen(const char *path, const char *mode)
{
    /* first, just try to open the file */
    FILE *dummy = fopen(path, mode);

    /* could not open file... */
    if (!dummy) {
        /* path did not exist? */
        if (errno == ENOENT) {

            /* create path for file... */
            if (create_path(path) == -1)
                return NULL;

            /* and retry opening the file */
            dummy = fopen(path, mode);
            if (dummy)
                return dummy;
        }

        /* two possibilities
         * 1: there was an other error while trying to open the file for the first time
         * 2: could still not open the file after the path was created
         */
        motion_log(LOG_ERR, 1, "Error opening file %s with mode %s", path, mode);

        return NULL;
    }

    return dummy;
}

/**
 * mystrftime
 *
 *   Motion-specific variant of strftime(3) that supports additional format
 *   specifiers in the format string.
 *
 * Parameters:
 *
 *   cnt        - current thread's context structure
 *   s          - destination string
 *   max        - max number of bytes to write
 *   userformat - format string
 *   tm         - time information
 *   filename   - string containing full path of filename
 *                set this to NULL if not relevant
 *   sqltype    - Filetype as used in SQL feature, set to 0 if not relevant
 *
 * Returns: number of bytes written to the string s
 */
size_t mystrftime(struct context *cnt, char *s, size_t max, const char *userformat,
                  const struct tm *tm, const char *filename, int sqltype)
{
    char formatstring[PATH_MAX] = "";
    char tempstring[PATH_MAX] = "";
    char *format, *tempstr;
    const char *pos_userformat;

    format = formatstring;

    /* if mystrftime is called with userformat = NULL we return a zero length string */
    if (userformat == NULL) {
        *s = '\0';
        return 0;
    }

    for (pos_userformat = userformat; *pos_userformat; ++pos_userformat) {

        if (*pos_userformat == '%') {
            /* Reset 'tempstr' to point to the beginning of 'tempstring',
             * otherwise we will eat up tempstring if there are many
             * format specifiers.
             */
            tempstr = tempstring;
            tempstr[0] = '\0';
            switch (*++pos_userformat) {
                case '\0': // end of string
                    --pos_userformat;
                    break;

                case 'v': // event
                    sprintf(tempstr, "%02d", cnt->event_nr);
                    break;

                case 'q': // shots
                    sprintf(tempstr, "%02d", cnt->current_image->shot);
                    break;

                case 'D': // diffs
                    sprintf(tempstr, "%d", cnt->current_image->diffs);
                    break;

                case 'N': // noise
                    sprintf(tempstr, "%d", cnt->noise);
                    break;

                case 'i': // motion width
                    sprintf(tempstr, "%d", cnt->current_image->location.width);
                    break;

                case 'J': // motion height
                    sprintf(tempstr, "%d", cnt->current_image->location.height);
                    break;

                case 'K': // motion center x
                    sprintf(tempstr, "%d", cnt->current_image->location.x);
                    break;

                case 'L': // motion center y
                    sprintf(tempstr, "%d", cnt->current_image->location.y);
                    break;

                case 'o': // threshold
                    sprintf(tempstr, "%d", cnt->threshold);
                    break;

                case 'Q': // number of labels
                    sprintf(tempstr, "%d", cnt->current_image->total_labels);
                    break;
                case 't': // thread number
                    sprintf(tempstr, "%d",(int)(unsigned long)
                            pthread_getspecific(tls_key_threadnr));
                    break;
                case 'C': // text_event
                    if (cnt->text_event_string && cnt->text_event_string[0])
                        snprintf(tempstr, PATH_MAX, "%s", cnt->text_event_string);
                    else
                        ++pos_userformat;
                    break;
                case 'f': // filename
                    if (filename)
                        snprintf(tempstr, PATH_MAX, "%s", filename);
                    else
                        ++pos_userformat;
                    break;
                case 'n': // sqltype
                    if (sqltype)
                        sprintf(tempstr, "%d", sqltype);
                    else
                        ++pos_userformat;
                    break;
                default: // Any other code is copied with the %-sign
                    *format++ = '%';
                    *format++ = *pos_userformat;
                    continue;
            }

            /* If a format specifier was found and used, copy the result from
             * 'tempstr' to 'format'.
             */
            if (tempstr[0]) {
                while ((*format = *tempstr++) != '\0')
                    ++format;
                continue;
            }
        }

        /* For any other character than % we just simply copy the character */
        *format++ = *pos_userformat;
    }

    *format = '\0';
    format = formatstring;

    return strftime(s, max, format, tm);
}

/**
 * motion_log
 *
 *    This routine is used for printing all informational, debug or error
 *    messages produced by any of the other motion functions.  It always
 *    produces a message of the form "[n] {message}", and (if the param
 *    'errno_flag' is set) follows the message with the associated error
 *    message from the library.
 *
 * Parameters:
 *
 *     level           logging level for the 'syslog' function
 *                     (-1 implies no syslog message should be produced)
 *     errno_flag      if set, the log message should be followed by the
 *                     error message.
 *     fmt             the format string for producing the message
 *     ap              variable-length argument list
 *
 * Returns:
 *                     Nothing
 */
void motion_log(int level, int errno_flag, const char *fmt, ...)
{
    int errno_save, n;
    char buf[1024];
#if (!defined(BSD)) && (!(_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE)
    char msg_buf[100];
#endif
    va_list ap;
    int threadnr;

    /* If pthread_getspecific fails (e.g., because the thread's TLS doesn't
     * contain anything for thread number, it returns NULL which casts to zero,
     * which is nice because that's what we want in that case.
     */
    threadnr = (unsigned long)pthread_getspecific(tls_key_threadnr);

    /*
     * First we save the current 'error' value.  This is required because
     * the subsequent calls to vsnprintf could conceivably change it!
     */
    errno_save = errno;

    /* Prefix the message with the thread number */
    n = snprintf(buf, sizeof(buf), "[%d] ", threadnr);

    /* Next add the user's message */
    va_start(ap, fmt);
    n += vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);

    /* If errno_flag is set, add on the library error message */
    if (errno_flag) {
        strncat(buf, ": ", 1024 - strlen(buf));
        n += 2;

        /*
         * this is bad - apparently gcc/libc wants to use the non-standard GNU
         * version of strerror_r, which doesn't actually put the message into
         * my buffer :-(.  I have put in a 'hack' to get around this.
         */
#if (defined(BSD))
        strerror_r(errno_save, buf + n, sizeof(buf) - n);    /* 2 for the ': ' */
#elif (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
        strerror_r(errno_save, buf + n, sizeof(buf) - n);
#else
        strncat(buf, strerror_r(errno_save, msg_buf, sizeof(msg_buf)), 1024 - strlen(buf));
#endif
    }
    /* If 'level' is not negative, send the message to the syslog */
    if (level >= 0)
        syslog(level, "%s", buf);

    /* For printing to stderr we need to add a newline */
    strcat(buf, "\n");
    fputs(buf, stderr);
    fflush(stderr);

    /* Clean up the argument list routine */
    va_end(ap);
}



