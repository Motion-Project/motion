/*    motion.c
 *
 *    Detect changes in a video stream.
 *    Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */
#include "translate.h"
#include "motion.h"
#include "ffmpeg.h"
#include "video_common.h"
#include "video_v4l2.h"
#include "video_loopback.h"
#include "conf.h"
#include "alg.h"
#include "track.h"
#include "event.h"
#include "picture.h"
#include "rotate.h"
#include "webu.h"


#define IMAGE_BUFFER_FLUSH ((unsigned int)-1)

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

/* Set this when we want main to end or restart */
volatile unsigned int finish = 0;

/* Log file used instead of stderr and syslog */
FILE *ptr_logfile = NULL;

/**
 * restart
 *
 *   Differentiates between a quit and a restart. When all threads have
 *   finished running, 'main' checks if 'restart' is true and if so starts
 *   up again (instead of just quitting).
 */
unsigned int restart = 0;


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
    /*
     * Only resize if :
     * Not in an event and
     * decreasing at last position in new buffer
     * increasing at last position in old buffer
     * e.g. at end of smallest buffer
     */
    if (cnt->event_nr != cnt->prev_event) {
        int smallest;

        if (new_size < cnt->imgs.image_ring_size)  /* Decreasing */
            smallest = new_size;
        else  /* Increasing */
            smallest = cnt->imgs.image_ring_size;

        if (cnt->imgs.image_ring_in == smallest - 1 || smallest == 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Resizing pre_capture buffer to %d items"), new_size);

            /* Create memory for new ring buffer */
            struct image_data *tmp;
            tmp = mymalloc(new_size * sizeof(struct image_data));

            /*
             * Copy all information from old to new
             * Smallest is 0 at initial init
             */
            if (smallest > 0)
                memcpy(tmp, cnt->imgs.image_ring, sizeof(struct image_data) * smallest);


            /* In the new buffers, allocate image memory */
            {
                int i;
                for(i = smallest; i < new_size; i++) {
                    tmp[i].image_norm = mymalloc(cnt->imgs.size_norm);
                    memset(tmp[i].image_norm, 0x80, cnt->imgs.size_norm);  /* initialize to grey */
                    if (cnt->imgs.size_high > 0){
                        tmp[i].image_high = mymalloc(cnt->imgs.size_high);
                        memset(tmp[i].image_high, 0x80, cnt->imgs.size_high);
                    }
                }
            }

            /* Free the old ring */
            free(cnt->imgs.image_ring);

            /* Point to the new ring */
            cnt->imgs.image_ring = tmp;
            cnt->current_image = NULL;

            cnt->imgs.image_ring_size = new_size;

            cnt->imgs.image_ring_in = 0;
            cnt->imgs.image_ring_out = 0;
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
    int i;

    /* Exit if don't have any ring */
    if (cnt->imgs.image_ring == NULL)
        return;

    /* Free all image buffers */
    for (i = 0; i < cnt->imgs.image_ring_size; i++){
        free(cnt->imgs.image_ring[i].image_norm);
        if (cnt->imgs.size_high >0 ) free(cnt->imgs.image_ring[i].image_high);
    }

    /* Free the ring */
    free(cnt->imgs.image_ring);

    cnt->imgs.image_ring = NULL;
    cnt->current_image = NULL;
    cnt->imgs.image_ring_size = 0;
}

/**
 * image_save_as_preview
 *
 * This routine is called when we detect motion and want to save an image in the preview buffer
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
    void *image_norm, *image_high;

    /* Save our pointers to our memory locations for images*/
    image_norm = cnt->imgs.preview_image.image_norm;
    image_high = cnt->imgs.preview_image.image_high;

    /* Copy over the meta data from the img into preview */
    memcpy(&cnt->imgs.preview_image, img, sizeof(struct image_data));

    /* Restore the pointers to the memory locations for images*/
    cnt->imgs.preview_image.image_norm = image_norm;
    cnt->imgs.preview_image.image_high = image_high;

    /* Copy the actual images for norm and high */
    memcpy(cnt->imgs.preview_image.image_norm, img->image_norm, cnt->imgs.size_norm);
    if (cnt->imgs.size_high > 0){
        memcpy(cnt->imgs.preview_image.image_high, img->image_high, cnt->imgs.size_high);
    }

    /*
     * If we set output_all to yes and during the event
     * there is no image with motion, diffs is 0, we are not going to save the preview event
     */
    if (cnt->imgs.preview_image.diffs == 0)
        cnt->imgs.preview_image.diffs = 1;

    /* draw locate box here when mode = LOCATE_PREVIEW */
    if (cnt->locate_motion_mode == LOCATE_PREVIEW) {

        if (cnt->locate_motion_style == LOCATE_BOX) {
            alg_draw_location(&img->location, &cnt->imgs, cnt->imgs.width, cnt->imgs.preview_image.image_norm,
                              LOCATE_BOX, LOCATE_NORMAL, cnt->process_thisframe);
        } else if (cnt->locate_motion_style == LOCATE_REDBOX) {
            alg_draw_red_location(&img->location, &cnt->imgs, cnt->imgs.width, cnt->imgs.preview_image.image_norm,
                                  LOCATE_REDBOX, LOCATE_NORMAL, cnt->process_thisframe);
        } else if (cnt->locate_motion_style == LOCATE_CROSS) {
            alg_draw_location(&img->location, &cnt->imgs, cnt->imgs.width, cnt->imgs.preview_image.image_norm,
                              LOCATE_CROSS, LOCATE_NORMAL, cnt->process_thisframe);
        } else if (cnt->locate_motion_style == LOCATE_REDCROSS) {
            alg_draw_red_location(&img->location, &cnt->imgs, cnt->imgs.width, cnt->imgs.preview_image.image_norm,
                                  LOCATE_REDCROSS, LOCATE_NORMAL, cnt->process_thisframe);
        }
    }
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
static void context_init(struct context *cnt)
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

    cnt->vdev = NULL;    /*Init to NULL to check loading parms vs web updates*/
    cnt->netcam = NULL;
    cnt->rtsp = NULL;
    cnt->rtsp_high = NULL;

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
    unsigned int j;

    /* Free memory allocated for config parameters */
    for (j = 0; config_params[j].param_name != NULL; j++) {
        if (config_params[j].copy == copy_string ||
            config_params[j].copy == copy_uri ||
            config_params[j].copy == read_camera_dir) {
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
    int i;

    /*The FALLTHROUGH is a special comment required by compiler.  Do not edit it*/
    switch(signo) {
    case SIGALRM:
        /*
         * Somebody (maybe we ourself) wants us to make a snapshot
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
        /* Trigger the end of a event */
        if (cnt_list) {
            i = -1;
            while (cnt_list[++i]){
                cnt_list[i]->event_stop = TRUE;
            }
        }
        break;
    case SIGHUP:
        restart = 1;
        /*
         * Fall through, as the value of 'restart' is the only difference
         * between SIGHUP and the ones below.
         */
         /*FALLTHROUGH*/
    case SIGINT:
        /*FALLTHROUGH*/
    case SIGQUIT:
        /*FALLTHROUGH*/
    case SIGTERM:
        /*
         * Somebody wants us to quit! We should finish the actual
         * movie and end up!
         */

        if (cnt_list) {
            i = -1;
            while (cnt_list[++i]) {
                cnt_list[i]->webcontrol_finish = TRUE;
                cnt_list[i]->event_stop = TRUE;
                cnt_list[i]->finish = 1;
                /*
                 * Don't restart thread when it ends,
                 * all threads restarts if global restart is set
                 */
                 cnt_list[i]->restart = 0;
            }
        }
        /*
         * Set flag we want to quit main check threads loop
         * if restart is set (above) we start up again
         */
        finish = 1;
        break;
    case SIGSEGV:
        exit(0);
    case SIGVTALRM:
        printf("SIGVTALRM went off\n");
        break;
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
 * setup_signals
 *   Attaches handlers to a number of signals that Motion need to catch.
 */
static void setup_signals(void){
    /*
     * Setup signals and do some initialization. 1 in the call to
     * 'motion_startup' means that Motion will become a daemon if so has been
     * requested, and argc and argc are necessary for reading the command
     * line options.
     */
    struct sigaction sig_handler_action;
    struct sigaction sigchild_action;

#ifdef SA_NOCLDWAIT
    sigchild_action.sa_flags = SA_NOCLDWAIT;
#else
    sigchild_action.sa_flags = 0;
#endif
    sigchild_action.sa_handler = sigchild_handler;
    sigemptyset(&sigchild_action.sa_mask);
#ifdef SA_RESTART
    sig_handler_action.sa_flags = SA_RESTART;
#else
    sig_handler_action.sa_flags = 0;
#endif
    sig_handler_action.sa_handler = sig_handler;
    sigemptyset(&sig_handler_action.sa_mask);

    /* Enable automatic zombie reaping */
    sigaction(SIGCHLD, &sigchild_action, NULL);
    sigaction(SIGPIPE, &sigchild_action, NULL);
    sigaction(SIGALRM, &sig_handler_action, NULL);
    sigaction(SIGHUP, &sig_handler_action, NULL);
    sigaction(SIGINT, &sig_handler_action, NULL);
    sigaction(SIGQUIT, &sig_handler_action, NULL);
    sigaction(SIGTERM, &sig_handler_action, NULL);
    sigaction(SIGUSR1, &sig_handler_action, NULL);

    /* use SIGVTALRM as a way to break out of the ioctl, don't restart */
    sig_handler_action.sa_flags = 0;
    sigaction(SIGVTALRM, &sig_handler_action, NULL);
}

/**
 * motion_remove_pid
 *   This function remove the process id file ( pid file ) before motion exit.
 */
static void motion_remove_pid(void)
{
    if ((cnt_list[0]->daemon) && (cnt_list[0]->conf.pid_file) && (restart == 0)) {
        if (!unlink(cnt_list[0]->conf.pid_file))
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Removed process id file (pid file)."));
        else
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Error removing pid file"));
    }

    if (ptr_logfile) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Closing logfile (%s)."),
                   cnt_list[0]->conf.log_file);
        myfclose(ptr_logfile);
        set_log_mode(LOGMODE_NONE);
        ptr_logfile = NULL;
    }

}

/**
 * motion_detected
 *
 *   Called from 'motion_loop' when motion is detected
 *   Can be called when no motion if emulate_motion is set!
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
    int indx;

    /* Draw location */
    if (cnt->locate_motion_mode == LOCATE_ON) {

        if (cnt->locate_motion_style == LOCATE_BOX) {
            alg_draw_location(location, imgs, imgs->width, img->image_norm, LOCATE_BOX,
                              LOCATE_BOTH, cnt->process_thisframe);
        } else if (cnt->locate_motion_style == LOCATE_REDBOX) {
            alg_draw_red_location(location, imgs, imgs->width, img->image_norm, LOCATE_REDBOX,
                                  LOCATE_BOTH, cnt->process_thisframe);
        } else if (cnt->locate_motion_style == LOCATE_CROSS) {
            alg_draw_location(location, imgs, imgs->width, img->image_norm, LOCATE_CROSS,
                              LOCATE_BOTH, cnt->process_thisframe);
        } else if (cnt->locate_motion_style == LOCATE_REDCROSS) {
            alg_draw_red_location(location, imgs, imgs->width, img->image_norm, LOCATE_REDCROSS,
                                  LOCATE_BOTH, cnt->process_thisframe);
        }
    }

    /* Calculate how centric motion is if configured preview center*/
    if (cnt->new_img & NEWIMG_CENTER) {
        unsigned int distX = abs((imgs->width / 2) - location->x);
        unsigned int distY = abs((imgs->height / 2) - location->y);

        img->cent_dist = distX * distX + distY * distY;
    }


    /* Do things only if we have got minimum_motion_frames */
    if (img->flags & IMAGE_TRIGGER) {
        /* Take action if this is a new event and we have a trigger image */
        if (cnt->event_nr != cnt->prev_event) {
            /*
             * Reset prev_event number to current event and save event time
             * in both time_t and struct tm format.
             */
            cnt->prev_event = cnt->event_nr;
            cnt->eventtime = img->timestamp_tv.tv_sec;
            localtime_r(&cnt->eventtime, cnt->eventtime_tm);

            /*
             * Since this is a new event we create the event_text_string used for
             * the %C conversion specifier. We may already need it for
             * on_motion_detected_commend so it must be done now.
             */
            mystrftime(cnt, cnt->text_event_string, sizeof(cnt->text_event_string),
                       cnt->conf.text_event, &img->timestamp_tv, NULL, 0);

            /* EVENT_FIRSTMOTION triggers on_event_start_command and event_ffmpeg_newfile */

            indx = cnt->imgs.image_ring_out-1;
            do {
                indx++;
                if (indx == cnt->imgs.image_ring_size) indx = 0;
                if ((cnt->imgs.image_ring[indx].flags & (IMAGE_SAVE | IMAGE_SAVED)) == IMAGE_SAVE){
                    event(cnt, EVENT_FIRSTMOTION, img, NULL, NULL, &cnt->imgs.image_ring[indx].timestamp_tv);
                    indx = cnt->imgs.image_ring_in;
                }
            } while (indx != cnt->imgs.image_ring_in);

            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motion detected - starting event %d"),
                       cnt->event_nr);

            /* always save first motion frame as preview-shot, may be changed to an other one later */
            if (cnt->new_img & (NEWIMG_FIRST | NEWIMG_BEST | NEWIMG_CENTER))
                image_save_as_preview(cnt, img);

        }

        /* EVENT_MOTION triggers event_beep and on_motion_detected_command */
        event(cnt, EVENT_MOTION, NULL, NULL, NULL, &img->timestamp_tv);
    }

    /* Limit framerate */
    if (img->shot < conf->framerate) {
        /*
         * If config option stream_motion is enabled, send the latest motion detected image
         * to the stream but only if it is not the first shot within a second. This is to
         * avoid double frames since we already have sent a frame to the stream.
         * We also disable this in setup_mode.
         */
        if (conf->stream_motion && !conf->setup_mode && img->shot != 1)
            event(cnt, EVENT_STREAM, img, NULL, NULL, &img->timestamp_tv);

        /*
         * Save motion jpeg, if configured
         * Output the image_out (motion) picture.
         */
        if (conf->picture_output_motion)
            event(cnt, EVENT_IMAGEM_DETECTED, NULL, NULL, NULL, &img->timestamp_tv);
    }

    /* if track enabled and auto track on */
    if (cnt->track.type && cnt->track.active)
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

static void process_image_ring(struct context *cnt, unsigned int max_images)
{
    /*
     * We are going to send an event, in the events there is still
     * some code that use cnt->current_image
     * so set it temporary to our image
     */
    struct image_data *saved_current_image = cnt->current_image;

    /* If image is flaged to be saved and not saved yet, process it */
    do {
        /* Check if we should save/send this image, breakout if not */
        assert(cnt->imgs.image_ring_out < cnt->imgs.image_ring_size);
        if ((cnt->imgs.image_ring[cnt->imgs.image_ring_out].flags & (IMAGE_SAVE | IMAGE_SAVED)) != IMAGE_SAVE)
            break;

        /* Set inte global context that we are working with this image */
        cnt->current_image = &cnt->imgs.image_ring[cnt->imgs.image_ring_out];

        if (cnt->imgs.image_ring[cnt->imgs.image_ring_out].shot < cnt->conf.framerate) {
            if (cnt->log_level >= DBG) {
                char tmp[32];
                const char *t;

                if (cnt->imgs.image_ring[cnt->imgs.image_ring_out].flags & IMAGE_TRIGGER)
                    t = "Trigger";
                else if (cnt->imgs.image_ring[cnt->imgs.image_ring_out].flags & IMAGE_MOTION)
                    t = "Motion";
                else if (cnt->imgs.image_ring[cnt->imgs.image_ring_out].flags & IMAGE_PRECAP)
                    t = "Precap";
                else if (cnt->imgs.image_ring[cnt->imgs.image_ring_out].flags & IMAGE_POSTCAP)
                    t = "Postcap";
                else
                    t = "Other";

                mystrftime(cnt, tmp, sizeof(tmp), "%H%M%S-%q",
                           &cnt->imgs.image_ring[cnt->imgs.image_ring_out].timestamp_tv, NULL, 0);
                draw_text(cnt->imgs.image_ring[cnt->imgs.image_ring_out].image_norm,
                          cnt->imgs.width, cnt->imgs.height, 10, 20, tmp, cnt->text_scale);
                draw_text(cnt->imgs.image_ring[cnt->imgs.image_ring_out].image_norm,
                          cnt->imgs.width, cnt->imgs.height, 10, 30, t, cnt->text_scale);
            }

            /* Output the picture to jpegs and ffmpeg */
            event(cnt, EVENT_IMAGE_DETECTED,
              &cnt->imgs.image_ring[cnt->imgs.image_ring_out], NULL, NULL,
              &cnt->imgs.image_ring[cnt->imgs.image_ring_out].timestamp_tv);


            /*
             * Check if we must add any "filler" frames into movie to keep up fps
             * Only if we are recording videos ( ffmpeg or extenal pipe )
             * While the overall elapsed time might be correct, if there are
             * many duplicated frames, say 10 fps, 5 duplicated, the video will
             * look like it is frozen every second for half a second.
             */
            if (!cnt->conf.movie_duplicate_frames) {
                /* don't duplicate frames */
            } else if ((cnt->imgs.image_ring[cnt->imgs.image_ring_out].shot == 0) &&
                (cnt->ffmpeg_output || (cnt->conf.movie_extpipe_use && cnt->extpipe))) {
                /*
                 * movie_last_shoot is -1 when file is created,
                 * we don't know how many frames there is in first sec
                 */
                if (cnt->movie_last_shot >= 0) {
                    if (cnt_list[0]->log_level >= DBG) {
                        int frames = cnt->movie_fps - (cnt->movie_last_shot + 1);
                        if (frames > 0) {
                            char tmp[25];
                            MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO
                            ,_("Added %d fillerframes into movie"), frames);
                            sprintf(tmp, "Fillerframes %d", frames);
                            draw_text(cnt->imgs.image_ring[cnt->imgs.image_ring_out].image_norm,
                                      cnt->imgs.width, cnt->imgs.height, 10, 40, tmp, cnt->text_scale);
                        }
                    }
                    /* Check how many frames it was last sec */
                    while ((cnt->movie_last_shot + 1) < cnt->movie_fps) {
                        /* Add a filler frame into encoder */
                        event(cnt, EVENT_FFMPEG_PUT,
                          &cnt->imgs.image_ring[cnt->imgs.image_ring_out], NULL, NULL,
                          &cnt->imgs.image_ring[cnt->imgs.image_ring_out].timestamp_tv);

                        cnt->movie_last_shot++;
                    }
                }
                cnt->movie_last_shot = 0;
            } else if (cnt->imgs.image_ring[cnt->imgs.image_ring_out].shot != (cnt->movie_last_shot + 1)) {
                /* We are out of sync! Propably we got motion - no motion - motion */
                cnt->movie_last_shot = -1;
            }

            /*
             * Save last shot added to movie
             * only when we not are within first sec
             */
            if (cnt->movie_last_shot >= 0)
                cnt->movie_last_shot = cnt->imgs.image_ring[cnt->imgs.image_ring_out].shot;
        }

        /* Mark the image as saved */
        cnt->imgs.image_ring[cnt->imgs.image_ring_out].flags |= IMAGE_SAVED;

        /* Store it as a preview image, only if it has motion */
        if (cnt->imgs.image_ring[cnt->imgs.image_ring_out].flags & IMAGE_MOTION) {
            /* Check for most significant preview-shot when picture_output=best */
            if (cnt->new_img & NEWIMG_BEST) {
                if (cnt->imgs.image_ring[cnt->imgs.image_ring_out].diffs > cnt->imgs.preview_image.diffs) {
                    image_save_as_preview(cnt, &cnt->imgs.image_ring[cnt->imgs.image_ring_out]);
                }
            }
            /* Check for most significant preview-shot when picture_output=center */
            if (cnt->new_img & NEWIMG_CENTER) {
                if (cnt->imgs.image_ring[cnt->imgs.image_ring_out].cent_dist < cnt->imgs.preview_image.cent_dist) {
                    image_save_as_preview(cnt, &cnt->imgs.image_ring[cnt->imgs.image_ring_out]);
                }
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

static int init_camera_type(struct context *cnt){

    cnt->camera_type = CAMERA_TYPE_UNKNOWN;

    #ifdef HAVE_MMAL
        if (cnt->conf.mmalcam_name) {
            cnt->camera_type = CAMERA_TYPE_MMAL;
            return 0;
        }
    #endif // HAVE_MMAL

    if (cnt->conf.netcam_url) {
        if ((strncmp(cnt->conf.netcam_url,"mjpeg",5) == 0) ||
            (strncmp(cnt->conf.netcam_url,"v4l2" ,4) == 0) ||
            (strncmp(cnt->conf.netcam_url,"file" ,4) == 0) ||
            (strncmp(cnt->conf.netcam_url,"rtmp" ,4) == 0) ||
            (strncmp(cnt->conf.netcam_url,"rtsp" ,4) == 0)) {
            cnt->camera_type = CAMERA_TYPE_RTSP;
        } else {
            cnt->camera_type = CAMERA_TYPE_NETCAM;
        }
        return 0;
    }

    #ifdef HAVE_BKTR
        if (strncmp(cnt->conf.video_device,"/dev/bktr",9) == 0) {
            cnt->camera_type = CAMERA_TYPE_BKTR;
            return 0;
        }
    #endif // HAVE_BKTR

    #ifdef HAVE_V4L2
        if (cnt->conf.video_device) {
            cnt->camera_type = CAMERA_TYPE_V4L2;
            return 0;
        }
    #endif // HAVE_V4L2


    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
        , _("Unable to determine camera type (MMAL, Netcam, V4L2, BKTR)"));
    return -1;

}

static void init_mask_privacy(struct context *cnt){

    int indxrow, indxcol;
    int start_cr, offset_cb, start_cb;
    int y_index, uv_index;
    int indx_img, indx_max;         /* Counter and max for norm/high */
    int indx_width, indx_height;
    unsigned char *img_temp, *img_temp_uv;


    FILE *picture;

    /* Load the privacy file if any */
    cnt->imgs.mask_privacy = NULL;
    cnt->imgs.mask_privacy_uv = NULL;
    cnt->imgs.mask_privacy_high = NULL;
    cnt->imgs.mask_privacy_high_uv = NULL;

    if (cnt->conf.mask_privacy) {
        if ((picture = myfopen(cnt->conf.mask_privacy, "r"))) {
            MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("Opening privacy mask file"));
            /*
             * NOTE: The mask is expected to have the output dimensions. I.e., the mask
             * applies to the already rotated image, not the capture image. Thus, use
             * width and height from imgs.
             */
            cnt->imgs.mask_privacy = get_pgm(picture, cnt->imgs.width, cnt->imgs.height);

            /* We only need the "or" mask for the U & V chrominance area.  */
            cnt->imgs.mask_privacy_uv = mymalloc((cnt->imgs.height * cnt->imgs.width) / 2);
            if (cnt->imgs.size_high > 0){
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
                    ,_("Opening high resolution privacy mask file"));
                rewind(picture);
                cnt->imgs.mask_privacy_high = get_pgm(picture, cnt->imgs.width_high, cnt->imgs.height_high);
                cnt->imgs.mask_privacy_high_uv = mymalloc((cnt->imgs.height_high * cnt->imgs.width_high) / 2);
            }

            myfclose(picture);
        } else {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                ,_("Error opening mask file %s"), cnt->conf.mask_privacy);
            /* Try to write an empty mask file to make it easier for the user to edit it */
            put_fixed_mask(cnt, cnt->conf.mask_privacy);
        }

        if (!cnt->imgs.mask_privacy) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("Failed to read mask privacy image. Mask privacy feature disabled."));
        } else {
            MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
            ,_("Mask privacy file \"%s\" loaded."), cnt->conf.mask_privacy);

            indx_img = 1;
            indx_max = 1;
            if (cnt->imgs.size_high > 0) indx_max = 2;

            while (indx_img <= indx_max){
                if (indx_img == 1){
                    start_cr = (cnt->imgs.height * cnt->imgs.width);
                    offset_cb = ((cnt->imgs.height * cnt->imgs.width)/4);
                    start_cb = start_cr + offset_cb;
                    indx_width = cnt->imgs.width;
                    indx_height = cnt->imgs.height;
                    img_temp = cnt->imgs.mask_privacy;
                    img_temp_uv = cnt->imgs.mask_privacy_uv;
                } else {
                    start_cr = (cnt->imgs.height_high * cnt->imgs.width_high);
                    offset_cb = ((cnt->imgs.height_high * cnt->imgs.width_high)/4);
                    start_cb = start_cr + offset_cb;
                    indx_width = cnt->imgs.width_high;
                    indx_height = cnt->imgs.height_high;
                    img_temp = cnt->imgs.mask_privacy_high;
                    img_temp_uv = cnt->imgs.mask_privacy_high_uv;
                }

                for (indxrow = 0; indxrow < indx_height; indxrow++) {
                    for (indxcol = 0; indxcol < indx_width; indxcol++) {
                        y_index = indxcol + (indxrow * indx_width);
                        if (img_temp[y_index] == 0xff) {
                            if ((indxcol % 2 == 0) && (indxrow % 2 == 0) ){
                                uv_index = (indxcol/2) + ((indxrow * indx_width)/4);
                                img_temp[start_cr + uv_index] = 0xff;
                                img_temp[start_cb + uv_index] = 0xff;
                                img_temp_uv[uv_index] = 0x00;
                                img_temp_uv[offset_cb + uv_index] = 0x00;
                            }
                        } else {
                            img_temp[y_index] = 0x00;
                            if ((indxcol % 2 == 0) && (indxrow % 2 == 0) ){
                                uv_index = (indxcol/2) + ((indxrow * indx_width)/4);
                                img_temp[start_cr + uv_index] = 0x00;
                                img_temp[start_cb + uv_index] = 0x00;
                                img_temp_uv[uv_index] = 0x80;
                                img_temp_uv[offset_cb + uv_index] = 0x80;
                            }
                        }
                    }
                }
                indx_img++;
            }
        }
    }

}

static void init_text_scale(struct context *cnt){

    /* Consider that web interface may change conf values at any moment.
     * The below can put two sections in the image so make sure that after
     * scaling does not occupy more than 1/4 of image (10 pixels * 2 lines)
     */

    cnt->text_scale = cnt->conf.text_scale;
    if (cnt->text_scale <= 0) cnt->text_scale = 1;

    if ((cnt->text_scale * 10 * 2) > (cnt->imgs.width / 4)) {
        cnt->text_scale = (cnt->imgs.width / (4 * 10 * 2));
        if (cnt->text_scale <= 0) cnt->text_scale = 1;
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Invalid text scale.  Adjusted to %d"), cnt->text_scale);
    }

    if ((cnt->text_scale * 10 * 2) > (cnt->imgs.height / 4)) {
        cnt->text_scale = (cnt->imgs.height / (4 * 10 * 2));
        if (cnt->text_scale <= 0) cnt->text_scale = 1;
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Invalid text scale.  Adjusted to %d"), cnt->text_scale);
    }

    /* If we had to modify the scale, change conf so we don't get another message */
    cnt->conf.text_scale = cnt->text_scale;

}

static void mot_stream_init(struct context *cnt){

    /* The image buffers are allocated in event_stream_put if needed*/
    pthread_mutex_init(&cnt->mutex_stream, NULL);

    cnt->imgs.substream_image = NULL;

    cnt->stream_norm.jpeg_size = 0;
    cnt->stream_norm.jpeg_data = NULL;
    cnt->stream_norm.cnct_count = 0;

    cnt->stream_sub.jpeg_size = 0;
    cnt->stream_sub.jpeg_data = NULL;
    cnt->stream_sub.cnct_count = 0;

    cnt->stream_motion.jpeg_size = 0;
    cnt->stream_motion.jpeg_data = NULL;
    cnt->stream_motion.cnct_count = 0;

    cnt->stream_source.jpeg_size = 0;
    cnt->stream_source.jpeg_data = NULL;
    cnt->stream_source.cnct_count = 0;

}

static void mot_stream_deinit(struct context *cnt){

    /* Need to check whether buffers were allocated since init
     * function defers the allocations to event_stream_put
    */

    pthread_mutex_destroy(&cnt->mutex_stream);

    if (cnt->imgs.substream_image != NULL){
        free(cnt->imgs.substream_image);
        cnt->imgs.substream_image = NULL;
    }

    if (cnt->stream_norm.jpeg_data != NULL){
        free(cnt->stream_norm.jpeg_data);
        cnt->stream_norm.jpeg_data = NULL;
    }

    if (cnt->stream_sub.jpeg_data != NULL){
        free(cnt->stream_sub.jpeg_data);
        cnt->stream_sub.jpeg_data = NULL;
    }

    if (cnt->stream_motion.jpeg_data != NULL){
        free(cnt->stream_motion.jpeg_data);
        cnt->stream_motion.jpeg_data = NULL;
    }

    if (cnt->stream_source.jpeg_data != NULL){
        free(cnt->stream_source.jpeg_data);
        cnt->stream_source.jpeg_data = NULL;
    }
}

/* TODO: dbse functions are to be moved to separate module in future change*/
static void dbse_global_deinit(void){
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, _("Closing MYSQL"));
    #if defined(HAVE_MYSQL) || defined(HAVE_MARIADB)
        mysql_library_end();
    #endif /* HAVE_MYSQL HAVE_MARIADB */

}

static void dbse_global_init(void){

    MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("Initializing database"));
   /* Initialize all the database items */
    #if defined(HAVE_MYSQL) || defined(HAVE_MARIADB)
        if (mysql_library_init(0, NULL, NULL)) {
            fprintf(stderr, "could not initialize MySQL library\n");
            exit(1);
        }
    #endif /* HAVE_MYSQL HAVE_MARIADB */

    #ifdef HAVE_SQLITE3
        int indx;
        /* database_sqlite3 == NULL if not changed causes each thread to create their own
        * sqlite3 connection this will only happens when using a non-threaded sqlite version */
        cnt_list[0]->database_sqlite3=NULL;
        if (cnt_list[0]->conf.database_type && ((!strcmp(cnt_list[0]->conf.database_type, "sqlite3")) && cnt_list[0]->conf.database_dbname)) {
            MOTION_LOG(NTC, TYPE_DB, NO_ERRNO
                ,_("SQLite3 Database filename %s")
                ,cnt_list[0]->conf.database_dbname);

            int thread_safe = sqlite3_threadsafe();
            if (thread_safe > 0) {
                MOTION_LOG(NTC, TYPE_DB, NO_ERRNO, _("SQLite3 is threadsafe"));
                MOTION_LOG(NTC, TYPE_DB, NO_ERRNO, _("SQLite3 serialized %s")
                    ,(sqlite3_config(SQLITE_CONFIG_SERIALIZED)?_("FAILED"):_("SUCCESS")));
                if (sqlite3_open( cnt_list[0]->conf.database_dbname, &cnt_list[0]->database_sqlite3) != SQLITE_OK) {
                    MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                        ,_("Can't open database %s : %s")
                        ,cnt_list[0]->conf.database_dbname
                        ,sqlite3_errmsg( cnt_list[0]->database_sqlite3));
                    sqlite3_close( cnt_list[0]->database_sqlite3);
                    exit(1);
                }
                MOTION_LOG(NTC, TYPE_DB, NO_ERRNO,_("database_busy_timeout %d msec"),
                        cnt_list[0]->conf.database_busy_timeout);
                if (sqlite3_busy_timeout( cnt_list[0]->database_sqlite3,  cnt_list[0]->conf.database_busy_timeout) != SQLITE_OK)
                    MOTION_LOG(ERR, TYPE_DB, NO_ERRNO,_("database_busy_timeout failed %s")
                        ,sqlite3_errmsg( cnt_list[0]->database_sqlite3));
            }
        }
        /* Cascade to all threads */
        indx = 1;
        while (cnt_list[indx] != NULL) {
            cnt_list[indx]->database_sqlite3 = cnt_list[0]->database_sqlite3;
            indx++;
        }

    #endif /* HAVE_SQLITE3 */

}

static int dbse_init_mysql(struct context *cnt){

    #if defined(HAVE_MYSQL) || defined(HAVE_MARIADB)
        int dbport;
        if ((!strcmp(cnt->conf.database_type, "mysql")) && (cnt->conf.database_dbname)) {
            cnt->database_event_id = 0;
            cnt->database = mymalloc(sizeof(MYSQL));
            mysql_init(cnt->database);
            if ((cnt->conf.database_port < 0) || (cnt->conf.database_port > 65535)){
                dbport = 0;
            } else {
                dbport = cnt->conf.database_port;
            }
            if (!mysql_real_connect(cnt->database, cnt->conf.database_host, cnt->conf.database_user,
                cnt->conf.database_password, cnt->conf.database_dbname, dbport, NULL, 0)) {
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                    ,_("Cannot connect to MySQL database %s on host %s with user %s")
                    ,cnt->conf.database_dbname, cnt->conf.database_host
                    ,cnt->conf.database_user);
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                    ,_("MySQL error was %s"), mysql_error(cnt->database));
                return -2;
            }
            #if (defined(MYSQL_VERSION_ID)) && (MYSQL_VERSION_ID > 50012)
                int my_true = TRUE;
                mysql_options(cnt->database, MYSQL_OPT_RECONNECT, &my_true);
            #endif
        }
    #else
        (void)cnt;  /* Avoid compiler warnings */
    #endif /* HAVE_MYSQL HAVE_MARIADB */

    return 0;

}

static int dbse_init_sqlite3(struct context *cnt){
    #ifdef HAVE_SQLITE3
        if (cnt_list[0]->database_sqlite3 != 0) {
            MOTION_LOG(NTC, TYPE_DB, NO_ERRNO,_("SQLite3 using shared handle"));
            cnt->database_sqlite3 = cnt_list[0]->database_sqlite3;

        } else if ((!strcmp(cnt->conf.database_type, "sqlite3")) && cnt->conf.database_dbname) {
            MOTION_LOG(NTC, TYPE_DB, NO_ERRNO
                ,_("SQLite3 Database filename %s"), cnt->conf.database_dbname);
            if (sqlite3_open(cnt->conf.database_dbname, &cnt->database_sqlite3) != SQLITE_OK) {
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                    ,_("Can't open database %s : %s")
                    ,cnt->conf.database_dbname, sqlite3_errmsg(cnt->database_sqlite3));
                sqlite3_close(cnt->database_sqlite3);
                return -2;
            }
            MOTION_LOG(NTC, TYPE_DB, NO_ERRNO
                ,_("database_busy_timeout %d msec"), cnt->conf.database_busy_timeout);
            if (sqlite3_busy_timeout(cnt->database_sqlite3, cnt->conf.database_busy_timeout) != SQLITE_OK)
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                    ,_("database_busy_timeout failed %s")
                    ,sqlite3_errmsg(cnt->database_sqlite3));
        }
    #else
        (void)cnt;  /* Avoid compiler warnings */
    #endif /* HAVE_SQLITE3 */

    return 0;

}

static int dbse_init_pgsql(struct context *cnt){
    #ifdef HAVE_PGSQL
        if ((!strcmp(cnt->conf.database_type, "postgresql")) && (cnt->conf.database_dbname)) {
            char connstring[255];

            /*
             * Create the connection string.
             * Quote the values so we can have null values (blank)
             */
            snprintf(connstring, 255,
                     "dbname='%s' host='%s' user='%s' password='%s' port='%d'",
                      cnt->conf.database_dbname, /* dbname */
                      (cnt->conf.database_host ? cnt->conf.database_host : ""), /* host (may be blank) */
                      (cnt->conf.database_user ? cnt->conf.database_user : ""), /* user (may be blank) */
                      (cnt->conf.database_password ? cnt->conf.database_password : ""), /* password (may be blank) */
                      cnt->conf.database_port
            );

            cnt->database_pg = PQconnectdb(connstring);
            if (PQstatus(cnt->database_pg) == CONNECTION_BAD) {
                MOTION_LOG(ERR, TYPE_DB, NO_ERRNO
                ,_("Connection to PostgreSQL database '%s' failed: %s")
                ,cnt->conf.database_dbname, PQerrorMessage(cnt->database_pg));
                return -2;
            }
        }
    #else
        (void)cnt;  /* Avoid compiler warnings */
    #endif /* HAVE_PGSQL */

    return 0;
}

static int dbse_init(struct context *cnt){
    int retcd = 0;

    if (cnt->conf.database_type) {
        MOTION_LOG(NTC, TYPE_DB, NO_ERRNO
            ,_("Database backend %s"), cnt->conf.database_type);

        retcd = dbse_init_mysql(cnt);
        if (retcd != 0) return retcd;

        retcd = dbse_init_sqlite3(cnt);
        if (retcd != 0) return retcd;

        retcd = dbse_init_pgsql(cnt);
        if (retcd != 0) return retcd;

        /* Set the sql mask file according to the SQL config options*/
        cnt->sql_mask = cnt->conf.sql_log_picture * (FTYPE_IMAGE + FTYPE_IMAGE_MOTION) +
                        cnt->conf.sql_log_snapshot * FTYPE_IMAGE_SNAPSHOT +
                        cnt->conf.sql_log_movie * (FTYPE_MPEG + FTYPE_MPEG_MOTION) +
                        cnt->conf.sql_log_timelapse * FTYPE_MPEG_TIMELAPSE;
    }

    return retcd;
}

static void dbse_deinit(struct context *cnt){
    if (cnt->conf.database_type) {
        #if defined(HAVE_MYSQL) || defined(HAVE_MARIADB)
            if ( (!strcmp(cnt->conf.database_type, "mysql")) && (cnt->conf.database_dbname)) {
                mysql_thread_end();
                mysql_close(cnt->database);
                cnt->database_event_id = 0;
            }
        #endif /* HAVE_MYSQL HAVE_MARIADB */

        #ifdef HAVE_PGSQL
                if ((!strcmp(cnt->conf.database_type, "postgresql")) && (cnt->conf.database_dbname)) {
                    PQfinish(cnt->database_pg);
                }
        #endif /* HAVE_PGSQL */

        #ifdef HAVE_SQLITE3
                /* Close the SQLite database */
                if ((!strcmp(cnt->conf.database_type, "sqlite3")) && (cnt->conf.database_dbname)) {
                    sqlite3_close(cnt->database_sqlite3);
                    cnt->database_sqlite3 = NULL;
                }
        #endif /* HAVE_SQLITE3 */
        (void)cnt;
    }
}

static void dbse_sqlmask_update(struct context *cnt){
    /*
    * Set the sql mask file according to the SQL config options
    * We update it for every frame in case the config was updated
    * via remote control.
    */
    cnt->sql_mask = cnt->conf.sql_log_picture * (FTYPE_IMAGE + FTYPE_IMAGE_MOTION) +
                    cnt->conf.sql_log_snapshot * FTYPE_IMAGE_SNAPSHOT +
                    cnt->conf.sql_log_movie * (FTYPE_MPEG + FTYPE_MPEG_MOTION) +
                    cnt->conf.sql_log_timelapse * FTYPE_MPEG_TIMELAPSE;
}

/**
 * motion_init
 *
 * This routine is called from motion_loop (the main thread of the program) to do
 * all of the initialization required before starting the actual run.
 *
 * Parameters:
 *
 *      cnt     Pointer to the motion context structure
 *
 * Returns:     0 OK
 *             -1 Fatal error, open loopback error
 *             -2 Fatal error, open SQL database error
 *             -3 Fatal error, image dimensions are not modulo 8
 */
static int motion_init(struct context *cnt)
{
    FILE *picture;
    int indx, retcd;

    util_threadname_set("ml",cnt->threadnr,cnt->conf.camera_name);

    /* Store thread number in TLS. */
    pthread_setspecific(tls_key_threadnr, (void *)((unsigned long)cnt->threadnr));

    cnt->currenttime_tm = mymalloc(sizeof(struct tm));
    cnt->eventtime_tm = mymalloc(sizeof(struct tm));
    /* Init frame time */
    cnt->currenttime = time(NULL);
    localtime_r(&cnt->currenttime, cnt->currenttime_tm);

    cnt->smartmask_speed = 0;

    /*
     * We initialize cnt->event_nr to 1 and cnt->prev_event to 0 (not really needed) so
     * that certain code below does not run until motion has been detected the first time
     */
    cnt->event_nr = 1;
    cnt->prev_event = 0;
    cnt->lightswitch_framecounter = 0;
    cnt->detecting_motion = 0;
    cnt->event_user = FALSE;
    cnt->event_stop = FALSE;

    /* Make sure to default the high res to zero */
    cnt->imgs.width_high = 0;
    cnt->imgs.height_high = 0;
    cnt->imgs.size_high = 0;
    cnt->movie_passthrough = cnt->conf.movie_passthrough;

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
        ,_("Camera %d started: motion detection %s"),
        cnt->camera_id, cnt->pause ? _("Disabled"):_("Enabled"));

    if (!cnt->conf.target_dir)
        cnt->conf.target_dir = mystrdup(".");

    if (init_camera_type(cnt) != 0 ) return -3;

    if ((cnt->camera_type != CAMERA_TYPE_RTSP) &&
        (cnt->movie_passthrough)) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Pass-through processing disabled."));
        cnt->movie_passthrough = FALSE;
    }

    if ((cnt->conf.height == 0) || (cnt->conf.width == 0)) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Invalid configuration dimensions %dx%d"),cnt->conf.height,cnt->conf.width);
        cnt->conf.height = DEF_HEIGHT;
        cnt->conf.width = DEF_WIDTH;
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Using default dimensions %dx%d"),cnt->conf.height,cnt->conf.width);
    }
    if (cnt->conf.width % 8) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image width (%d) requested is not modulo 8."), cnt->conf.width);
        cnt->conf.width = cnt->conf.width - (cnt->conf.width % 8) + 8;
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Adjusting width to next higher multiple of 8 (%d)."), cnt->conf.width);
    }
    if (cnt->conf.height % 8) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image height (%d) requested is not modulo 8."), cnt->conf.height);
        cnt->conf.height = cnt->conf.height - (cnt->conf.height % 8) + 8;
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Adjusting height to next higher multiple of 8 (%d)."), cnt->conf.height);
    }
    if (cnt->conf.width  < 64) cnt->conf.width  = 64;
    if (cnt->conf.height < 64) cnt->conf.height = 64;

    if (cnt->conf.netcam_decoder != NULL){
        cnt->netcam_decoder = mymalloc(strlen(cnt->conf.netcam_decoder)+1);
        retcd = snprintf(cnt->netcam_decoder,strlen(cnt->conf.netcam_decoder)+1
            ,"%s",cnt->conf.netcam_decoder);
        if (retcd < 0){
            free(cnt->netcam_decoder);
            cnt->netcam_decoder = NULL;
        }
    } else {
        cnt->netcam_decoder = NULL;
    }


    /* set the device settings */
    cnt->video_dev = vid_start(cnt);

    /*
     * We failed to get an initial image from a camera
     * So we need to guess height and width based on the config
     * file options.
     */
    if (cnt->video_dev == -1) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Could not fetch initial image from camera "));
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Motion continues using width and height from config file(s)"));
        cnt->imgs.width = cnt->conf.width;
        cnt->imgs.height = cnt->conf.height;
        cnt->imgs.size_norm = cnt->conf.width * cnt->conf.height * 3 / 2;
        cnt->imgs.motionsize = cnt->conf.width * cnt->conf.height;
    } else if (cnt->video_dev == -2) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Could not fetch initial image from camera "));
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Motion only supports width and height modulo 8"));
        return -3;
    }
    /* Revalidate we got a valid image size */
    if ((cnt->imgs.width % 8) || (cnt->imgs.height % 8)) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image width (%d) or height(%d) requested is not modulo 8.")
            ,cnt->imgs.width, cnt->imgs.height);
        return -3;
    }
    if ((cnt->imgs.width  < 64) || (cnt->imgs.height < 64)){
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Motion only supports width and height greater than or equal to 64 %dx%d")
            ,cnt->imgs.width, cnt->imgs.height);
            return -3;
    }
    /* Substream size notification*/
    if ((cnt->imgs.width % 16) || (cnt->imgs.height % 16)) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Substream not available.  Image sizes not modulo 16."));
    }


    /* We set size_high here so that it can be used in the retry function to determine whether
     * we need to break and reallocate buffers
     */
    cnt->imgs.size_high = (cnt->imgs.width_high * cnt->imgs.height_high * 3) / 2;

    image_ring_resize(cnt, 1); /* Create a initial precapture ring buffer with 1 frame */

    cnt->imgs.ref = mymalloc(cnt->imgs.size_norm);
    cnt->imgs.img_motion.image_norm = mymalloc(cnt->imgs.size_norm);

    /* contains the moving objects of ref. frame */
    cnt->imgs.ref_dyn = mymalloc(cnt->imgs.motionsize * sizeof(*cnt->imgs.ref_dyn));
    cnt->imgs.image_virgin.image_norm = mymalloc(cnt->imgs.size_norm);
    cnt->imgs.image_vprvcy.image_norm = mymalloc(cnt->imgs.size_norm);
    cnt->imgs.smartmask = mymalloc(cnt->imgs.motionsize);
    cnt->imgs.smartmask_final = mymalloc(cnt->imgs.motionsize);
    cnt->imgs.smartmask_buffer = mymalloc(cnt->imgs.motionsize * sizeof(*cnt->imgs.smartmask_buffer));
    cnt->imgs.labels = mymalloc(cnt->imgs.motionsize * sizeof(*cnt->imgs.labels));
    cnt->imgs.labelsize = mymalloc((cnt->imgs.motionsize/2+1) * sizeof(*cnt->imgs.labelsize));
    cnt->imgs.preview_image.image_norm = mymalloc(cnt->imgs.size_norm);
    cnt->imgs.common_buffer = mymalloc(3 * cnt->imgs.width * cnt->imgs.height);
    if (cnt->imgs.size_high > 0){
        cnt->imgs.image_virgin.image_high = mymalloc(cnt->imgs.size_high);
        cnt->imgs.preview_image.image_high = mymalloc(cnt->imgs.size_high);
    }

    mot_stream_init(cnt);

    /* Set output picture type */
    if (!strcmp(cnt->conf.picture_type, "ppm"))
        cnt->imgs.picture_type = IMAGE_TYPE_PPM;
    else if (!strcmp(cnt->conf.picture_type, "webp")) {
        #ifdef HAVE_WEBP
                cnt->imgs.picture_type = IMAGE_TYPE_WEBP;
        #else
                /* Fallback to jpeg if webp was selected in the config file, but the support for it was not compiled in */
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("webp image format is not available, failing back to jpeg"));
                cnt->imgs.picture_type = IMAGE_TYPE_JPEG;
        #endif /* HAVE_WEBP */
    }
    else
        cnt->imgs.picture_type = IMAGE_TYPE_JPEG;

    /*
     * Now is a good time to init rotation data. Since vid_start has been
     * called, we know that we have imgs.width and imgs.height. When capturing
     * from a V4L device, these are copied from the corresponding conf values
     * in vid_start. When capturing from a netcam, they get set in netcam_start,
     * which is called from vid_start.
     *
     * rotate_init will set cap_width and cap_height in cnt->rotate_data.
     */
    rotate_init(cnt); /* rotate_deinit is called in main */

    init_text_scale(cnt);   /*Initialize and validate the text_scale */

    /* Capture first image, or we will get an alarm on start */
    if (cnt->video_dev >= 0) {
        int i;

        for (i = 0; i < 5; i++) {
            if (vid_next(cnt, &cnt->imgs.image_virgin) == 0)
                break;
            SLEEP(2, 0);
        }

        if (i >= 5) {
            memset(cnt->imgs.image_virgin.image_norm, 0x80, cnt->imgs.size_norm);       /* initialize to grey */
            draw_text(cnt->imgs.image_virgin.image_norm, cnt->imgs.width, cnt->imgs.height,
                      10, 20, "Error capturing first image", cnt->text_scale);
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error capturing first image"));
        }
    }
    cnt->current_image = &cnt->imgs.image_ring[cnt->imgs.image_ring_in];

    /* create a reference frame */
    alg_update_reference_frame(cnt, RESET_REF_FRAME);

    #if defined(HAVE_V4L2) && !defined(BSD)
        /* open video loopback devices if enabled */
        if (cnt->conf.video_pipe) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Opening video loopback device for normal pictures"));

            /* vid_startpipe should get the output dimensions */
            cnt->pipe = vlp_startpipe(cnt->conf.video_pipe, cnt->imgs.width, cnt->imgs.height);

            if (cnt->pipe < 0) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Failed to open video loopback for normal pictures"));
                return -1;
            }
        }

        if (cnt->conf.video_pipe_motion) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Opening video loopback device for motion pictures"));

            /* vid_startpipe should get the output dimensions */
            cnt->mpipe = vlp_startpipe(cnt->conf.video_pipe_motion, cnt->imgs.width, cnt->imgs.height);

            if (cnt->mpipe < 0) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Failed to open video loopback for motion pictures"));
                return -1;
            }
        }
    #endif /* HAVE_V4L2 && !BSD */

    retcd = dbse_init(cnt);
    if (retcd != 0) return retcd;

    /* Load the mask file if any */
    if (cnt->conf.mask_file) {
        if ((picture = myfopen(cnt->conf.mask_file, "r"))) {
            /*
             * NOTE: The mask is expected to have the output dimensions. I.e., the mask
             * applies to the already rotated image, not the capture image. Thus, use
             * width and height from imgs.
             */
            cnt->imgs.mask = get_pgm(picture, cnt->imgs.width, cnt->imgs.height);
            myfclose(picture);
        } else {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                ,_("Error opening mask file %s")
                ,cnt->conf.mask_file);
            /*
             * Try to write an empty mask file to make it easier
             * for the user to edit it
             */
            put_fixed_mask(cnt, cnt->conf.mask_file);
        }

        if (!cnt->imgs.mask) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("Failed to read mask image. Mask feature disabled."));
        } else {
            MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
                ,_("Maskfile \"%s\" loaded.")
                ,cnt->conf.mask_file);
        }
    } else {
        cnt->imgs.mask = NULL;
    }

    init_mask_privacy(cnt);

    /* Always initialize smart_mask - someone could turn it on later... */
    memset(cnt->imgs.smartmask, 0, cnt->imgs.motionsize);
    memset(cnt->imgs.smartmask_final, 255, cnt->imgs.motionsize);
    memset(cnt->imgs.smartmask_buffer, 0, cnt->imgs.motionsize * sizeof(*cnt->imgs.smartmask_buffer));

    /* Set noise level */
    cnt->noise = cnt->conf.noise_level;

    /* Set threshold value */
    cnt->threshold = cnt->conf.threshold;
    if (cnt->conf.threshold_maximum > cnt->conf.threshold ){
        cnt->threshold_maximum = cnt->conf.threshold_maximum;
    } else {
        cnt->threshold_maximum = (cnt->imgs.height * cnt->imgs.width * 3) / 2;
    }

    if (cnt->conf.stream_preview_method == 99){
        /* This is the depreciated Stop stream process */

        /* Initialize stream server if stream port is specified to not 0 */

        if (cnt->conf.stream_port) {
            if (stream_init (&(cnt->stream), cnt->conf.stream_port, cnt->conf.stream_localhost,
                cnt->conf.webcontrol_ipv6, cnt->conf.stream_cors_header) == -1) {
                MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                    ,_("Problem enabling motion-stream server in port %d")
                    ,cnt->conf.stream_port);
                cnt->conf.stream_port = 0;
                cnt->finish = 1;
            } else {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                    ,_("Started motion-stream server on port %d (auth %s)")
                    ,cnt->conf.stream_port
                    ,cnt->conf.stream_auth_method ? _("Enabled"):_("Disabled"));
            }
        }

    } /* End of legacy stream methods*/


    /* Prevent first few frames from triggering motion... */
    cnt->moved = 8;

    /* Work out expected frame rate based on config setting */
    if (cnt->conf.framerate < 2)
        cnt->conf.framerate = 2;

    /* 2 sec startup delay so FPS is calculated correct */
    cnt->startup_frames = (cnt->conf.framerate * 2) + cnt->conf.pre_capture + cnt->conf.minimum_motion_frames;

    cnt->required_frame_time = 1000000L / cnt->conf.framerate;

    cnt->frame_delay = cnt->required_frame_time;

    /*
     * Reserve enough space for a 10 second timing history buffer. Note that,
     * if there is any problem on the allocation, mymalloc does not return.
     */
    cnt->rolling_average_data = NULL;
    cnt->rolling_average_limit = 10 * cnt->conf.framerate;
    cnt->rolling_average_data = mymalloc(sizeof(cnt->rolling_average_data) * cnt->rolling_average_limit);

    /* Preset history buffer with expected frame rate */
    for (indx = 0; indx < cnt->rolling_average_limit; indx++)
        cnt->rolling_average_data[indx] = cnt->required_frame_time;


    cnt->track_posx = 0;
    cnt->track_posy = 0;
    if (cnt->track.type)
        cnt->moved = track_center(cnt, cnt->video_dev, 0, 0, 0);

    /* Initialize area detection */
    cnt->area_minx[0] = cnt->area_minx[3] = cnt->area_minx[6] = 0;
    cnt->area_miny[0] = cnt->area_miny[1] = cnt->area_miny[2] = 0;

    cnt->area_minx[1] = cnt->area_minx[4] = cnt->area_minx[7] = cnt->imgs.width / 3;
    cnt->area_maxx[0] = cnt->area_maxx[3] = cnt->area_maxx[6] = cnt->imgs.width / 3;

    cnt->area_minx[2] = cnt->area_minx[5] = cnt->area_minx[8] = cnt->imgs.width / 3 * 2;
    cnt->area_maxx[1] = cnt->area_maxx[4] = cnt->area_maxx[7] = cnt->imgs.width / 3 * 2;

    cnt->area_miny[3] = cnt->area_miny[4] = cnt->area_miny[5] = cnt->imgs.height / 3;
    cnt->area_maxy[0] = cnt->area_maxy[1] = cnt->area_maxy[2] = cnt->imgs.height / 3;

    cnt->area_miny[6] = cnt->area_miny[7] = cnt->area_miny[8] = cnt->imgs.height / 3 * 2;
    cnt->area_maxy[3] = cnt->area_maxy[4] = cnt->area_maxy[5] = cnt->imgs.height / 3 * 2;

    cnt->area_maxx[2] = cnt->area_maxx[5] = cnt->area_maxx[8] = cnt->imgs.width;
    cnt->area_maxy[6] = cnt->area_maxy[7] = cnt->area_maxy[8] = cnt->imgs.height;

    cnt->areadetect_eventnbr = 0;

    cnt->timenow = 0;
    cnt->timebefore = 0;
    cnt->rate_limit = 0;
    cnt->lastframetime = 0;
    cnt->minimum_frame_time_downcounter = cnt->conf.minimum_frame_time;
    cnt->get_image = 1;

    cnt->olddiffs = 0;
    cnt->smartmask_ratio = 0;
    cnt->smartmask_count = 20;

    cnt->previous_diffs = 0;
    cnt->previous_location_x = 0;
    cnt->previous_location_y = 0;

    cnt->time_last_frame = 1;
    cnt->time_current_frame = 0;

    cnt->smartmask_lastrate = 0;

    cnt->passflag = 0;  //only purpose to flag first frame
    cnt->rolling_frame = 0;

    if (cnt->conf.emulate_motion) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("Emulating motion"));
    }

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
static void motion_cleanup(struct context *cnt) {

    if (cnt->conf.stream_preview_method == 99){
        /* This is the depreciated Stop stream process */
        if ((cnt->conf.stream_port) && (cnt->stream.socket != -1))
            stream_stop(&cnt->stream);
    }

    event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, NULL);
    event(cnt, EVENT_ENDMOTION, NULL, NULL, NULL, NULL);

    mot_stream_deinit(cnt);

    if (cnt->video_dev >= 0) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("Calling vid_close() from motion_cleanup"));
        vid_close(cnt);
    }

    free(cnt->imgs.img_motion.image_norm);
    cnt->imgs.img_motion.image_norm = NULL;

    free(cnt->imgs.ref);
    cnt->imgs.ref = NULL;

    free(cnt->imgs.ref_dyn);
    cnt->imgs.ref_dyn = NULL;

    free(cnt->imgs.image_virgin.image_norm);
    cnt->imgs.image_virgin.image_norm = NULL;

    free(cnt->imgs.image_vprvcy.image_norm);
    cnt->imgs.image_vprvcy.image_norm = NULL;

    free(cnt->imgs.labels);
    cnt->imgs.labels = NULL;

    free(cnt->imgs.labelsize);
    cnt->imgs.labelsize = NULL;

    free(cnt->imgs.smartmask);
    cnt->imgs.smartmask = NULL;

    free(cnt->imgs.smartmask_final);
    cnt->imgs.smartmask_final = NULL;

    free(cnt->imgs.smartmask_buffer);
    cnt->imgs.smartmask_buffer = NULL;

    if (cnt->imgs.mask) free(cnt->imgs.mask);
    cnt->imgs.mask = NULL;

    if (cnt->imgs.mask_privacy) free(cnt->imgs.mask_privacy);
    cnt->imgs.mask_privacy = NULL;

    if (cnt->imgs.mask_privacy_uv) free(cnt->imgs.mask_privacy_uv);
    cnt->imgs.mask_privacy_uv = NULL;

    if (cnt->imgs.mask_privacy_high) free(cnt->imgs.mask_privacy_high);
    cnt->imgs.mask_privacy_high = NULL;

    if (cnt->imgs.mask_privacy_high_uv) free(cnt->imgs.mask_privacy_high_uv);
    cnt->imgs.mask_privacy_high_uv = NULL;

    free(cnt->imgs.common_buffer);
    cnt->imgs.common_buffer = NULL;

    free(cnt->imgs.preview_image.image_norm);
    cnt->imgs.preview_image.image_norm = NULL;

    if (cnt->imgs.size_high > 0){
        free(cnt->imgs.image_virgin.image_high);
        cnt->imgs.image_virgin.image_high = NULL;

        free(cnt->imgs.preview_image.image_high);
        cnt->imgs.preview_image.image_high = NULL;
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

    if (cnt->rolling_average_data != NULL) free(cnt->rolling_average_data);


    /* Cleanup the current time structure */
    free(cnt->currenttime_tm);
    cnt->currenttime_tm = NULL;

    /* Cleanup the event time structure */
    free(cnt->eventtime_tm);
    cnt->eventtime_tm = NULL;

    dbse_deinit(cnt);

    if (cnt->netcam_decoder){
        free(cnt->netcam_decoder);
        cnt->netcam_decoder = NULL;
    }

}

static void mlp_mask_privacy(struct context *cnt){

    if (cnt->imgs.mask_privacy == NULL) return;

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
    indx_max = 1;
    if (cnt->imgs.size_high > 0) indx_max = 2;
    increment = sizeof(unsigned long);

    while (indx_img <= indx_max){
        if (indx_img == 1) {
            /* Normal Resolution */
            index_y = cnt->imgs.height * cnt->imgs.width;
            image = cnt->current_image->image_norm;
            mask = cnt->imgs.mask_privacy;
            index_crcb = cnt->imgs.size_norm - index_y;
            maskuv = cnt->imgs.mask_privacy_uv;
        } else {
            /* High Resolution */
            index_y = cnt->imgs.height_high * cnt->imgs.width_high;
            image = cnt->current_image->image_high;
            mask = cnt->imgs.mask_privacy_high;
            index_crcb = cnt->imgs.size_high - index_y;
            maskuv = cnt->imgs.mask_privacy_high_uv;
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
            if (*(mask++) == 0x00) *image = 0x80; // Mask last remaining bytes.
            image += 1;
        }

        indx_img++;
    }
}

static void mlp_areadetect(struct context *cnt){
    int i, j, z = 0;
    /*
     * Simple hack to recognize motion in a specific area
     * Do we need a new coversion specifier as well??
     */
    if ((cnt->conf.area_detect) &&
        (cnt->event_nr != cnt->areadetect_eventnbr) &&
        (cnt->current_image->flags & IMAGE_TRIGGER)) {
        j = strlen(cnt->conf.area_detect);
        for (i = 0; i < j; i++) {
            z = cnt->conf.area_detect[i] - 49; /* characters are stored as ascii 48-57 (0-9) */
            if ((z >= 0) && (z < 9)) {
                if (cnt->current_image->location.x > cnt->area_minx[z] &&
                    cnt->current_image->location.x < cnt->area_maxx[z] &&
                    cnt->current_image->location.y > cnt->area_miny[z] &&
                    cnt->current_image->location.y < cnt->area_maxy[z]) {
                    event(cnt, EVENT_AREA_DETECTED, NULL, NULL, NULL, &cnt->current_image->timestamp_tv);
                    cnt->areadetect_eventnbr = cnt->event_nr; /* Fire script only once per event */
                    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO
                        ,_("Motion in area %d detected."), z + 1);
                    break;
                }
            }
        }
    }

}

static void mlp_prepare(struct context *cnt){

    int frame_buffer_size;
    struct timeval tv1;

    /***** MOTION LOOP - PREPARE FOR NEW FRAME SECTION *****/
    cnt->watchdog = WATCHDOG_TMO;

    /* Get current time and preserver last time for frame interval calc. */

    /* This may be better at the end of the loop or moving the part in
     * the end doing elapsed time calc in here
     */
    cnt->timebefore = cnt->timenow;
    gettimeofday(&tv1, NULL);
    cnt->timenow = tv1.tv_usec + 1000000L * tv1.tv_sec;

    /*
     * Calculate detection rate limit. Above 5fps we limit the detection
     * rate to 3fps to reduce load at higher framerates.
     */
    cnt->process_thisframe = 0;
    cnt->rate_limit++;
    if (cnt->rate_limit >= (cnt->lastrate / 3)) {
        cnt->rate_limit = 0;
        cnt->process_thisframe = 1;
    }

    /*
     * Since we don't have sanity checks done when options are set,
     * this sanity check must go in the main loop :(, before pre_captures
     * are attempted.
     */
    if (cnt->conf.minimum_motion_frames < 1)
        cnt->conf.minimum_motion_frames = 1;

    if (cnt->conf.pre_capture < 0)
        cnt->conf.pre_capture = 0;

    /*
     * Check if our buffer is still the right size
     * If pre_capture or minimum_motion_frames has been changed
     * via the http remote control we need to re-size the ring buffer
     */
    frame_buffer_size = cnt->conf.pre_capture + cnt->conf.minimum_motion_frames;

    if (cnt->imgs.image_ring_size != frame_buffer_size)
        image_ring_resize(cnt, frame_buffer_size);

    /* Get time for current frame */
    cnt->currenttime = time(NULL);

    /*
     * localtime returns static data and is not threadsafe
     * so we use localtime_r which is reentrant and threadsafe
     */
    localtime_r(&cnt->currenttime, cnt->currenttime_tm);

    /*
     * If we have started on a new second we reset the shots variable
     * lastrate is updated to be the number of the last frame. last rate
     * is used as the ffmpeg framerate when motion is detected.
     */
    if (cnt->lastframetime != cnt->currenttime) {
        cnt->lastrate = cnt->shots + 1;
        cnt->shots = -1;
        cnt->lastframetime = cnt->currenttime;

        if (cnt->conf.minimum_frame_time) {
            cnt->minimum_frame_time_downcounter--;
            if (cnt->minimum_frame_time_downcounter == 0)
                cnt->get_image = 1;
        } else {
            cnt->get_image = 1;
        }
    }


    /* Increase the shots variable for each frame captured within this second */
    cnt->shots++;

    if (cnt->startup_frames > 0)
        cnt->startup_frames--;


}

static void mlp_resetimages(struct context *cnt){

    struct image_data *old_image;

    if (cnt->conf.minimum_frame_time) {
        cnt->minimum_frame_time_downcounter = cnt->conf.minimum_frame_time;
        cnt->get_image = 0;
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
    old_image = cnt->current_image;
    cnt->current_image = &cnt->imgs.image_ring[cnt->imgs.image_ring_in];

    /* Init/clear current_image */
    if (cnt->process_thisframe) {
        /* set diffs to 0 now, will be written after we calculated diffs in new image */
        cnt->current_image->diffs = 0;

        /* Set flags to 0 */
        cnt->current_image->flags = 0;
        cnt->current_image->cent_dist = 0;

        /* Clear location data */
        memset(&cnt->current_image->location, 0, sizeof(cnt->current_image->location));
        cnt->current_image->total_labels = 0;
    } else if (cnt->current_image && old_image) {
        /* not processing this frame: save some important values for next image */
        cnt->current_image->diffs = old_image->diffs;
        cnt->current_image->timestamp_tv = old_image->timestamp_tv;
        cnt->current_image->shot = old_image->shot;
        cnt->current_image->cent_dist = old_image->cent_dist;
        cnt->current_image->flags = old_image->flags & (~IMAGE_SAVED);
        cnt->current_image->location = old_image->location;
        cnt->current_image->total_labels = old_image->total_labels;
    }

    /* Store time with pre_captured image */
    gettimeofday(&cnt->current_image->timestamp_tv, NULL);

    /* Store shot number with pre_captured image */
    cnt->current_image->shot = cnt->shots;

}

static int mlp_retry(struct context *cnt){

    /*
     * If a camera is not available we keep on retrying every 10 seconds
     * until it shows up.
     */
    int size_high;

    if (cnt->video_dev < 0 &&
        cnt->currenttime % 10 == 0 && cnt->shots == 0) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Retrying until successful connection with camera"));
        cnt->video_dev = vid_start(cnt);

        if (cnt->video_dev < 0) {
            return 1;
        }

        if ((cnt->imgs.width % 8) || (cnt->imgs.height % 8)) {
            MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("Image width (%d) or height(%d) requested is not modulo 8.")
                ,cnt->imgs.width, cnt->imgs.height);
            return 1;
        }

        if ((cnt->imgs.width  < 64) || (cnt->imgs.height < 64)){
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("Motion only supports width and height greater than or equal to 64 %dx%d")
                ,cnt->imgs.width, cnt->imgs.height);
                return 1;
        }

        /*
         * If the netcam has different dimensions than in the config file
         * we need to restart Motion to re-allocate all the buffers
         */
        if (cnt->imgs.width != cnt->conf.width || cnt->imgs.height != cnt->conf.height) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Camera has finally become available\n"
                       "Camera image has different width and height"
                       "from what is in the config file. You should fix that\n"
                       "Restarting Motion thread to reinitialize all "
                       "image buffers to new picture dimensions"));
            cnt->conf.width = cnt->imgs.width;
            cnt->conf.height = cnt->imgs.height;
            /*
             * Break out of main loop terminating thread
             * watchdog will start us again
             */
            return 1;
        }
        /*
         * For high res, we check the size of buffer to determine whether to break out
         * the init_motion function allocated the buffer for high using the cnt->imgs.size_high
         * and the vid_start ONLY re-populates the height/width so we can check the size here.
         */
        size_high = (cnt->imgs.width_high * cnt->imgs.height_high * 3) / 2;
        if (cnt->imgs.size_high != size_high) return 1;
    }
    return 0;
}

static int mlp_capture(struct context *cnt){

    const char *tmpin;
    char tmpout[80];
    int vid_return_code = 0;        /* Return code used when calling vid_next */
    struct timeval tv1;

    /***** MOTION LOOP - IMAGE CAPTURE SECTION *****/
    /*
     * Fetch next frame from camera
     * If vid_next returns 0 all is well and we got a new picture
     * Any non zero value is an error.
     * 0 = OK, valid picture
     * <0 = fatal error - leave the thread by breaking out of the main loop
     * >0 = non fatal error - copy last image or show grey image with message
     */
    if (cnt->video_dev >= 0)
        vid_return_code = vid_next(cnt, cnt->current_image);
    else
        vid_return_code = 1; /* Non fatal error */

    // VALID PICTURE
    if (vid_return_code == 0) {
        cnt->lost_connection = 0;
        cnt->connectionlosttime = 0;

        /* If all is well reset missing_frame_counter */
        if (cnt->missing_frame_counter >= MISSING_FRAMES_TIMEOUT * cnt->conf.framerate) {
            /* If we previously logged starting a grey image, now log video re-start */
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Video signal re-acquired"));
            // event for re-acquired video signal can be called here
            event(cnt, EVENT_CAMERA_FOUND, NULL, NULL, NULL, NULL);
        }
        cnt->missing_frame_counter = 0;

        /*
         * Save the newly captured still virgin image to a buffer
         * which we will not alter with text and location graphics
         */
        memcpy(cnt->imgs.image_virgin.image_norm, cnt->current_image->image_norm, cnt->imgs.size_norm);

        mlp_mask_privacy(cnt);

        memcpy(cnt->imgs.image_vprvcy.image_norm, cnt->current_image->image_norm, cnt->imgs.size_norm);

        /*
         * If the camera is a netcam we let the camera decide the pace.
         * Otherwise we will keep on adding duplicate frames.
         * By resetting the timer the framerate becomes maximum the rate
         * of the Netcam.
         */
        if (cnt->conf.netcam_url) {
            gettimeofday(&tv1, NULL);
            cnt->timenow = tv1.tv_usec + 1000000L * tv1.tv_sec;
        }
    // FATAL ERROR - leave the thread by breaking out of the main loop
    } else if (vid_return_code < 0) {
        /* Fatal error - Close video device */
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Video device fatal error - Closing video device"));
        vid_close(cnt);
        /*
         * Use virgin image, if we are not able to open it again next loop
         * a gray image with message is applied
         * flag lost_connection
         */
        memcpy(cnt->current_image->image_norm, cnt->imgs.image_virgin.image_norm, cnt->imgs.size_norm);
        cnt->lost_connection = 1;
    /* NO FATAL ERROR -
    *        copy last image or show grey image with message
    *        flag on lost_connection if :
    *               vid_return_code == NETCAM_RESTART_ERROR
    *        cnt->video_dev < 0
    *        cnt->missing_frame_counter > (MISSING_FRAMES_TIMEOUT * cnt->conf.framerate)
    */
    } else {

        //MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "vid_return_code %d",vid_return_code);

        /*
         * Netcams that change dimensions while Motion is running will
         * require that Motion restarts to reinitialize all the many
         * buffers inside Motion. It will be a mess to try and recover any
         * other way
         */
        if (vid_return_code == NETCAM_RESTART_ERROR) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Restarting Motion thread to reinitialize all "
                "image buffers"));
            /*
             * Break out of main loop terminating thread
             * watchdog will start us again
             * Set lost_connection flag on
             */
            cnt->lost_connection = 1;
            return 1;
        }

        /*
         * First missed frame - store timestamp
         * Don't reset time when thread restarts
         */
        if (cnt->connectionlosttime == 0){
            cnt->connectionlosttime = cnt->currenttime;
        }


        /*
         * Increase missing_frame_counter
         * The first MISSING_FRAMES_TIMEOUT seconds we copy previous virgin image
         * After MISSING_FRAMES_TIMEOUT seconds we put a grey error image in the buffer
         * If we still have not yet received the initial image from a camera
         * we go straight for the grey error image.
         */
        ++cnt->missing_frame_counter;

        if (cnt->video_dev >= 0 &&
            cnt->missing_frame_counter < (MISSING_FRAMES_TIMEOUT * cnt->conf.framerate)) {
            memcpy(cnt->current_image->image_norm, cnt->imgs.image_vprvcy.image_norm, cnt->imgs.size_norm);
        } else {
            cnt->lost_connection = 1;

            if (cnt->video_dev >= 0)
                tmpin = "CONNECTION TO CAMERA LOST\\nSINCE %Y-%m-%d %T";
            else
                tmpin = "UNABLE TO OPEN VIDEO DEVICE\\nSINCE %Y-%m-%d %T";

            tv1.tv_sec=cnt->connectionlosttime;
            tv1.tv_usec = 0;
            memset(cnt->current_image->image_norm, 0x80, cnt->imgs.size_norm);
            mystrftime(cnt, tmpout, sizeof(tmpout), tmpin, &tv1, NULL, 0);
            draw_text(cnt->current_image->image_norm, cnt->imgs.width, cnt->imgs.height,
                      10, 20 * cnt->text_scale, tmpout, cnt->text_scale);

            /* Write error message only once */
            if (cnt->missing_frame_counter == MISSING_FRAMES_TIMEOUT * cnt->conf.framerate) {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                    ,_("Video signal lost - Adding grey image"));
                // Event for lost video signal can be called from here
                event(cnt, EVENT_CAMERA_LOST, NULL, NULL, NULL, &tv1);
            }

            /*
             * If we don't get a valid frame for a long time, try to close/reopen device
             * Only try this when a device is open
             */
            if ((cnt->video_dev > 0) &&
                (cnt->missing_frame_counter == (MISSING_FRAMES_TIMEOUT * 4) * cnt->conf.framerate)) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Video signal still lost - "
                    "Trying to close video device"));
                vid_close(cnt);
            }
        }
    }
    return 0;

}

static void mlp_detection(struct context *cnt){


    /***** MOTION LOOP - MOTION DETECTION SECTION *****/
    /*
     * The actual motion detection takes place in the following
     * diffs is the number of pixels detected as changed
     * Make a differences picture in image_out
     *
     * alg_diff_standard is the slower full feature motion detection algorithm
     * alg_diff first calls a fast detection algorithm which only looks at a
     * fraction of the pixels. If this detects possible motion alg_diff_standard
     * is called.
     */
    if (cnt->process_thisframe) {
        if (cnt->threshold && !cnt->pause) {
            /*
             * If we've already detected motion and we want to see if there's
             * still motion, don't bother trying the fast one first. IF there's
             * motion, the alg_diff will trigger alg_diff_standard
             * anyway
             */
            if (cnt->detecting_motion || cnt->conf.setup_mode)
                cnt->current_image->diffs = alg_diff_standard(cnt, cnt->imgs.image_vprvcy.image_norm);
            else
                cnt->current_image->diffs = alg_diff(cnt, cnt->imgs.image_vprvcy.image_norm);

            /* Lightswitch feature - has light intensity changed?
             * This can happen due to change of light conditions or due to a sudden change of the camera
             * sensitivity. If alg_lightswitch detects lightswitch we suspend motion detection the next
             * 'lightswitch_frames' frames to allow the camera to settle.
             * Don't check if we have lost connection, we detect "Lost signal" frame as lightswitch
             */
            if (cnt->conf.lightswitch_percent > 1 && !cnt->lost_connection) {
                if (alg_lightswitch(cnt, cnt->current_image->diffs)) {
                    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("Lightswitch detected"));

                    if (cnt->conf.lightswitch_frames < 1)
                        cnt->conf.lightswitch_frames = 1;
                    else if (cnt->conf.lightswitch_frames > 1000)
                        cnt->conf.lightswitch_frames = 1000;

                    if (cnt->moved < (unsigned int)cnt->conf.lightswitch_frames)
                        cnt->moved = (unsigned int)cnt->conf.lightswitch_frames;

                    cnt->current_image->diffs = 0;
                    alg_update_reference_frame(cnt, RESET_REF_FRAME);
                }
            }

            /*
             * Switchfilter feature tries to detect a change in the video signal
             * from one camera to the next. This is normally used in the Round
             * Robin feature. The algorithm is not very safe.
             * The algorithm takes a little time so we only call it when needed
             * ie. when feature is enabled and diffs>threshold.
             * We do not suspend motion detection like we did for lightswitch
             * because with Round Robin this is controlled by roundrobin_skip.
             */
            if (cnt->conf.roundrobin_switchfilter && cnt->current_image->diffs > cnt->threshold) {
                cnt->current_image->diffs = alg_switchfilter(cnt, cnt->current_image->diffs,
                                                             cnt->current_image->image_norm);

                if ((cnt->current_image->diffs <= cnt->threshold) ||
                    (cnt->current_image->diffs > cnt->threshold_maximum)) {

                    cnt->current_image->diffs = 0;
                    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("Switchfilter detected"));
                }
            }

            /*
             * Despeckle feature
             * First we run (as given by the despeckle_filter option iterations
             * of erode and dilate algorithms.
             * Finally we run the labelling feature.
             * All this is done in the alg_despeckle code.
             */
            cnt->current_image->total_labels = 0;
            cnt->imgs.largest_label = 0;
            cnt->olddiffs = 0;

            if (cnt->conf.despeckle_filter && cnt->current_image->diffs > 0) {
                cnt->olddiffs = cnt->current_image->diffs;
                cnt->current_image->diffs = alg_despeckle(cnt, cnt->olddiffs);
            } else if (cnt->imgs.labelsize_max) {
                cnt->imgs.labelsize_max = 0; /* Disable labeling if enabled */
            }

        } else if (!cnt->conf.setup_mode) {
            cnt->current_image->diffs = 0;
        }
    }

    //TODO:  This section needs investigation for purpose, cause and effect
    /* Manipulate smart_mask sensitivity (only every smartmask_ratio seconds) */
    if ((cnt->smartmask_speed && (cnt->event_nr != cnt->prev_event)) &&
        (!--cnt->smartmask_count)) {
        alg_tune_smartmask(cnt);
        cnt->smartmask_count = cnt->smartmask_ratio;
    }

    /*
     * cnt->moved is set by the tracking code when camera has been asked to move.
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

}

static void mlp_tuning(struct context *cnt){

    /***** MOTION LOOP - TUNING SECTION *****/

    /*
     * If noise tuning was selected, do it now. but only when
     * no frames have been recorded and only once per second
     */
    if ((cnt->conf.noise_tune && cnt->shots == 0) &&
         (!cnt->detecting_motion && (cnt->current_image->diffs <= cnt->threshold)))
        alg_noise_tune(cnt, cnt->imgs.image_vprvcy.image_norm);


    /*
     * If we are not noise tuning lets make sure that remote controlled
     * changes of noise_level are used.
     */
    if (cnt->process_thisframe) {
        /*
         * threshold tuning if enabled
         * if we are not threshold tuning lets make sure that remote controlled
         * changes of threshold are used.
         */
        if (cnt->conf.threshold_tune){
            alg_threshold_tune(cnt, cnt->current_image->diffs, cnt->detecting_motion);
        }

        /*
         * If motion is detected (cnt->current_image->diffs > cnt->threshold) and before we add text to the pictures
         * we find the center and size coordinates of the motion to be used for text overlays and later
         * for adding the locate rectangle
         */
        if ((cnt->current_image->diffs > cnt->threshold) &&
            (cnt->current_image->diffs < cnt->threshold_maximum)){

            alg_locate_center_size(&cnt->imgs
                , cnt->imgs.width
                , cnt->imgs.height
                , &cnt->current_image->location);
            }

        /*
         * Update reference frame.
         * micro-lighswitch: trying to auto-detect lightswitch events.
         * frontdoor illumination. Updates are rate-limited to 3 per second at
         * framerates above 5fps to save CPU resources and to keep sensitivity
         * at a constant level.
         */

        if ((cnt->current_image->diffs > cnt->threshold) &&
            (cnt->current_image->diffs < cnt->threshold_maximum) &&
            (cnt->conf.lightswitch_percent >= 1) &&
            (cnt->lightswitch_framecounter < (cnt->lastrate * 2)) && /* two seconds window only */
            /* number of changed pixels almost the same in two consecutive frames and */
            ((abs(cnt->previous_diffs - cnt->current_image->diffs)) < (cnt->previous_diffs / 15)) &&
            /* center of motion in about the same place ? */
            ((abs(cnt->current_image->location.x - cnt->previous_location_x)) <= (cnt->imgs.width / 150)) &&
            ((abs(cnt->current_image->location.y - cnt->previous_location_y)) <= (cnt->imgs.height / 150))) {
            alg_update_reference_frame(cnt, RESET_REF_FRAME);
            cnt->current_image->diffs = 0;
            cnt->lightswitch_framecounter = 0;

            MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("micro-lightswitch!"));
        } else {
            alg_update_reference_frame(cnt, UPDATE_REF_FRAME);
        }
        cnt->previous_diffs = cnt->current_image->diffs;
        cnt->previous_location_x = cnt->current_image->location.x;
        cnt->previous_location_y = cnt->current_image->location.y;
    }


}

static void mlp_overlay(struct context *cnt){

    char tmp[PATH_MAX];

    /***** MOTION LOOP - TEXT AND GRAPHICS OVERLAY SECTION *****/
    /*
     * Some overlays on top of the motion image
     * Note that these now modifies the cnt->imgs.out so this buffer
     * can no longer be used for motion detection features until next
     * picture frame is captured.
     */

    /* Smartmask overlay */
    if (cnt->smartmask_speed &&
        (cnt->conf.picture_output_motion || cnt->conf.movie_output_motion ||
         cnt->conf.setup_mode || (cnt->stream_motion.cnct_count > 0)))
        overlay_smartmask(cnt, cnt->imgs.img_motion.image_norm);

    /* Largest labels overlay */
    if (cnt->imgs.largest_label && (cnt->conf.picture_output_motion || cnt->conf.movie_output_motion ||
        cnt->conf.setup_mode || (cnt->stream_motion.cnct_count > 0)))
        overlay_largest_label(cnt, cnt->imgs.img_motion.image_norm);

    /* Fixed mask overlay */
    if (cnt->imgs.mask && (cnt->conf.picture_output_motion || cnt->conf.movie_output_motion ||
        cnt->conf.setup_mode || (cnt->stream_motion.cnct_count > 0)))
        overlay_fixed_mask(cnt, cnt->imgs.img_motion.image_norm);

    /* Add changed pixels in upper right corner of the pictures */
    if (cnt->conf.text_changes) {
        if (!cnt->pause)
            sprintf(tmp, "%d", cnt->current_image->diffs);
        else
            sprintf(tmp, "-");

        draw_text(cnt->current_image->image_norm, cnt->imgs.width, cnt->imgs.height,
                  cnt->imgs.width - 10, 10, tmp, cnt->text_scale);
    }

    /*
     * Add changed pixels to motion-images (for stream) in setup_mode
     * and always overlay smartmask (not only when motion is detected)
     */
    if (cnt->conf.setup_mode || (cnt->stream_motion.cnct_count > 0)) {
        sprintf(tmp, "D:%5d L:%3d N:%3d", cnt->current_image->diffs,
                cnt->current_image->total_labels, cnt->noise);
        draw_text(cnt->imgs.img_motion.image_norm, cnt->imgs.width, cnt->imgs.height,
                  cnt->imgs.width - 10, cnt->imgs.height - (30 * cnt->text_scale),
                  tmp, cnt->text_scale);
        sprintf(tmp, "THREAD %d SETUP", cnt->threadnr);
        draw_text(cnt->imgs.img_motion.image_norm, cnt->imgs.width, cnt->imgs.height,
                  cnt->imgs.width - 10, cnt->imgs.height - (10 * cnt->text_scale),
                  tmp, cnt->text_scale);
    }

    /* Add text in lower left corner of the pictures */
    if (cnt->conf.text_left) {
        mystrftime(cnt, tmp, sizeof(tmp), cnt->conf.text_left,
                   &cnt->current_image->timestamp_tv, NULL, 0);
        draw_text(cnt->current_image->image_norm, cnt->imgs.width, cnt->imgs.height,
                  10, cnt->imgs.height - (10 * cnt->text_scale), tmp, cnt->text_scale);
    }

    /* Add text in lower right corner of the pictures */
    if (cnt->conf.text_right) {
        mystrftime(cnt, tmp, sizeof(tmp), cnt->conf.text_right,
                   &cnt->current_image->timestamp_tv, NULL, 0);
        draw_text(cnt->current_image->image_norm, cnt->imgs.width, cnt->imgs.height,
                  cnt->imgs.width - 10, cnt->imgs.height - (10 * cnt->text_scale),
                  tmp, cnt->text_scale);
    }

}

static void mlp_actions(struct context *cnt){

    int indx;

    /***** MOTION LOOP - ACTIONS AND EVENT CONTROL SECTION *****/

    if ((cnt->current_image->diffs > cnt->threshold) &&
        (cnt->current_image->diffs < cnt->threshold_maximum)) {
        /* flag this image, it have motion */
        cnt->current_image->flags |= IMAGE_MOTION;
        cnt->lightswitch_framecounter++; /* micro lightswitch */
    } else {
        cnt->lightswitch_framecounter = 0;
    }

    /*
     * If motion has been detected we take action and start saving
     * pictures and movies etc by calling motion_detected().
     * Is emulate_motion enabled we always call motion_detected()
     * If post_capture is enabled we also take care of this in the this
     * code section.
     */
    if ((cnt->conf.emulate_motion || cnt->event_user) && (cnt->startup_frames == 0)) {
        /*  If we were previously detecting motion, started a movie, then got
         *  no motion then we reset the start movie time so that we do not
         *  get a pause in the movie.
        */
        if ( (cnt->detecting_motion == 0) && (cnt->ffmpeg_output != NULL) )
            ffmpeg_reset_movie_start_time(cnt->ffmpeg_output, &cnt->current_image->timestamp_tv);
        cnt->detecting_motion = 1;
        if (cnt->conf.post_capture > 0) {
            /* Setup the postcap counter */
            cnt->postcap = cnt->conf.post_capture;
            // MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "(Em) Init post capture %d", cnt->postcap);
        }

        cnt->current_image->flags |= (IMAGE_TRIGGER | IMAGE_SAVE);
        /* Mark all images in image_ring to be saved */
        for (indx = 0; indx < cnt->imgs.image_ring_size; indx++){
            cnt->imgs.image_ring[indx].flags |= IMAGE_SAVE;
        }

        motion_detected(cnt, cnt->video_dev, cnt->current_image);
    } else if ((cnt->current_image->flags & IMAGE_MOTION) && (cnt->startup_frames == 0)) {
        /*
         * Did we detect motion (like the cat just walked in :) )?
         * If so, ensure the motion is sustained if minimum_motion_frames
         */

        /* Count how many frames with motion there is in the last minimum_motion_frames in precap buffer */
        int frame_count = 0;
        int pos = cnt->imgs.image_ring_in;

        for (indx = 0; indx < cnt->conf.minimum_motion_frames; indx++) {
            if (cnt->imgs.image_ring[pos].flags & IMAGE_MOTION)
                frame_count++;

            if (pos == 0)
                pos = cnt->imgs.image_ring_size-1;
            else
                pos--;
        }

        if (frame_count >= cnt->conf.minimum_motion_frames) {

            cnt->current_image->flags |= (IMAGE_TRIGGER | IMAGE_SAVE);
            /*  If we were previously detecting motion, started a movie, then got
             *  no motion then we reset the start movie time so that we do not
             *  get a pause in the movie.
            */
            if ( (cnt->detecting_motion == 0) && (cnt->ffmpeg_output != NULL) )
                ffmpeg_reset_movie_start_time(cnt->ffmpeg_output, &cnt->current_image->timestamp_tv);

            cnt->detecting_motion = 1;

            /* Setup the postcap counter */
            cnt->postcap = cnt->conf.post_capture;
            //MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "Setup post capture %d", cnt->postcap);

            /* Mark all images in image_ring to be saved */
            for (indx = 0; indx < cnt->imgs.image_ring_size; indx++)
                cnt->imgs.image_ring[indx].flags |= IMAGE_SAVE;

        } else if (cnt->postcap > 0) {
           /* we have motion in this frame, but not enought frames for trigger. Check postcap */
            cnt->current_image->flags |= (IMAGE_POSTCAP | IMAGE_SAVE);
            cnt->postcap--;
            //MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "post capture %d", cnt->postcap);
        } else {
            cnt->current_image->flags |= IMAGE_PRECAP;
        }

        /* Always call motion_detected when we have a motion image */
        motion_detected(cnt, cnt->video_dev, cnt->current_image);
    } else if (cnt->postcap > 0) {
        /* No motion, doing postcap */
        cnt->current_image->flags |= (IMAGE_POSTCAP | IMAGE_SAVE);
        cnt->postcap--;
        //MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "post capture %d", cnt->postcap);
    } else {
        /* Done with postcap, so just have the image in the precap buffer */
        cnt->current_image->flags |= IMAGE_PRECAP;
        /* gapless movie feature */
        if ((cnt->conf.event_gap == 0) && (cnt->detecting_motion == 1))
            cnt->event_stop = TRUE;
        cnt->detecting_motion = 0;
    }

    /* Update last frame saved time, so we can end event after gap time */
    if (cnt->current_image->flags & IMAGE_SAVE)
        cnt->lasttime = cnt->current_image->timestamp_tv.tv_sec;


    mlp_areadetect(cnt);

    /*
     * Is the movie too long? Then make movies
     * First test for movie_max_time
     */
    if ((cnt->conf.movie_max_time && cnt->event_nr == cnt->prev_event) &&
        (cnt->currenttime - cnt->eventtime >= cnt->conf.movie_max_time))
        cnt->event_stop = TRUE;

    /*
     * Now test for quiet longer than 'gap' OR make movie as decided in
     * previous statement.
     */
    if (((cnt->currenttime - cnt->lasttime >= cnt->conf.event_gap) && cnt->conf.event_gap > 0) ||
          cnt->event_stop) {
        if (cnt->event_nr == cnt->prev_event || cnt->event_stop) {

            /* Flush image buffer */
            process_image_ring(cnt, IMAGE_BUFFER_FLUSH);

            /* Save preview_shot here at the end of event */
            if (cnt->imgs.preview_image.diffs) {
                event(cnt, EVENT_IMAGE_PREVIEW, NULL, NULL, NULL, &cnt->current_image->timestamp_tv);
                cnt->imgs.preview_image.diffs = 0;
            }

            event(cnt, EVENT_ENDMOTION, NULL, NULL, NULL, &cnt->current_image->timestamp_tv);

            /*
             * If tracking is enabled we center our camera so it does not
             * point to a place where it will miss the next action
             */
            if (cnt->track.type)
                cnt->moved = track_center(cnt, cnt->video_dev, 0, 0, 0);

            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("End of event %d"), cnt->event_nr);

            cnt->event_stop = FALSE;
            cnt->event_user = FALSE;

            /* Reset post capture */
            cnt->postcap = 0;

            /* Finally we increase the event number */
            cnt->event_nr++;
            cnt->lightswitch_framecounter = 0;

            /*
             * And we unset the text_event_string to avoid that buffered
             * images get a timestamp from previous event.
             */
            cnt->text_event_string[0] = '\0';
        }
    }

    /* Save/send to movie some images */
    process_image_ring(cnt, 2);


}

static void mlp_setupmode(struct context *cnt){
    /***** MOTION LOOP - SETUP MODE CONSOLE OUTPUT SECTION *****/

    /* If CAMERA_VERBOSE enabled output some numbers to console */
    if (cnt->conf.setup_mode) {
        char msg[1024] = "\0";
        char part[100];

        if (cnt->conf.despeckle_filter) {
            snprintf(part, 99, _("Raw changes: %5d - changes after '%s': %5d"),
                     cnt->olddiffs, cnt->conf.despeckle_filter, cnt->current_image->diffs);
            strcat(msg, part);
            if (strchr(cnt->conf.despeckle_filter, 'l')) {
                snprintf(part, 99,_(" - labels: %3d"), cnt->current_image->total_labels);
                strcat(msg, part);
            }
        } else {
            snprintf(part, 99,_("Changes: %5d"), cnt->current_image->diffs);
            strcat(msg, part);
        }

        if (cnt->conf.noise_tune) {
            snprintf(part, 99,_(" - noise level: %2d"), cnt->noise);
            strcat(msg, part);
        }

        if (cnt->conf.threshold_tune) {
            snprintf(part, 99, _(" - threshold: %d"), cnt->threshold);
            strcat(msg, part);
        }

        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "%s", msg);
    }

}

static void mlp_snapshot(struct context *cnt){
    /***** MOTION LOOP - SNAPSHOT FEATURE SECTION *****/
    /*
     * Did we get triggered to make a snapshot from control http? Then shoot a snap
     * If snapshot_interval is not zero and time since epoch MOD snapshot_interval = 0 then snap
     * We actually allow the time to run over the interval in case we have a delay
     * from slow camera.
     * Note: Negative value means SIGALRM snaps are enabled
     * httpd-control snaps are always enabled.
     */

    /* time_current_frame is used both for snapshot and timelapse features */
    cnt->time_current_frame = cnt->currenttime;

    if ((cnt->conf.snapshot_interval > 0 && cnt->shots == 0 &&
         cnt->time_current_frame % cnt->conf.snapshot_interval <= cnt->time_last_frame % cnt->conf.snapshot_interval) ||
         cnt->snapshot) {
        event(cnt, EVENT_IMAGE_SNAPSHOT, cnt->current_image, NULL, NULL, &cnt->current_image->timestamp_tv);
        cnt->snapshot = 0;
    }

}

static void mlp_timelapse(struct context *cnt){

    struct tm timestamp_tm;

    if (cnt->conf.timelapse_interval) {
        localtime_r(&cnt->current_image->timestamp_tv.tv_sec, &timestamp_tm);

        /*
         * Check to see if we should start a new timelapse file. We start one when
         * we are on the first shot, and and the seconds are zero. We must use the seconds
         * to prevent the timelapse file from getting reset multiple times during the minute.
         */
        if (timestamp_tm.tm_min == 0 &&
            (cnt->time_current_frame % 60 < cnt->time_last_frame % 60) &&
            cnt->shots == 0) {

            if (strcasecmp(cnt->conf.timelapse_mode, "manual") == 0) {
                ;/* No action */

            /* If we are daily, raise timelapseend event at midnight */
            } else if (strcasecmp(cnt->conf.timelapse_mode, "daily") == 0) {
                if (timestamp_tm.tm_hour == 0)
                    event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cnt->current_image->timestamp_tv);

            /* handle the hourly case */
            } else if (strcasecmp(cnt->conf.timelapse_mode, "hourly") == 0) {
                event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cnt->current_image->timestamp_tv);

            /* If we are weekly-sunday, raise timelapseend event at midnight on sunday */
            } else if (strcasecmp(cnt->conf.timelapse_mode, "weekly-sunday") == 0) {
                if (timestamp_tm.tm_wday == 0 &&
                    timestamp_tm.tm_hour == 0)
                    event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cnt->current_image->timestamp_tv);
            /* If we are weekly-monday, raise timelapseend event at midnight on monday */
            } else if (strcasecmp(cnt->conf.timelapse_mode, "weekly-monday") == 0) {
                if (timestamp_tm.tm_wday == 1 &&
                    timestamp_tm.tm_hour == 0)
                    event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cnt->current_image->timestamp_tv);
            /* If we are monthly, raise timelapseend event at midnight on first day of month */
            } else if (strcasecmp(cnt->conf.timelapse_mode, "monthly") == 0) {
                if (timestamp_tm.tm_mday == 1 &&
                    timestamp_tm.tm_hour == 0)
                    event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cnt->current_image->timestamp_tv);
            /* If invalid we report in syslog once and continue in manual mode */
            } else {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Invalid timelapse_mode argument '%s'"), cnt->conf.timelapse_mode);
                MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
                    ,_("%:s Defaulting to manual timelapse mode"));
                conf_cmdparse(&cnt, (char *)"ffmpeg_timelapse_mode",(char *)"manual");
            }
        }

        /*
         * If ffmpeg timelapse is enabled and time since epoch MOD ffmpeg_timelaps = 0
         * add a timelapse frame to the timelapse movie.
         */
        if (cnt->shots == 0 && cnt->time_current_frame % cnt->conf.timelapse_interval <=
            cnt->time_last_frame % cnt->conf.timelapse_interval) {
                event(cnt, EVENT_TIMELAPSE, cnt->current_image, NULL, NULL,
                    &cnt->current_image->timestamp_tv);
        }
    } else if (cnt->ffmpeg_timelapse) {
    /*
     * If timelapse movie is in progress but conf.timelapse_interval is zero then close timelapse file
     * This is an important feature that allows manual roll-over of timelapse file using the http
     * remote control via a cron job.
     */
        event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cnt->current_image->timestamp_tv);
    }

    cnt->time_last_frame = cnt->time_current_frame;


}

static void mlp_loopback(struct context *cnt){
    /*
     * Feed last image and motion image to video device pipes and the stream clients
     * In setup mode we send the special setup mode image to both stream and vloopback pipe
     * In normal mode we feed the latest image to vloopback device and we send
     * the image to the stream. We always send the first image in a second to the stream.
     * Other image are sent only when the config option stream_motion is off
     * The result is that with stream_motion on the stream stream is normally at the minimal
     * 1 frame per second but the minute motion is detected the motion_detected() function
     * sends all detected pictures to the stream except the 1st per second which is already sent.
     */
    if (cnt->conf.setup_mode) {

        event(cnt, EVENT_IMAGE, &cnt->imgs.img_motion, NULL, &cnt->pipe, &cnt->current_image->timestamp_tv);
        event(cnt, EVENT_STREAM, &cnt->imgs.img_motion, NULL, NULL, &cnt->current_image->timestamp_tv);
    } else {
        event(cnt, EVENT_IMAGE, cnt->current_image, NULL,
              &cnt->pipe, &cnt->current_image->timestamp_tv);

        if (!cnt->conf.stream_motion || cnt->shots == 1)
            event(cnt, EVENT_STREAM, cnt->current_image, NULL, NULL,
                  &cnt->current_image->timestamp_tv);
    }

    event(cnt, EVENT_IMAGEM, &cnt->imgs.img_motion, NULL, &cnt->mpipe, &cnt->current_image->timestamp_tv);

}

static void mlp_parmsupdate(struct context *cnt){
    /***** MOTION LOOP - ONCE PER SECOND PARAMETER UPDATE SECTION *****/

    /* Check for some config parameter changes but only every second */
    if (cnt->shots != 0) return;

    init_text_scale(cnt);  /* Initialize and validate text_scale */

    if (strcasecmp(cnt->conf.picture_output, "on") == 0)
        cnt->new_img = NEWIMG_ON;
    else if (strcasecmp(cnt->conf.picture_output, "first") == 0)
        cnt->new_img = NEWIMG_FIRST;
    else if (strcasecmp(cnt->conf.picture_output, "best") == 0)
        cnt->new_img = NEWIMG_BEST;
    else if (strcasecmp(cnt->conf.picture_output, "center") == 0)
        cnt->new_img = NEWIMG_CENTER;
    else
        cnt->new_img = NEWIMG_OFF;

    if (strcasecmp(cnt->conf.locate_motion_mode, "on") == 0)
        cnt->locate_motion_mode = LOCATE_ON;
    else if (strcasecmp(cnt->conf.locate_motion_mode, "preview") == 0)
        cnt->locate_motion_mode = LOCATE_PREVIEW;
    else
        cnt->locate_motion_mode = LOCATE_OFF;

    if (strcasecmp(cnt->conf.locate_motion_style, "box") == 0)
        cnt->locate_motion_style = LOCATE_BOX;
    else if (strcasecmp(cnt->conf.locate_motion_style, "redbox") == 0)
        cnt->locate_motion_style = LOCATE_REDBOX;
    else if (strcasecmp(cnt->conf.locate_motion_style, "cross") == 0)
        cnt->locate_motion_style = LOCATE_CROSS;
    else if (strcasecmp(cnt->conf.locate_motion_style, "redcross") == 0)
        cnt->locate_motion_style = LOCATE_REDCROSS;
    else
        cnt->locate_motion_style = LOCATE_BOX;

    /* Sanity check for smart_mask_speed, silly value disables smart mask */
    if (cnt->conf.smart_mask_speed < 0 || cnt->conf.smart_mask_speed > 10)
        cnt->conf.smart_mask_speed = 0;

    /* Has someone changed smart_mask_speed or framerate? */
    if (cnt->conf.smart_mask_speed != cnt->smartmask_speed ||
        cnt->smartmask_lastrate != cnt->lastrate) {
        if (cnt->conf.smart_mask_speed == 0) {
            memset(cnt->imgs.smartmask, 0, cnt->imgs.motionsize);
            memset(cnt->imgs.smartmask_final, 255, cnt->imgs.motionsize);
        }

        cnt->smartmask_lastrate = cnt->lastrate;
        cnt->smartmask_speed = cnt->conf.smart_mask_speed;
        /*
            * Decay delay - based on smart_mask_speed (framerate independent)
            * This is always 5*smartmask_speed seconds
            */
        cnt->smartmask_ratio = 5 * cnt->lastrate * (11 - cnt->smartmask_speed);
    }

    dbse_sqlmask_update(cnt);

    cnt->threshold = cnt->conf.threshold;
    if (cnt->conf.threshold_maximum > cnt->conf.threshold ){
        cnt->threshold_maximum = cnt->conf.threshold_maximum;
    } else {
        cnt->threshold_maximum = (cnt->imgs.height * cnt->imgs.width * 3) / 2;
    }

    if (!cnt->conf.noise_tune){
        cnt->noise = cnt->conf.noise_level;
    }

}

static void mlp_frametiming(struct context *cnt){

    int indx;
    struct timeval tv2;
    unsigned long int elapsedtime;  //TODO: Need to evaluate logic for needing this.
    long int delay_time_nsec;

    /***** MOTION LOOP - FRAMERATE TIMING AND SLEEPING SECTION *****/
    /*
     * Work out expected frame rate based on config setting which may
     * have changed from http-control
     */
    if (cnt->conf.framerate)
        cnt->required_frame_time = 1000000L / cnt->conf.framerate;
    else
        cnt->required_frame_time = 0;

    /* Get latest time to calculate time taken to process video data */
    gettimeofday(&tv2, NULL);
    elapsedtime = (tv2.tv_usec + 1000000L * tv2.tv_sec) - cnt->timenow;

    /*
     * Update history buffer but ignore first pass as timebefore
     * variable will be inaccurate
     */
    if (cnt->passflag)
        cnt->rolling_average_data[cnt->rolling_frame] = cnt->timenow - cnt->timebefore;
    else
        cnt->passflag = 1;

    cnt->rolling_frame++;
    if (cnt->rolling_frame >= cnt->rolling_average_limit)
        cnt->rolling_frame = 0;

    /* Calculate 10 second average and use deviation in delay calculation */
    cnt->rolling_average = 0L;

    for (indx = 0; indx < cnt->rolling_average_limit; indx++)
        cnt->rolling_average += cnt->rolling_average_data[indx];

    cnt->rolling_average /= cnt->rolling_average_limit;
    cnt->frame_delay = cnt->required_frame_time - elapsedtime - (cnt->rolling_average - cnt->required_frame_time);

    if (cnt->frame_delay > 0) {
        /* Apply delay to meet frame time */
        if (cnt->frame_delay > cnt->required_frame_time)
            cnt->frame_delay = cnt->required_frame_time;

        /* Delay time in nanoseconds for SLEEP */
        delay_time_nsec = cnt->frame_delay * 1000;

        if (delay_time_nsec > 999999999)
            delay_time_nsec = 999999999;

        /* SLEEP as defined in motion.h  A safe sleep using nanosleep */
        SLEEP(0, delay_time_nsec);
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

    if (motion_init(cnt) == 0){
        while (!cnt->finish || cnt->event_stop) {
            mlp_prepare(cnt);
            if (cnt->get_image) {
                mlp_resetimages(cnt);
                if (mlp_retry(cnt) == 1)  break;
                if (mlp_capture(cnt) == 1)  break;
                mlp_detection(cnt);
                mlp_tuning(cnt);
                mlp_overlay(cnt);
                mlp_actions(cnt);
                mlp_setupmode(cnt);
            }
            mlp_snapshot(cnt);
            mlp_timelapse(cnt);
            mlp_loopback(cnt);
            mlp_parmsupdate(cnt);
            mlp_frametiming(cnt);
        }
    }

    cnt->lost_connection = 1;
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Thread exiting"));

    motion_cleanup(cnt);

    pthread_mutex_lock(&global_lock);
        threads_running--;
    pthread_mutex_unlock(&global_lock);

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
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motion going to daemon mode"));
        exit(0);
    }

    /*
     * Create the pid file if defined, if failed exit
     * If we fail we report it. If we succeed we postpone the log entry till
     * later when we have closed stdout. Otherwise Motion hangs in the terminal waiting
     * for an enter.
     */
    if (cnt_list[0]->conf.pid_file) {
        pidf = myfopen(cnt_list[0]->conf.pid_file, "w+");

        if (pidf) {
            (void)fprintf(pidf, "%d\n", getpid());
            myfclose(pidf);
        } else {
            MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO
                ,_("Exit motion, cannot create process"
                " id file (pid file) %s"), cnt_list[0]->conf.pid_file);
            if (ptr_logfile)
                myfclose(ptr_logfile);
            exit(0);
        }
    }

    /*
     * Changing dir to root enables people to unmount a disk
     * without having to stop Motion
     */
    if (chdir("/"))
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Could not change directory"));


#if (defined(BSD) && !defined(__APPLE__))
    setpgrp(0, getpid());
#else
    setpgrp();
#endif


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
    if (pidf)
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Created process id file %s. Process ID is %d")
            ,cnt_list[0]->conf.pid_file, getpid());

    sigaction(SIGTTOU, &sig_ign_action, NULL);
    sigaction(SIGTTIN, &sig_ign_action, NULL);
    sigaction(SIGTSTP, &sig_ign_action, NULL);
}

static void cntlist_create(int argc, char *argv[]){
    /*
     * cnt_list is an array of pointers to the context structures cnt for each thread.
     * First we reserve room for a pointer to thread 0's context structure
     * and a NULL pointer which indicates that end of the array of pointers to
     * thread context structures.
     */
    cnt_list = mymalloc(sizeof(struct context *) * 2);

    /* Now we reserve room for thread 0's context structure and let cnt_list[0] point to it */
    cnt_list[0] = mymalloc(sizeof(struct context));

    /* Populate context structure with start/default values */
    context_init(cnt_list[0]);

    /* Initialize some static and global string variables */
    gethostname (cnt_list[0]->hostname, PATH_MAX);
    cnt_list[0]->hostname[PATH_MAX-1] = '\0';
    /* end of variables */

    /* cnt_list[1] pointing to zero indicates no more thread context structures - they get added later */
    cnt_list[1] = NULL;

    /*
     * Command line arguments are being pointed to from cnt_list[0] and we call conf_load which loads
     * the config options from motion.conf, thread config files and the command line.
     */
    cnt_list[0]->conf.argv = argv;
    cnt_list[0]->conf.argc = argc;
    cnt_list = conf_load(cnt_list);
}

static void motion_shutdown(void){
    int i = -1;

    motion_remove_pid();

    webu_stop(cnt_list);

    while (cnt_list[++i])
        context_destroy(cnt_list[i]);

    free(cnt_list);
    cnt_list = NULL;

    vid_mutex_destroy();
}

static void motion_camera_ids(void){
    /* Set the camera id's on the context.  They must be unique */
    int indx, indx2, invalid_ids;

    /* Set defaults */
    indx = 0;
    while (cnt_list[indx] != NULL){
        if (cnt_list[indx]->conf.camera_id > 0){
            cnt_list[indx]->camera_id = cnt_list[indx]->conf.camera_id;
        } else {
            cnt_list[indx]->camera_id = indx;
        }
        indx++;
    }

    invalid_ids = FALSE;
    indx = 0;
    while (cnt_list[indx] != NULL){
        if (cnt_list[indx]->camera_id > 32000) invalid_ids = TRUE;
        indx2 = indx + 1;
        while (cnt_list[indx2] != NULL){
            if (cnt_list[indx]->camera_id == cnt_list[indx2]->camera_id) invalid_ids = TRUE;

            indx2++;
        }
        indx++;
    }
    if (invalid_ids){
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Camara IDs are not unique or have values over 32,000.  Falling back to thread numbers"));
        indx = 0;
        while (cnt_list[indx] != NULL){
            cnt_list[indx]->camera_id = indx;
            indx++;
        }
    }
}

static void motion_ntc(void){

    #ifdef HAVE_V4L2
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("v4l2   : available"));
    #else
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("v4l2   : not available"));
    #endif

    #ifdef HAVE_BKTR
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("bktr   : available"));
    #else
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("bktr   : not available"));
    #endif

    #ifdef HAVE_WEBP
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("webp   : available"));
    #else
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("webp   : not available"));
    #endif

    #ifdef HAVE_MMAL
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("mmal   : available"));
    #else
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("mmal   : not available"));
    #endif

    #ifdef HAVE_FFMPEG
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("ffmpeg : available"));
    #else
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("ffmpeg : not available"));
    #endif

    #ifdef HAVE_MYSQL
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("mysql  : available"));
    #else
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("mysql  : not available"));
    #endif

    #ifdef HAVE_MARIADB
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("MariaDB: available"));
    #else
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("MariaDB: not available"));
    #endif

    #ifdef HAVE_SQLITE3
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("sqlite3: available"));
    #else
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("sqlite3: not available"));
    #endif

    #ifdef HAVE_PGSQL
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("pgsql  : available"));
    #else
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("pgsql  : not available"));
    #endif

    #ifdef HAVE_GETTEXT
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("nls    : available"));
    #else
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("nls    : not available"));
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

    /*
     * Create the list of context structures and load the
     * configuration.
     */
    cntlist_create(argc, argv);

    if ((cnt_list[0]->conf.log_level > ALL) ||
        (cnt_list[0]->conf.log_level == 0)) {
        cnt_list[0]->conf.log_level = LEVEL_DEFAULT;
        cnt_list[0]->log_level = cnt_list[0]->conf.log_level;
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Using default log level (%s) (%d)")
            ,get_log_level_str(cnt_list[0]->log_level)
            ,SHOW_LEVEL_VALUE(cnt_list[0]->log_level));
    } else {
        cnt_list[0]->log_level = cnt_list[0]->conf.log_level - 1; // Let's make syslog compatible
    }


    if ((cnt_list[0]->conf.log_file) && (strncmp(cnt_list[0]->conf.log_file, "syslog", 6))) {
        set_log_mode(LOGMODE_FILE);
        ptr_logfile = set_logfile(cnt_list[0]->conf.log_file);

        if (ptr_logfile) {
            set_log_mode(LOGMODE_SYSLOG);
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Logging to file (%s)"),cnt_list[0]->conf.log_file);
            set_log_mode(LOGMODE_FILE);
        } else {
            MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO
                ,_("Exit motion, cannot create log file %s")
                ,cnt_list[0]->conf.log_file);
            exit(0);
        }
    } else {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Logging to syslog"));
    }

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motion %s Started"),VERSION);

    if ((cnt_list[0]->conf.log_type == NULL) ||
        !(cnt_list[0]->log_type = get_log_type(cnt_list[0]->conf.log_type))) {
        cnt_list[0]->log_type = TYPE_DEFAULT;
        cnt_list[0]->conf.log_type = mystrcpy(cnt_list[0]->conf.log_type, "ALL");
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Using default log type (%s)"),
                   get_log_type_str(cnt_list[0]->log_type));
    }

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Using log type (%s) log level (%s)"),
               get_log_type_str(cnt_list[0]->log_type), get_log_level_str(cnt_list[0]->log_level));

    set_log_level(cnt_list[0]->log_level);
    set_log_type(cnt_list[0]->log_type);


    if (daemonize) {
        /*
         * If daemon mode is requested, and we're not going into setup mode,
         * become daemon.
         */
        if (cnt_list[0]->daemon && cnt_list[0]->conf.setup_mode == 0) {
            become_daemon();
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motion running as daemon process"));
        }
    }

    if (cnt_list[0]->conf.setup_mode)
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Motion running in setup mode."));

    conf_output_parms(cnt_list);

    motion_ntc();

    motion_camera_ids();

    initialize_chars();

    webu_start(cnt_list);

    vid_mutex_init();

}

/**
 * motion_start_thread
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
static void motion_start_thread(struct context *cnt){
    int i;
    char service[6];
    pthread_attr_t thread_attr;

    if (strcmp(cnt->conf_filename, "")){
        cnt->conf_filename[sizeof(cnt->conf_filename) - 1] = '\0';
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Camera ID: %d is from %s")
            ,cnt->camera_id, cnt->conf_filename);
    }

    if (cnt->conf.netcam_url){
        snprintf(service,6,"%s",cnt->conf.netcam_url);
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Camera ID: %d Camera Name: %s Service: %s")
            ,cnt->camera_id, cnt->conf.camera_name,service);
    } else {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Camera ID: %d Camera Name: %s Device: %s")
            ,cnt->camera_id, cnt->conf.camera_name,cnt->conf.video_device);
    }

    /*
     * Check the stream port number for conflicts.
     * First we check for conflict with the control port.
     * Second we check for that two threads does not use the same port number
     * for the stream. If a duplicate port is found the stream feature gets disabled (port = 0)
     * for this thread and a warning is written to console and syslog.
     */

    if (cnt->conf.stream_port != 0) {
        /* Compare against the control port. */
        if (cnt_list[0]->conf.webcontrol_port == cnt->conf.stream_port) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("Stream port number %d for thread %d conflicts with the control port")
                ,cnt->conf.stream_port, cnt->threadnr);
            MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
                ,_("Stream feature for thread %d is disabled.")
                ,cnt->threadnr);
            cnt->conf.stream_port = 0;
        }
        /* Compare against stream ports of other threads. */
        for (i = 1; cnt_list[i]; i++) {
            if (cnt_list[i] == cnt) continue;

            if (cnt_list[i]->conf.stream_port == cnt->conf.stream_port) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Stream port number %d for thread %d conflicts with thread %d")
                    ,cnt->conf.stream_port, cnt->threadnr, cnt_list[i]->threadnr);
                MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
                    ,_("Stream feature for thread %d is disabled.")
                    ,cnt->threadnr);
                cnt->conf.stream_port = 0;
            }
        }
    }

    /*
     * Update how many threads we have running. This is done within a
     * mutex lock to prevent multiple simultaneous updates to
     * 'threads_running'.
     */
    pthread_mutex_lock(&global_lock);
    threads_running++;
    pthread_mutex_unlock(&global_lock);

    /* Set a flag that we want this thread running */
    cnt->restart = 1;

    /* Give the thread WATCHDOG_TMO to start */
    cnt->watchdog = WATCHDOG_TMO;

    /* Flag it as running outside of the thread, otherwise if the main loop
     * checked if it is was running before the thread set it to 1, it would
     * start another thread for this device. */
    cnt->running = 1;

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&cnt->thread_id, &thread_attr, &motion_loop, cnt)) {
        /* thread create failed, undo running state */
        cnt->running = 0;
        pthread_mutex_lock(&global_lock);
        threads_running--;
        pthread_mutex_unlock(&global_lock);
    }
    pthread_attr_destroy(&thread_attr);

}

static void motion_restart(int argc, char **argv){
    /*
    * Handle the restart situation. Currently the approach is to
    * cleanup everything, and then initialize everything again
    * (including re-reading the config file(s)).
    */
    MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Restarting motion."));
    motion_shutdown();

    SLEEP(2, 0);

    motion_startup(0, argc, argv); /* 0 = skip daemon init */
    MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Motion restarted"));

    restart = 0;
}

static void motion_watchdog(int indx){

    /* Notes:
     * To test scenarios, just double lock a mutex in a spawned thread.
     * We use detached threads because pthread_join would lock the main thread
     * If we only call the first pthread_cancel when we reach the watchdog_kill
     *   it does not break us out of the mutex lock.
     * We keep sending VTAlarms so the pthread_cancel queued can be caught.
     * The calls to pthread_kill 'may' not work or cause crashes
     *   The cancel could finish and then the pthread_kill could be called
     *   on the invalid thread_id which could cause undefined results
     * Even if the cancel finishes it is not clean since memory is not cleaned.
     * The other option instead of cancel would be to exit(1) and terminate everything
     * Best to just not get into a watchdog situation...
     */

    if (!cnt_list[indx]->running) return;

    cnt_list[indx]->watchdog--;
    if (cnt_list[indx]->watchdog == 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Thread %d - Watchdog timeout. Trying to do a graceful restart")
            , cnt_list[indx]->threadnr);
        cnt_list[indx]->event_stop = TRUE; /* Trigger end of event */
        cnt_list[indx]->finish = 1;
    }

    if (cnt_list[indx]->watchdog == WATCHDOG_KILL) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Thread %d - Watchdog timeout did NOT restart, killing it!")
            , cnt_list[indx]->threadnr);
        if ((cnt_list[indx]->camera_type == CAMERA_TYPE_RTSP) &&
            (cnt_list[indx]->rtsp != NULL)){
            pthread_cancel(cnt_list[indx]->rtsp->thread_id);
        }
        if ((cnt_list[indx]->camera_type == CAMERA_TYPE_RTSP) &&
            (cnt_list[indx]->rtsp_high != NULL)){
            pthread_cancel(cnt_list[indx]->rtsp_high->thread_id);
        }
        if ((cnt_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (cnt_list[indx]->netcam != NULL)){
            pthread_cancel(cnt_list[indx]->netcam->thread_id);
        }
        pthread_cancel(cnt_list[indx]->thread_id);
    }

    if (cnt_list[indx]->watchdog < WATCHDOG_KILL) {
        if ((cnt_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (cnt_list[indx]->rtsp != NULL)){
            if (!cnt_list[indx]->rtsp->handler_finished &&
                pthread_kill(cnt_list[indx]->rtsp->thread_id, 0) == ESRCH) {
                cnt_list[indx]->rtsp->handler_finished = TRUE;
                pthread_mutex_lock(&global_lock);
                    threads_running--;
                pthread_mutex_unlock(&global_lock);
                netcam_rtsp_cleanup(cnt_list[indx],FALSE);
            } else {
                pthread_kill(cnt_list[indx]->rtsp->thread_id, SIGVTALRM);
            }
        }
        if ((cnt_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (cnt_list[indx]->rtsp_high != NULL)){
            if (!cnt_list[indx]->rtsp_high->handler_finished &&
                pthread_kill(cnt_list[indx]->rtsp_high->thread_id, 0) == ESRCH) {
                cnt_list[indx]->rtsp_high->handler_finished = TRUE;
                pthread_mutex_lock(&global_lock);
                    threads_running--;
                pthread_mutex_unlock(&global_lock);
                netcam_rtsp_cleanup(cnt_list[indx],FALSE);
            } else {
                pthread_kill(cnt_list[indx]->rtsp_high->thread_id, SIGVTALRM);
            }
        }
        if ((cnt_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (cnt_list[indx]->netcam != NULL)){
            if (!cnt_list[indx]->netcam->handler_finished &&
                pthread_kill(cnt_list[indx]->netcam->thread_id, 0) == ESRCH) {
                pthread_mutex_lock(&global_lock);
                    threads_running--;
                pthread_mutex_unlock(&global_lock);
                cnt_list[indx]->netcam->handler_finished = TRUE;
                cnt_list[indx]->netcam->finish = FALSE;
            } else {
                pthread_kill(cnt_list[indx]->netcam->thread_id, SIGVTALRM);
            }
        }
        if (cnt_list[indx]->running &&
            pthread_kill(cnt_list[indx]->thread_id, 0) == ESRCH){
            MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO
                ,_("Thread %d - Cleaning thread.")
                , cnt_list[indx]->threadnr);
            pthread_mutex_lock(&global_lock);
                threads_running--;
            pthread_mutex_unlock(&global_lock);
            motion_cleanup(cnt_list[indx]);
            cnt_list[indx]->running = 0;
            cnt_list[indx]->finish = 0;
        } else {
            pthread_kill(cnt_list[indx]->thread_id,SIGVTALRM);
        }
    }
}

static int motion_check_threadcount(void){
    /* Return 1 if we should break out of loop */

    /* It has been observed that this is not counting every
     * thread running.  The netcams spawn handler threads which are not
     * counted here.  This is only counting context threads and when they
     * all get to zero, then we are done.
     */

    int motion_threads_running, indx;

    motion_threads_running = 0;

    for (indx = (cnt_list[1] != NULL ? 1 : 0); cnt_list[indx]; indx++) {
        if (cnt_list[indx]->running || cnt_list[indx]->restart)
            motion_threads_running++;
    }

    /* If the web control/streams are in finish/shutdown, we
     * do not want to count them.  They will be completely closed
     * by the process outside of loop that is checking the counts
     * of threads.  If the webcontrol is not in a finish / shutdown
     * then we want to keep them in the tread count to allow user
     * to restart the cameras and keep Motion running.
     */
    indx = 0;
    while (cnt_list[indx] != NULL){
        if ((cnt_list[indx]->webcontrol_finish == FALSE) &&
            ((cnt_list[indx]->webcontrol_daemon != NULL) ||
             (cnt_list[indx]->webstream_daemon != NULL))) {
            motion_threads_running++;
        }
        indx++;
    }


    if (((motion_threads_running == 0) && finish) ||
        ((motion_threads_running == 0) && (threads_running == 0))) {
        MOTION_LOG(ALL, TYPE_ALL, NO_ERRNO
            ,_("DEBUG-1 threads_running %d motion_threads_running %d , finish %d")
            ,threads_running, motion_threads_running, finish);
        return 1;
    } else {
        return 0;
    }
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

    /* Create the TLS key for thread number. */
    pthread_key_create(&tls_key_threadnr, NULL);
    pthread_setspecific(tls_key_threadnr, (void *)(0));

    setup_signals();

    motion_startup(1, argc, argv);

    ffmpeg_global_init();

    dbse_global_init();

    translate_init();

    do {
        if (restart) motion_restart(argc, argv);

        for (i = cnt_list[1] != NULL ? 1 : 0; cnt_list[i]; i++) {
            cnt_list[i]->threadnr = i ? i : 1;
            motion_start_thread(cnt_list[i]);
        }

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Waiting for threads to finish, pid: %d"), getpid());

        while (1) {
            SLEEP(1, 0);
            if (motion_check_threadcount()) break;

            for (i = (cnt_list[1] != NULL ? 1 : 0); cnt_list[i]; i++) {
                /* Check if threads wants to be restarted */
                if ((!cnt_list[i]->running) && (cnt_list[i]->restart)) {
                    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                        ,_("Motion thread %d restart"), cnt_list[i]->threadnr);
                    motion_start_thread(cnt_list[i]);
                }
                motion_watchdog(i);
            }
        }

        /* Reset end main loop flag */
        finish = 0;

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Threads finished"));

        /* Rest for a while if we're supposed to restart. */
        if (restart) SLEEP(1, 0);

    } while (restart); /* loop if we're supposed to restart */


    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motion terminating"));

    ffmpeg_global_deinit();

    dbse_global_deinit();

    motion_shutdown();

    /* Perform final cleanup. */
    pthread_key_delete(tls_key_threadnr);
    pthread_mutex_destroy(&global_lock);

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
    void *dummy = calloc(nbytes, 1);

    if (!dummy) {
        MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO, _("Could not allocate %llu bytes of memory!")
            ,(unsigned long long)nbytes);
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
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Warning! Function %s tries to resize memoryblock at %p to 0 bytes!")
            ,desc, ptr);
    } else {
        dummy = realloc(ptr, size);
        if (!dummy) {
            MOTION_LOG(EMG, TYPE_ALL, NO_ERRNO
                ,_("Could not resize memory-block at offset %p to %llu bytes (function %s)!")
                ,ptr, (unsigned long long)size, desc);
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
        char *buffer = mystrdup(path);
        buffer[start-path] = 0x00;

        if (mkdir(buffer, mode) == -1 && errno != EEXIST) {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                ,_("Problem creating directory %s"), buffer);
            free(buffer);
            return -1;
        }

        start = strchr(start + 1, '/');

        if (!start)
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("creating directory %s"), buffer);

        free(buffer);
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
    if (dummy) return dummy;

    /* could not open file... */
    /* path did not exist? */
    if (errno == ENOENT) {

        /* create path for file... */
        if (create_path(path) == -1)
            return NULL;

        /* and retry opening the file */
        dummy = fopen(path, mode);
    }
    if (!dummy) {
        /*
         * Two possibilities
         * 1: there was an other error while trying to open the file for the
         * first time
         * 2: could still not open the file after the path was created
         */
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
            ,_("Error opening file %s with mode %s"), path, mode);
        return NULL;
    }

    return dummy;
}

/**
 * myfclose
 *
 *  Motion-specific variant of fclose()
 *
 * Returns: fclose() return value
 */
int myfclose(FILE* fh)
{
    int rval = fclose(fh);

    if (rval != 0)
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Error closing file"));

    return rval;
}

/**
 * mystrftime_long
 *
 *   Motion-specific long form of format specifiers.
 *
 * Parameters:
 *
 *   cnt        - current thread's context structure.
 *   width      - width associated with the format specifier.
 *   word       - beginning of the format specifier's word.
 *   l          - length of the format specifier's word.
 *   out        - output buffer where to store the result. Size: PATH_MAX.
 *
 * This is called if a format specifier with the format below was found:
 *
 *   % { word }
 *
 * As a special edge case, an incomplete format at the end of the string
 * is processed as well:
 *
 *   % { word \0
 *
 * Any valid format specified width is supported, e.g. "%12{host}".
 *
 * The following specifier keywords are currently supported:
 *
 * host    Replaced with the name of the local machine (see gethostname(2)).
 * fps     Equivalent to %fps.
 */
static void mystrftime_long (const struct context *cnt,
                             int width, const char *word, int l, char *out)
{
#define SPECIFIERWORD(k) ((strlen(k)==l) && (!strncmp (k, word, l)))

    if (SPECIFIERWORD("host")) {
        snprintf (out, PATH_MAX, "%*s", width, cnt->hostname);
        return;
    }
    if (SPECIFIERWORD("fps")) {
        sprintf(out, "%*d", width, cnt->movie_fps);
        return;
    }
    if (SPECIFIERWORD("dbeventid")) {
        sprintf(out, "%*llu", width, cnt->database_event_id);
        return;
    }
    if (SPECIFIERWORD("ver")) {
        sprintf(out, "%*s", width, VERSION);
        return;
    }

    // Not a valid modifier keyword. Log the error and ignore.
    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
        _("invalid format specifier keyword %*.*s"), l, l, word);

    // Do not let the output buffer empty, or else where to restart the
    // interpretation of the user string will become dependent to far too
    // many conditions. Maybe change loop to "if (*pos_userformat == '%') {
    // ...} __else__ ..."?
    out[0] = '~'; out[1] = 0;
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
size_t mystrftime(const struct context *cnt, char *s, size_t max, const char *userformat,
                  const struct timeval *tv1, const char *filename, int sqltype)
{
    char formatstring[PATH_MAX] = "";
    char tempstring[PATH_MAX] = "";
    char *format, *tempstr;
    const char *pos_userformat;
    int width;
    struct tm timestamp_tm;

    localtime_r(&tv1->tv_sec, &timestamp_tm);

    format = formatstring;

    /* if mystrftime is called with userformat = NULL we return a zero length string */
    if (userformat == NULL) {
        *s = '\0';
        return 0;
    }

    for (pos_userformat = userformat; *pos_userformat; ++pos_userformat) {

        if (*pos_userformat == '%') {
            /*
             * Reset 'tempstr' to point to the beginning of 'tempstring',
             * otherwise we will eat up tempstring if there are many
             * format specifiers.
             */
            tempstr = tempstring;
            tempstr[0] = '\0';
            width = 0;
            while ('0' <= pos_userformat[1] && pos_userformat[1] <= '9') {
                width *= 10;
                width += pos_userformat[1] - '0';
                ++pos_userformat;
            }

            switch (*++pos_userformat) {
            case '\0': // end of string
                --pos_userformat;
                break;

            case 'v': // event
                sprintf(tempstr, "%0*d", width ? width : 2, cnt->event_nr);
                break;

            case 'q': // shots
                sprintf(tempstr, "%0*d", width ? width : 2,
                    cnt->current_image->shot);
                break;

            case 'D': // diffs
                sprintf(tempstr, "%*d", width, cnt->current_image->diffs);
                break;

            case 'N': // noise
                sprintf(tempstr, "%*d", width, cnt->noise);
                break;

            case 'i': // motion width
                sprintf(tempstr, "%*d", width,
                    cnt->current_image->location.width);
                break;

            case 'J': // motion height
                sprintf(tempstr, "%*d", width,
                    cnt->current_image->location.height);
                break;

            case 'K': // motion center x
                sprintf(tempstr, "%*d", width, cnt->current_image->location.x);
                break;

            case 'L': // motion center y
                sprintf(tempstr, "%*d", width, cnt->current_image->location.y);
                break;

            case 'o': // threshold
                sprintf(tempstr, "%*d", width, cnt->threshold);
                break;

            case 'Q': // number of labels
                sprintf(tempstr, "%*d", width,
                    cnt->current_image->total_labels);
                break;

            case 't': // camera id
                sprintf(tempstr, "%*d", width, cnt->camera_id);
                break;

            case 'C': // text_event
                if (cnt->text_event_string[0])
                    snprintf(tempstr, PATH_MAX, "%*s", width,
                        cnt->text_event_string);
                else
                    ++pos_userformat;
                break;

            case 'w': // picture width
                sprintf(tempstr, "%*d", width, cnt->imgs.width);
                break;

            case 'h': // picture height
                sprintf(tempstr, "%*d", width, cnt->imgs.height);
                break;

            case 'f': // filename -- or %fps
                if ((*(pos_userformat+1) == 'p') && (*(pos_userformat+2) == 's')) {
                    sprintf(tempstr, "%*d", width, cnt->movie_fps);
                    pos_userformat += 2;
                    break;
                }

                if (filename)
                    snprintf(tempstr, PATH_MAX, "%*s", width, filename);
                else
                    ++pos_userformat;
                break;

            case 'n': // sqltype
                if (sqltype)
                    sprintf(tempstr, "%*d", width, sqltype);
                else
                    ++pos_userformat;
                break;

            case '{': // long format specifier word.
                {
                    const char *word = ++pos_userformat;
                    while ((*pos_userformat != '}') && (*pos_userformat != 0))
                        ++pos_userformat;
                    mystrftime_long (cnt, width, word, (int)(pos_userformat-word), tempstr);
                    if (*pos_userformat == '\0') --pos_userformat;
                }
                break;

            case '$': // thread name
                if (cnt->conf.camera_name && cnt->conf.camera_name[0])
                    snprintf(tempstr, PATH_MAX, "%s", cnt->conf.camera_name);
                else
                    ++pos_userformat;
                break;

            default: // Any other code is copied with the %-sign
                *format++ = '%';
                *format++ = *pos_userformat;
                continue;
            }

            /*
             * If a format specifier was found and used, copy the result from
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

    return strftime(s, max, format, &timestamp_tm);
}
/* This is a temporary location for these util functions.  All the generic utility
 * functions will be collected here and ultimately moved into a new common "util" module
 */
void util_threadname_set(const char *abbr, int threadnbr, const char *threadname){
    /* When the abbreviation is sent in as null, that means we are being
     * provided a fully filled out thread name (usually obtained from a
     * previously called get_threadname so we set it without additional
     *  formatting.
     */

    char tname[16];
    if (abbr != NULL){
        snprintf(tname, sizeof(tname), "%s%d%s%s",abbr,threadnbr,
             threadname ? ":" : "",
             threadname ? threadname : "");
    } else {
        snprintf(tname, sizeof(tname), "%s",threadname);
    }

#ifdef __APPLE__
    pthread_setname_np(tname);
#elif defined(BSD)
    pthread_set_name_np(pthread_self(), tname);
#elif HAVE_PTHREAD_SETNAME_NP
    pthread_setname_np(pthread_self(), tname);
#else
    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Unable to set thread name %s"), tname);
#endif

}

void util_threadname_get(char *threadname){

#if ((!defined(BSD) && HAVE_PTHREAD_GETNAME_NP) || defined(__APPLE__))
    char currname[16];
    pthread_getname_np(pthread_self(), currname, sizeof(currname));
    snprintf(threadname, sizeof(currname), "%s",currname);
#else
    snprintf(threadname, 8, "%s","Unknown");
#endif

}
int util_check_passthrough(struct context *cnt){
#if (HAVE_FFMPEG && LIBAVFORMAT_VERSION_MAJOR < 55)
    if (cnt->movie_passthrough)
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("FFMPEG version too old. Disabling pass-through processing."));
    return 0;
#else
    if (cnt->movie_passthrough){
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("pass-through is enabled but is still experimental."));
        return 1;
    } else {
        return 0;
    }
#endif

}
