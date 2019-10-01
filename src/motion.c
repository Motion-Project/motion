/*    motion.c
 *
 *    Detect changes in a video stream.
 *    Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */
#include "motion.h"
#include "movie.h"
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
#include "dbse.h"
#include "draw.h"
#include "webu_stream.h"

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
 * cam_list
 *
 *   List of context structures, one for each main Motion thread.
 */
struct ctx_cam **cam_list = NULL;

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


void translate_locale_chg(const char *langcd){
    #ifdef HAVE_GETTEXT
        /* This routine is for development testing only.  It is not used for
        * regular users because once this locale is change, it changes the
        * whole computer over to the new locale.  Therefore, we just return
        */
        return;

        setenv ("LANGUAGE", langcd, 1);
        /* Invoke external function to change locale*/
        ++_nl_msg_cat_cntr;
    #else
        if (langcd != NULL) MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,"No native language support");
    #endif
}

void translate_init(void){
    #ifdef HAVE_GETTEXT
        /* Set the flag to enable native language support */
        nls_enabled = 1;

        setlocale (LC_ALL, "");

        //translate_locale_chg("li");
        translate_locale_chg("es");

        bindtextdomain ("motion", LOCALEDIR);
        bind_textdomain_codeset ("motion", "UTF-8");
        textdomain ("motion");

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Language: English"));

    #else
        /* Disable native language support */
        nls_enabled = 0;

        /* This avoids a unused function warning */
        translate_locale_chg("en");
    #endif
}

char* translate_text(const char *msgid){
    #ifdef HAVE_GETTEXT
        if (nls_enabled){
            return (char*)gettext(msgid);
        } else {
            return (char*)msgid;
        }
    #else
        return (char*)msgid;
    #endif
}

/**
 * image_ring_resize
 *
 * This routine is called from motion_loop to resize the image precapture ringbuffer
 * NOTE: This function clears all images in the old ring buffer

 * Parameters:
 *
 *      cam      Pointer to the motion ctx_cam structure
 *      new_size The new size of the ring buffer
 *
 * Returns:     nothing
 */
static void image_ring_resize(struct ctx_cam *cam, int new_size)
{
    /*
     * Only resize if :
     * Not in an event and
     * decreasing at last position in new buffer
     * increasing at last position in old buffer
     * e.g. at end of smallest buffer
     */
    if (cam->event_nr != cam->prev_event) {
        int smallest;

        if (new_size < cam->imgs.ring_size)  /* Decreasing */
            smallest = new_size;
        else  /* Increasing */
            smallest = cam->imgs.ring_size;

        if (cam->imgs.ring_in == smallest - 1 || smallest == 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Resizing pre_capture buffer to %d items"), new_size);

            /* Create memory for new ring buffer */
            struct ctx_image_data *tmp;
            tmp = mymalloc(new_size * sizeof(struct ctx_image_data));

            /*
             * Copy all information from old to new
             * Smallest is 0 at initial init
             */
            if (smallest > 0)
                memcpy(tmp, cam->imgs.image_ring, sizeof(struct ctx_image_data) * smallest);


            /* In the new buffers, allocate image memory */
            {
                int i;
                for(i = smallest; i < new_size; i++) {
                    tmp[i].image_norm = mymalloc(cam->imgs.size_norm);
                    memset(tmp[i].image_norm, 0x80, cam->imgs.size_norm);  /* initialize to grey */
                    if (cam->imgs.size_high > 0){
                        tmp[i].image_high = mymalloc(cam->imgs.size_high);
                        memset(tmp[i].image_high, 0x80, cam->imgs.size_high);
                    }
                }
            }

            /* Free the old ring */
            free(cam->imgs.image_ring);

            /* Point to the new ring */
            cam->imgs.image_ring = tmp;
            cam->current_image = NULL;

            cam->imgs.ring_size = new_size;

            cam->imgs.ring_in = 0;
            cam->imgs.ring_out = 0;
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
 *      cam      Pointer to the motion context structure
 *
 * Returns:     nothing
 */
static void image_ring_destroy(struct ctx_cam *cam)
{
    int i;

    /* Exit if don't have any ring */
    if (cam->imgs.image_ring == NULL)
        return;

    /* Free all image buffers */
    for (i = 0; i < cam->imgs.ring_size; i++){
        free(cam->imgs.image_ring[i].image_norm);
        if (cam->imgs.size_high >0 ) free(cam->imgs.image_ring[i].image_high);
    }

    /* Free the ring */
    free(cam->imgs.image_ring);

    cam->imgs.image_ring = NULL;
    cam->current_image = NULL;
    cam->imgs.ring_size = 0;
}

/**
 * image_save_as_preview
 *
 * This routine is called when we detect motion and want to save an image in the preview buffer
 *
 * Parameters:
 *
 *      cam      Pointer to the motion context structure
 *      img      Pointer to the ctx_image_data we want to set as preview image
 *
 * Returns:     nothing
 */
static void image_save_as_preview(struct ctx_cam *cam, struct ctx_image_data *img)
{
    void *image_norm, *image_high;

    /* Save our pointers to our memory locations for images*/
    image_norm = cam->imgs.image_preview.image_norm;
    image_high = cam->imgs.image_preview.image_high;

    /* Copy over the meta data from the img into preview */
    memcpy(&cam->imgs.image_preview, img, sizeof(struct ctx_image_data));

    /* Restore the pointers to the memory locations for images*/
    cam->imgs.image_preview.image_norm = image_norm;
    cam->imgs.image_preview.image_high = image_high;

    /* Copy the actual images for norm and high */
    memcpy(cam->imgs.image_preview.image_norm, img->image_norm, cam->imgs.size_norm);
    if (cam->imgs.size_high > 0){
        memcpy(cam->imgs.image_preview.image_high, img->image_high, cam->imgs.size_high);
    }

    /*
     * If we set output_all to yes and during the event
     * there is no image with motion, diffs is 0, we are not going to save the preview event
     */
    if (cam->imgs.image_preview.diffs == 0)
        cam->imgs.image_preview.diffs = 1;

    /* draw locate box here when mode = LOCATE_PREVIEW */
    if (cam->locate_motion_mode == LOCATE_PREVIEW) {

        if (cam->locate_motion_style == LOCATE_BOX) {
            alg_draw_location(&img->location, &cam->imgs, cam->imgs.width, cam->imgs.image_preview.image_norm,
                              LOCATE_BOX, LOCATE_NORMAL, cam->process_thisframe);
        } else if (cam->locate_motion_style == LOCATE_REDBOX) {
            alg_draw_red_location(&img->location, &cam->imgs, cam->imgs.width, cam->imgs.image_preview.image_norm,
                                  LOCATE_REDBOX, LOCATE_NORMAL, cam->process_thisframe);
        } else if (cam->locate_motion_style == LOCATE_CROSS) {
            alg_draw_location(&img->location, &cam->imgs, cam->imgs.width, cam->imgs.image_preview.image_norm,
                              LOCATE_CROSS, LOCATE_NORMAL, cam->process_thisframe);
        } else if (cam->locate_motion_style == LOCATE_REDCROSS) {
            alg_draw_red_location(&img->location, &cam->imgs, cam->imgs.width, cam->imgs.image_preview.image_norm,
                                  LOCATE_REDCROSS, LOCATE_NORMAL, cam->process_thisframe);
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
 *   cam - the context struct to destroy
 *
 * Returns: nothing
 */
static void context_init(struct ctx_cam *cam)
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

    memset(cam, 0, sizeof(struct ctx_cam));
    cam->noise = 255;
    cam->lastrate = 25;

    memcpy(&cam->track, &track_template, sizeof(struct trackoptions));

    cam->pipe = -1;
    cam->mpipe = -1;

    cam->vdev = NULL;    /*Init to NULL to check loading parms vs web updates*/
    cam->netcam = NULL;
    cam->netcam = NULL;
    cam->netcam_high = NULL;

}

/**
 * context_destroy
 *
 *   Destroys a context struct by freeing allocated memory, calling the
 *   appropriate cleanup functions and finally freeing the struct itself.
 *
 * Parameters:
 *
 *   cam - the context struct to destroy
 *
 * Returns: nothing
 */
static void context_destroy(struct ctx_cam *cam)
{
    unsigned int j;

    /* Free memory allocated for config parameters */
    for (j = 0; config_params[j].param_name != NULL; j++) {
        if (config_params[j].copy == copy_string ||
            config_params[j].copy == copy_uri ||
            config_params[j].copy == read_camera_dir) {
            void **val;
            val = (void *)((char *)cam+(int)config_params[j].conf_value);
            if (*val) {
                free(*val);
                *val = NULL;
            }
        }
    }

    free(cam);
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
        if (cam_list) {
            i = -1;
            while (cam_list[++i]) {
                if (cam_list[i]->conf.snapshot_interval)
                    cam_list[i]->snapshot = 1;

            }
        }
        break;
    case SIGUSR1:
        /* Trigger the end of a event */
        if (cam_list) {
            i = -1;
            while (cam_list[++i]){
                cam_list[i]->event_stop = TRUE;
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

        if (cam_list) {
            i = -1;
            while (cam_list[++i]) {
                cam_list[i]->webcontrol_finish = TRUE;
                cam_list[i]->event_stop = TRUE;
                cam_list[i]->finish = 1;
                /*
                 * Don't restart thread when it ends,
                 * all threads restarts if global restart is set
                 */
                 cam_list[i]->restart = 0;
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
static void sigchild_handler(int signo)
{
    (void)signo;
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
    if ((cam_list[0]->daemon) && (cam_list[0]->conf.pid_file) && (restart == 0)) {
        if (!unlink(cam_list[0]->conf.pid_file))
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Removed process id file (pid file)."));
        else
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Error removing pid file"));
    }

    if (ptr_logfile) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Closing logfile (%s)."),
                   cam_list[0]->conf.log_file);
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
 *   cam      - current thread's context struct
 *   dev      - video device file descriptor
 *   img      - pointer to the captured ctx_image_data with detected motion
 */
static void motion_detected(struct ctx_cam *cam, int dev, struct ctx_image_data *img)
{
    struct config *conf = &cam->conf;
    struct ctx_images *imgs = &cam->imgs;
    struct ctx_coord *location = &img->location;

    /* Draw location */
    if (cam->locate_motion_mode == LOCATE_ON) {

        if (cam->locate_motion_style == LOCATE_BOX) {
            alg_draw_location(location, imgs, imgs->width, img->image_norm, LOCATE_BOX,
                              LOCATE_BOTH, cam->process_thisframe);
        } else if (cam->locate_motion_style == LOCATE_REDBOX) {
            alg_draw_red_location(location, imgs, imgs->width, img->image_norm, LOCATE_REDBOX,
                                  LOCATE_BOTH, cam->process_thisframe);
        } else if (cam->locate_motion_style == LOCATE_CROSS) {
            alg_draw_location(location, imgs, imgs->width, img->image_norm, LOCATE_CROSS,
                              LOCATE_BOTH, cam->process_thisframe);
        } else if (cam->locate_motion_style == LOCATE_REDCROSS) {
            alg_draw_red_location(location, imgs, imgs->width, img->image_norm, LOCATE_REDCROSS,
                                  LOCATE_BOTH, cam->process_thisframe);
        }
    }

    /* Calculate how centric motion is if configured preview center*/
    if (cam->new_img & NEWIMG_CENTER) {
        unsigned int distX = abs((imgs->width / 2) - location->x);
        unsigned int distY = abs((imgs->height / 2) - location->y);

        img->cent_dist = distX * distX + distY * distY;
    }


    /* Do things only if we have got minimum_motion_frames */
    if (img->flags & IMAGE_TRIGGER) {
        if (cam->event_nr != cam->prev_event) {

            cam->prev_event = cam->event_nr;
            cam->eventtime = img->imgts.tv_sec;

            mystrftime(cam, cam->text_event_string, sizeof(cam->text_event_string),
                       cam->conf.text_event, &img->imgts, NULL, 0);

            event(cam, EVENT_FIRSTMOTION, img, NULL, NULL,
                &cam->imgs.image_ring[cam->imgs.ring_out].imgts);

            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motion detected - starting event %d"),
                       cam->event_nr);

            if (cam->new_img & (NEWIMG_FIRST | NEWIMG_BEST | NEWIMG_CENTER))
                image_save_as_preview(cam, img);

        }

        event(cam, EVENT_MOTION, NULL, NULL, NULL, &img->imgts);
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
            event(cam, EVENT_STREAM, img, NULL, NULL, &img->imgts);

        /*
         * Save motion jpeg, if configured
         * Output the image_out (motion) picture.
         */
        if (conf->picture_output_motion)
            event(cam, EVENT_IMAGEM_DETECTED, NULL, NULL, NULL, &img->imgts);
    }

    /* if track enabled and auto track on */
    if (cam->track.type && cam->track.active)
        cam->moved = track_move(cam, dev, location, imgs, 0);

}

/**
 * process_image_ring
 *
 *   Called from 'motion_loop' to save images / send images to movie
 *
 * Parameters:
 *
 *   cam        - current thread's context struct
 *   max_images - Max number of images to process
 *                Set to IMAGE_BUFFER_FLUSH to send/save all images in buffer
 */

static void process_image_ring(struct ctx_cam *cam, unsigned int max_images)
{
    /*
     * We are going to send an event, in the events there is still
     * some code that use cam->current_image
     * so set it temporary to our image
     */
    struct ctx_image_data *saved_current_image = cam->current_image;

    /* If image is flaged to be saved and not saved yet, process it */
    do {
        /* Check if we should save/send this image, breakout if not */
        assert(cam->imgs.ring_out < cam->imgs.ring_size);
        if ((cam->imgs.image_ring[cam->imgs.ring_out].flags & (IMAGE_SAVE | IMAGE_SAVED)) != IMAGE_SAVE)
            break;

        /* Set inte global context that we are working with this image */
        cam->current_image = &cam->imgs.image_ring[cam->imgs.ring_out];

        if (cam->imgs.image_ring[cam->imgs.ring_out].shot < cam->conf.framerate) {
            if (cam->log_level >= DBG) {
                char tmp[32];
                const char *t;

                if (cam->imgs.image_ring[cam->imgs.ring_out].flags & IMAGE_TRIGGER)
                    t = "Trigger";
                else if (cam->imgs.image_ring[cam->imgs.ring_out].flags & IMAGE_MOTION)
                    t = "Motion";
                else if (cam->imgs.image_ring[cam->imgs.ring_out].flags & IMAGE_PRECAP)
                    t = "Precap";
                else if (cam->imgs.image_ring[cam->imgs.ring_out].flags & IMAGE_POSTCAP)
                    t = "Postcap";
                else
                    t = "Other";

                mystrftime(cam, tmp, sizeof(tmp), "%H%M%S-%q",
                           &cam->imgs.image_ring[cam->imgs.ring_out].imgts, NULL, 0);
                draw_text(cam->imgs.image_ring[cam->imgs.ring_out].image_norm,
                          cam->imgs.width, cam->imgs.height, 10, 20, tmp, cam->text_scale);
                draw_text(cam->imgs.image_ring[cam->imgs.ring_out].image_norm,
                          cam->imgs.width, cam->imgs.height, 10, 30, t, cam->text_scale);
            }

            /* Output the picture to jpegs and ffmpeg */
            event(cam, EVENT_IMAGE_DETECTED,
              &cam->imgs.image_ring[cam->imgs.ring_out], NULL, NULL,
              &cam->imgs.image_ring[cam->imgs.ring_out].imgts);


            /*
             * Check if we must add any "filler" frames into movie to keep up fps
             * Only if we are recording videos ( ffmpeg or extenal pipe )
             * While the overall elapsed time might be correct, if there are
             * many duplicated frames, say 10 fps, 5 duplicated, the video will
             * look like it is frozen every second for half a second.
             */
            if (!cam->conf.movie_duplicate_frames) {
                /* don't duplicate frames */
            } else if ((cam->imgs.image_ring[cam->imgs.ring_out].shot == 0) &&
                (cam->movie_norm || (cam->conf.movie_extpipe_use && cam->extpipe))) {
                /*
                 * movie_last_shoot is -1 when file is created,
                 * we don't know how many frames there is in first sec
                 */
                if (cam->movie_last_shot >= 0) {
                    if (cam_list[0]->log_level >= DBG) {
                        int frames = cam->movie_fps - (cam->movie_last_shot + 1);
                        if (frames > 0) {
                            char tmp[25];
                            MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO
                            ,_("Added %d fillerframes into movie"), frames);
                            sprintf(tmp, "Fillerframes %d", frames);
                            draw_text(cam->imgs.image_ring[cam->imgs.ring_out].image_norm,
                                      cam->imgs.width, cam->imgs.height, 10, 40, tmp, cam->text_scale);
                        }
                    }
                    /* Check how many frames it was last sec */
                    while ((cam->movie_last_shot + 1) < cam->movie_fps) {
                        /* Add a filler frame into encoder */
                        event(cam, EVENT_MOVIE_PUT,
                          &cam->imgs.image_ring[cam->imgs.ring_out], NULL, NULL,
                          &cam->imgs.image_ring[cam->imgs.ring_out].imgts);

                        cam->movie_last_shot++;
                    }
                }
                cam->movie_last_shot = 0;
            } else if (cam->imgs.image_ring[cam->imgs.ring_out].shot != (cam->movie_last_shot + 1)) {
                /* We are out of sync! Propably we got motion - no motion - motion */
                cam->movie_last_shot = -1;
            }

            /*
             * Save last shot added to movie
             * only when we not are within first sec
             */
            if (cam->movie_last_shot >= 0)
                cam->movie_last_shot = cam->imgs.image_ring[cam->imgs.ring_out].shot;
        }

        /* Mark the image as saved */
        cam->imgs.image_ring[cam->imgs.ring_out].flags |= IMAGE_SAVED;

        /* Store it as a preview image, only if it has motion */
        if (cam->imgs.image_ring[cam->imgs.ring_out].flags & IMAGE_MOTION) {
            /* Check for most significant preview-shot when picture_output=best */
            if (cam->new_img & NEWIMG_BEST) {
                if (cam->imgs.image_ring[cam->imgs.ring_out].diffs > cam->imgs.image_preview.diffs) {
                    image_save_as_preview(cam, &cam->imgs.image_ring[cam->imgs.ring_out]);
                }
            }
            /* Check for most significant preview-shot when picture_output=center */
            if (cam->new_img & NEWIMG_CENTER) {
                if (cam->imgs.image_ring[cam->imgs.ring_out].cent_dist < cam->imgs.image_preview.cent_dist) {
                    image_save_as_preview(cam, &cam->imgs.image_ring[cam->imgs.ring_out]);
                }
            }
        }

        /* Increment to image after last sended */
        if (++cam->imgs.ring_out >= cam->imgs.ring_size)
            cam->imgs.ring_out = 0;

        if (max_images != IMAGE_BUFFER_FLUSH) {
            max_images--;
            /* breakout if we have done max_images */
            if (max_images == 0)
                break;
        }

        /* loop until out and in is same e.g. buffer empty */
    } while (cam->imgs.ring_out != cam->imgs.ring_in);

    /* restore global context values */
    cam->current_image = saved_current_image;
}

static int init_camera_type(struct ctx_cam *cam){

    cam->camera_type = CAMERA_TYPE_UNKNOWN;

    if (cam->conf.mmalcam_name) {
        cam->camera_type = CAMERA_TYPE_MMAL;
        return 0;
    }

    if (cam->conf.netcam_url) {
        if ((strncmp(cam->conf.netcam_url,"mjpeg",5) == 0) ||
            (strncmp(cam->conf.netcam_url,"http" ,4) == 0) ||
            (strncmp(cam->conf.netcam_url,"v4l2" ,4) == 0) ||
            (strncmp(cam->conf.netcam_url,"file" ,4) == 0) ||
            (strncmp(cam->conf.netcam_url,"rtmp" ,4) == 0) ||
            (strncmp(cam->conf.netcam_url,"rtsp" ,4) == 0)) {
            cam->camera_type = CAMERA_TYPE_NETCAM;
        }
        return 0;
    }

    #ifdef HAVE_V4L2
        if (cam->conf.video_device) {
            cam->camera_type = CAMERA_TYPE_V4L2;
            return 0;
        }
    #endif // HAVE_V4L2


    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
        , _("Unable to determine camera type (MMAL, Netcam, V4L2)"));
    return -1;

}

static void init_mask_privacy(struct ctx_cam *cam){

    int indxrow, indxcol;
    int start_cr, offset_cb, start_cb;
    int y_index, uv_index;
    int indx_img, indx_max;         /* Counter and max for norm/high */
    int indx_width, indx_height;
    unsigned char *img_temp, *img_temp_uv;


    FILE *picture;

    /* Load the privacy file if any */
    cam->imgs.mask_privacy = NULL;
    cam->imgs.mask_privacy_uv = NULL;
    cam->imgs.mask_privacy_high = NULL;
    cam->imgs.mask_privacy_high_uv = NULL;

    if (cam->conf.mask_privacy) {
        if ((picture = myfopen(cam->conf.mask_privacy, "r"))) {
            MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("Opening privacy mask file"));
            /*
             * NOTE: The mask is expected to have the output dimensions. I.e., the mask
             * applies to the already rotated image, not the capture image. Thus, use
             * width and height from imgs.
             */
            cam->imgs.mask_privacy = get_pgm(picture, cam->imgs.width, cam->imgs.height);

            /* We only need the "or" mask for the U & V chrominance area.  */
            cam->imgs.mask_privacy_uv = mymalloc((cam->imgs.height * cam->imgs.width) / 2);
            if (cam->imgs.size_high > 0){
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
                    ,_("Opening high resolution privacy mask file"));
                rewind(picture);
                cam->imgs.mask_privacy_high = get_pgm(picture, cam->imgs.width_high, cam->imgs.height_high);
                cam->imgs.mask_privacy_high_uv = mymalloc((cam->imgs.height_high * cam->imgs.width_high) / 2);
            }

            myfclose(picture);
        } else {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                ,_("Error opening mask file %s"), cam->conf.mask_privacy);
            /* Try to write an empty mask file to make it easier for the user to edit it */
            put_fixed_mask(cam, cam->conf.mask_privacy);
        }

        if (!cam->imgs.mask_privacy) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("Failed to read mask privacy image. Mask privacy feature disabled."));
        } else {
            MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
            ,_("Mask privacy file \"%s\" loaded."), cam->conf.mask_privacy);

            indx_img = 1;
            indx_max = 1;
            if (cam->imgs.size_high > 0) indx_max = 2;

            while (indx_img <= indx_max){
                if (indx_img == 1){
                    start_cr = (cam->imgs.height * cam->imgs.width);
                    offset_cb = ((cam->imgs.height * cam->imgs.width)/4);
                    start_cb = start_cr + offset_cb;
                    indx_width = cam->imgs.width;
                    indx_height = cam->imgs.height;
                    img_temp = cam->imgs.mask_privacy;
                    img_temp_uv = cam->imgs.mask_privacy_uv;
                } else {
                    start_cr = (cam->imgs.height_high * cam->imgs.width_high);
                    offset_cb = ((cam->imgs.height_high * cam->imgs.width_high)/4);
                    start_cb = start_cr + offset_cb;
                    indx_width = cam->imgs.width_high;
                    indx_height = cam->imgs.height_high;
                    img_temp = cam->imgs.mask_privacy_high;
                    img_temp_uv = cam->imgs.mask_privacy_high_uv;
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

static void init_text_scale(struct ctx_cam *cam){

    /* Consider that web interface may change conf values at any moment.
     * The below can put two sections in the image so make sure that after
     * scaling does not occupy more than 1/4 of image (10 pixels * 2 lines)
     */

    cam->text_scale = cam->conf.text_scale;
    if (cam->text_scale <= 0) cam->text_scale = 1;

    if ((cam->text_scale * 10 * 2) > (cam->imgs.width / 4)) {
        cam->text_scale = (cam->imgs.width / (4 * 10 * 2));
        if (cam->text_scale <= 0) cam->text_scale = 1;
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Invalid text scale.  Adjusted to %d"), cam->text_scale);
    }

    if ((cam->text_scale * 10 * 2) > (cam->imgs.height / 4)) {
        cam->text_scale = (cam->imgs.height / (4 * 10 * 2));
        if (cam->text_scale <= 0) cam->text_scale = 1;
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Invalid text scale.  Adjusted to %d"), cam->text_scale);
    }

    /* If we had to modify the scale, change conf so we don't get another message */
    cam->conf.text_scale = cam->text_scale;

}

/** motion_init */
static int motion_init(struct ctx_cam *cam)
{
    FILE *picture;

    util_threadname_set("ml",cam->threadnr,cam->conf.camera_name);

    /* Store thread number in TLS. */
    pthread_setspecific(tls_key_threadnr, (void *)((unsigned long)cam->threadnr));

    clock_gettime(CLOCK_REALTIME, &cam->frame_last_ts);
    clock_gettime(CLOCK_REALTIME, &cam->frame_curr_ts);

    cam->smartmask_speed = 0;

    /*
     * We initialize cam->event_nr to 1 and cam->prev_event to 0 (not really needed) so
     * that certain code below does not run until motion has been detected the first time
     */
    cam->event_nr = 1;
    cam->prev_event = 0;
    cam->lightswitch_framecounter = 0;
    cam->detecting_motion = 0;
    cam->event_user = FALSE;
    cam->event_stop = FALSE;

    /* Make sure to default the high res to zero */
    cam->imgs.width_high = 0;
    cam->imgs.height_high = 0;
    cam->imgs.size_high = 0;
    cam->movie_passthrough = cam->conf.movie_passthrough;

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
        ,_("Camera %d started: motion detection %s"),
        cam->camera_id, cam->pause ? _("Disabled"):_("Enabled"));

    if (!cam->conf.target_dir)
        cam->conf.target_dir = mystrdup(".");

    if (init_camera_type(cam) != 0 ) return -3;

    if ((cam->camera_type != CAMERA_TYPE_NETCAM) &&
        (cam->movie_passthrough)) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Pass-through processing disabled."));
        cam->movie_passthrough = FALSE;
    }

    if ((cam->conf.height == 0) || (cam->conf.width == 0)) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Invalid configuration dimensions %dx%d"),cam->conf.height,cam->conf.width);
        cam->conf.height = DEF_HEIGHT;
        cam->conf.width = DEF_WIDTH;
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Using default dimensions %dx%d"),cam->conf.height,cam->conf.width);
    }
    if (cam->conf.width % 8) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image width (%d) requested is not modulo 8."), cam->conf.width);
        cam->conf.width = cam->conf.width - (cam->conf.width % 8) + 8;
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Adjusting width to next higher multiple of 8 (%d)."), cam->conf.width);
    }
    if (cam->conf.height % 8) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image height (%d) requested is not modulo 8."), cam->conf.height);
        cam->conf.height = cam->conf.height - (cam->conf.height % 8) + 8;
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Adjusting height to next higher multiple of 8 (%d)."), cam->conf.height);
    }
    if (cam->conf.width  < 64) cam->conf.width  = 64;
    if (cam->conf.height < 64) cam->conf.height = 64;

    /* set the device settings */
    cam->video_dev = vid_start(cam);

    /*
     * We failed to get an initial image from a camera
     * So we need to guess height and width based on the config
     * file options.
     */
    if (cam->video_dev == -1) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Could not fetch initial image from camera "));
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Motion continues using width and height from config file(s)"));
        cam->imgs.width = cam->conf.width;
        cam->imgs.height = cam->conf.height;
        cam->imgs.size_norm = cam->conf.width * cam->conf.height * 3 / 2;
        cam->imgs.motionsize = cam->conf.width * cam->conf.height;
    } else if (cam->video_dev == -2) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Could not fetch initial image from camera "));
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Motion only supports width and height modulo 8"));
        return -3;
    }
    /* Revalidate we got a valid image size */
    if ((cam->imgs.width % 8) || (cam->imgs.height % 8)) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image width (%d) or height(%d) requested is not modulo 8.")
            ,cam->imgs.width, cam->imgs.height);
        return -3;
    }
    if ((cam->imgs.width  < 64) || (cam->imgs.height < 64)){
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Motion only supports width and height greater than or equal to 64 %dx%d")
            ,cam->imgs.width, cam->imgs.height);
            return -3;
    }
    /* Substream size notification*/
    if ((cam->imgs.width % 16) || (cam->imgs.height % 16)) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Substream not available.  Image sizes not modulo 16."));
    }


    /* We set size_high here so that it can be used in the retry function to determine whether
     * we need to break and reallocate buffers
     */
    cam->imgs.size_high = (cam->imgs.width_high * cam->imgs.height_high * 3) / 2;

    image_ring_resize(cam, 1); /* Create a initial precapture ring buffer with 1 frame */

    cam->imgs.ref = mymalloc(cam->imgs.size_norm);
    cam->imgs.image_motion.image_norm = mymalloc(cam->imgs.size_norm);

    /* contains the moving objects of ref. frame */
    cam->imgs.ref_dyn = mymalloc(cam->imgs.motionsize * sizeof(*cam->imgs.ref_dyn));
    cam->imgs.image_virgin = mymalloc(cam->imgs.size_norm);
    cam->imgs.image_vprvcy = mymalloc(cam->imgs.size_norm);
    cam->imgs.smartmask = mymalloc(cam->imgs.motionsize);
    cam->imgs.smartmask_final = mymalloc(cam->imgs.motionsize);
    cam->imgs.smartmask_buffer = mymalloc(cam->imgs.motionsize * sizeof(*cam->imgs.smartmask_buffer));
    cam->imgs.labels = mymalloc(cam->imgs.motionsize * sizeof(*cam->imgs.labels));
    cam->imgs.labelsize = mymalloc((cam->imgs.motionsize/2+1) * sizeof(*cam->imgs.labelsize));
    cam->imgs.image_preview.image_norm = mymalloc(cam->imgs.size_norm);
    cam->imgs.common_buffer = mymalloc(3 * cam->imgs.width * cam->imgs.height);
    if (cam->imgs.size_high > 0){
        cam->imgs.image_preview.image_high = mymalloc(cam->imgs.size_high);
    }

    webu_stream_init(cam);

    /* Set output picture type */
    if (!strcmp(cam->conf.picture_type, "ppm"))
        cam->imgs.picture_type = IMAGE_TYPE_PPM;
    else if (!strcmp(cam->conf.picture_type, "webp")) {
        #ifdef HAVE_WEBP
                cam->imgs.picture_type = IMAGE_TYPE_WEBP;
        #else
                /* Fallback to jpeg if webp was selected in the config file, but the support for it was not compiled in */
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("webp image format is not available, failing back to jpeg"));
                cam->imgs.picture_type = IMAGE_TYPE_JPEG;
        #endif /* HAVE_WEBP */
    }
    else
        cam->imgs.picture_type = IMAGE_TYPE_JPEG;

    /*
     * Now is a good time to init rotation data. Since vid_start has been
     * called, we know that we have imgs.width and imgs.height. When capturing
     * from a V4L device, these are copied from the corresponding conf values
     * in vid_start. When capturing from a netcam, they get set in netcam_start,
     * which is called from vid_start.
     *
     * rotate_init will set cap_width and cap_height in cam->rotate_data.
     */
    rotate_init(cam); /* rotate_deinit is called in main */

    init_text_scale(cam);   /*Initialize and validate the text_scale */

    /* Capture first image, or we will get an alarm on start */
    if (cam->video_dev >= 0) {
        int i;

        for (i = 0; i < 5; i++) {
            if (vid_next(cam, cam->current_image) == 0)
                break;
            SLEEP(2, 0);
        }

        if (i >= 5) {
            memset(cam->imgs.image_virgin, 0x80, cam->imgs.size_norm);       /* initialize to grey */
            draw_text(cam->imgs.image_virgin, cam->imgs.width, cam->imgs.height,
                      10, 20, "Error capturing first image", cam->text_scale);
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error capturing first image"));
        }
    }
    cam->current_image = &cam->imgs.image_ring[cam->imgs.ring_in];

    /* create a reference frame */
    alg_update_reference_frame(cam, RESET_REF_FRAME);

    #if defined(HAVE_V4L2) && !defined(BSD)
        /* open video loopback devices if enabled */
        if (cam->conf.video_pipe) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Opening video loopback device for normal pictures"));

            /* vid_startpipe should get the output dimensions */
            cam->pipe = vlp_startpipe(cam->conf.video_pipe, cam->imgs.width, cam->imgs.height);

            if (cam->pipe < 0) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Failed to open video loopback for normal pictures"));
                return -1;
            }
        }

        if (cam->conf.video_pipe_motion) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Opening video loopback device for motion pictures"));

            /* vid_startpipe should get the output dimensions */
            cam->mpipe = vlp_startpipe(cam->conf.video_pipe_motion, cam->imgs.width, cam->imgs.height);

            if (cam->mpipe < 0) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Failed to open video loopback for motion pictures"));
                return -1;
            }
        }
    #endif /* HAVE_V4L2 && !BSD */

    dbse_init(cam);

    /* Load the mask file if any */
    if (cam->conf.mask_file) {
        if ((picture = myfopen(cam->conf.mask_file, "r"))) {
            /*
             * NOTE: The mask is expected to have the output dimensions. I.e., the mask
             * applies to the already rotated image, not the capture image. Thus, use
             * width and height from imgs.
             */
            cam->imgs.mask = get_pgm(picture, cam->imgs.width, cam->imgs.height);
            myfclose(picture);
        } else {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO
                ,_("Error opening mask file %s")
                ,cam->conf.mask_file);
            /*
             * Try to write an empty mask file to make it easier
             * for the user to edit it
             */
            put_fixed_mask(cam, cam->conf.mask_file);
        }

        if (!cam->imgs.mask) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("Failed to read mask image. Mask feature disabled."));
        } else {
            MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
                ,_("Maskfile \"%s\" loaded.")
                ,cam->conf.mask_file);
        }
    } else {
        cam->imgs.mask = NULL;
    }

    init_mask_privacy(cam);

    /* Always initialize smart_mask - someone could turn it on later... */
    memset(cam->imgs.smartmask, 0, cam->imgs.motionsize);
    memset(cam->imgs.smartmask_final, 255, cam->imgs.motionsize);
    memset(cam->imgs.smartmask_buffer, 0, cam->imgs.motionsize * sizeof(*cam->imgs.smartmask_buffer));

    /* Set noise level */
    cam->noise = cam->conf.noise_level;

    /* Set threshold value */
    cam->threshold = cam->conf.threshold;
    if (cam->conf.threshold_maximum > cam->conf.threshold ){
        cam->threshold_maximum = cam->conf.threshold_maximum;
    } else {
        cam->threshold_maximum = (cam->imgs.height * cam->imgs.width * 3) / 2;
    }

    /* Prevent first few frames from triggering motion... */
    cam->moved = 8;

    /* Work out expected frame rate based on config setting */
    if (cam->conf.framerate < 2)
        cam->conf.framerate = 2;

    /* 2 sec startup delay so FPS is calculated correct */
    cam->startup_frames = (cam->conf.framerate * 2) + cam->conf.pre_capture + cam->conf.minimum_motion_frames;

    cam->required_frame_time = 1000000L / cam->conf.framerate;

    cam->frame_delay = cam->required_frame_time;

    cam->track_posx = 0;
    cam->track_posy = 0;
    if (cam->track.type)
        cam->moved = track_center(cam, cam->video_dev, 0, 0, 0);

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

    cam->timenow = 0;
    cam->timebefore = 0;
    cam->rate_limit = 0;
    cam->lastframetime = 0;
    cam->minimum_frame_time_downcounter = cam->conf.minimum_frame_time;
    cam->get_image = 1;

    cam->olddiffs = 0;
    cam->smartmask_ratio = 0;
    cam->smartmask_count = 20;

    cam->previous_diffs = 0;
    cam->previous_location_x = 0;
    cam->previous_location_y = 0;

    cam->time_last_frame = 1;
    cam->time_current_frame = 0;

    cam->smartmask_lastrate = 0;

    cam->passflag = 0;  //only purpose to flag first frame
    cam->rolling_frame = 0;

    if (cam->conf.emulate_motion) {
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
 *      cam     Pointer to the motion context structure
 *
 * Returns:     nothing
 */
static void motion_cleanup(struct ctx_cam *cam) {

    event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, NULL);
    event(cam, EVENT_ENDMOTION, NULL, NULL, NULL, NULL);

    webu_stream_deinit(cam);

    if (cam->video_dev >= 0) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("Calling vid_close() from motion_cleanup"));
        vid_close(cam);
    }

    free(cam->imgs.image_motion.image_norm);
    cam->imgs.image_motion.image_norm = NULL;

    free(cam->imgs.ref);
    cam->imgs.ref = NULL;

    free(cam->imgs.ref_dyn);
    cam->imgs.ref_dyn = NULL;

    free(cam->imgs.image_virgin);
    cam->imgs.image_virgin = NULL;

    free(cam->imgs.image_vprvcy);
    cam->imgs.image_vprvcy = NULL;

    free(cam->imgs.labels);
    cam->imgs.labels = NULL;

    free(cam->imgs.labelsize);
    cam->imgs.labelsize = NULL;

    free(cam->imgs.smartmask);
    cam->imgs.smartmask = NULL;

    free(cam->imgs.smartmask_final);
    cam->imgs.smartmask_final = NULL;

    free(cam->imgs.smartmask_buffer);
    cam->imgs.smartmask_buffer = NULL;

    if (cam->imgs.mask) free(cam->imgs.mask);
    cam->imgs.mask = NULL;

    if (cam->imgs.mask_privacy) free(cam->imgs.mask_privacy);
    cam->imgs.mask_privacy = NULL;

    if (cam->imgs.mask_privacy_uv) free(cam->imgs.mask_privacy_uv);
    cam->imgs.mask_privacy_uv = NULL;

    if (cam->imgs.mask_privacy_high) free(cam->imgs.mask_privacy_high);
    cam->imgs.mask_privacy_high = NULL;

    if (cam->imgs.mask_privacy_high_uv) free(cam->imgs.mask_privacy_high_uv);
    cam->imgs.mask_privacy_high_uv = NULL;

    free(cam->imgs.common_buffer);
    cam->imgs.common_buffer = NULL;

    free(cam->imgs.image_preview.image_norm);
    cam->imgs.image_preview.image_norm = NULL;

    if (cam->imgs.size_high > 0){
        free(cam->imgs.image_preview.image_high);
        cam->imgs.image_preview.image_high = NULL;
    }

    image_ring_destroy(cam); /* Cleanup the precapture ring buffer */

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

static void mlp_mask_privacy(struct ctx_cam *cam){

    if (cam->imgs.mask_privacy == NULL) return;

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
    if (cam->imgs.size_high > 0) indx_max = 2;
    increment = sizeof(unsigned long);

    while (indx_img <= indx_max){
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
            if (*(mask++) == 0x00) *image = 0x80; // Mask last remaining bytes.
            image += 1;
        }

        indx_img++;
    }
}

static void mlp_areadetect(struct ctx_cam *cam){
    int i, j, z = 0;
    /*
     * Simple hack to recognize motion in a specific area
     * Do we need a new coversion specifier as well??
     */
    if ((cam->conf.area_detect) &&
        (cam->event_nr != cam->areadetect_eventnbr) &&
        (cam->current_image->flags & IMAGE_TRIGGER)) {
        j = strlen(cam->conf.area_detect);
        for (i = 0; i < j; i++) {
            z = cam->conf.area_detect[i] - 49; /* characters are stored as ascii 48-57 (0-9) */
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

static void mlp_prepare(struct ctx_cam *cam){

    int frame_buffer_size;

    cam->watchdog = WATCHDOG_TMO;

    cam->timebefore = cam->timenow;

    cam->frame_last_ts.tv_sec = cam->frame_curr_ts.tv_sec;
    cam->frame_last_ts.tv_nsec = cam->frame_curr_ts.tv_nsec;
    clock_gettime(CLOCK_REALTIME, &cam->frame_curr_ts);

    /*
     * Calculate detection rate limit. Above 5fps we limit the detection
     * rate to 3fps to reduce load at higher framerates.
     */
    cam->process_thisframe = 0;
    cam->rate_limit++;
    if (cam->rate_limit >= (cam->lastrate / 3)) {
        cam->rate_limit = 0;
        cam->process_thisframe = 1;
    }

    if (cam->conf.minimum_motion_frames < 1)
        cam->conf.minimum_motion_frames = 1;

    if (cam->conf.pre_capture < 0)
        cam->conf.pre_capture = 0;

    frame_buffer_size = cam->conf.pre_capture + cam->conf.minimum_motion_frames;

    if (cam->imgs.ring_size != frame_buffer_size)
        image_ring_resize(cam, frame_buffer_size);

    /*
     * If we have started on a new second we reset the shots variable
     * lastrate is updated to be the number of the last frame. last rate
     * is used as the ffmpeg framerate when motion is detected.
     */
    if (cam->frame_last_ts.tv_sec != cam->frame_curr_ts.tv_sec) {
        cam->lastrate = cam->shots + 1;
        cam->shots = -1;
        cam->lastframetime = cam->currenttime;

        if (cam->conf.minimum_frame_time) {
            cam->minimum_frame_time_downcounter--;
            if (cam->minimum_frame_time_downcounter == 0)
                cam->get_image = 1;
        } else {
            cam->get_image = 1;
        }
    }


    /* Increase the shots variable for each frame captured within this second */
    cam->shots++;

    if (cam->startup_frames > 0)
        cam->startup_frames--;


}

static void mlp_resetimages(struct ctx_cam *cam){

    struct ctx_image_data *old_image;

    if (cam->conf.minimum_frame_time) {
        cam->minimum_frame_time_downcounter = cam->conf.minimum_frame_time;
        cam->get_image = 0;
    }

    /* ring_buffer_in is pointing to current pos, update before put in a new image */
    if (++cam->imgs.ring_in >= cam->imgs.ring_size)
        cam->imgs.ring_in = 0;

    /* Check if we have filled the ring buffer, throw away last image */
    if (cam->imgs.ring_in == cam->imgs.ring_out) {
        if (++cam->imgs.ring_out >= cam->imgs.ring_size)
            cam->imgs.ring_out = 0;
    }

    /* cam->current_image points to position in ring where to store image, diffs etc. */
    old_image = cam->current_image;
    cam->current_image = &cam->imgs.image_ring[cam->imgs.ring_in];

    /* Init/clear current_image */
    if (cam->process_thisframe) {
        /* set diffs to 0 now, will be written after we calculated diffs in new image */
        cam->current_image->diffs = 0;

        /* Set flags to 0 */
        cam->current_image->flags = 0;
        cam->current_image->cent_dist = 0;

        /* Clear location data */
        memset(&cam->current_image->location, 0, sizeof(cam->current_image->location));
        cam->current_image->total_labels = 0;
    } else if (cam->current_image && old_image) {
        /* not processing this frame: save some important values for next image */
        cam->current_image->diffs = old_image->diffs;
        cam->current_image->imgts = old_image->imgts;
        cam->current_image->shot = old_image->shot;
        cam->current_image->cent_dist = old_image->cent_dist;
        cam->current_image->flags = old_image->flags & (~IMAGE_SAVED);
        cam->current_image->location = old_image->location;
        cam->current_image->total_labels = old_image->total_labels;
    }

    clock_gettime(CLOCK_REALTIME, &cam->current_image->imgts);

    /* Store shot number with pre_captured image */
    cam->current_image->shot = cam->shots;

}

static int mlp_retry(struct ctx_cam *cam){

    /*
     * If a camera is not available we keep on retrying every 10 seconds
     * until it shows up.
     */
    int size_high;

    if (cam->video_dev < 0 &&
        cam->currenttime % 10 == 0 && cam->shots == 0) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Retrying until successful connection with camera"));
        cam->video_dev = vid_start(cam);

        if (cam->video_dev < 0) {
            return 1;
        }

        if ((cam->imgs.width % 8) || (cam->imgs.height % 8)) {
            MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("Image width (%d) or height(%d) requested is not modulo 8.")
                ,cam->imgs.width, cam->imgs.height);
            return 1;
        }

        if ((cam->imgs.width  < 64) || (cam->imgs.height < 64)){
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("Motion only supports width and height greater than or equal to 64 %dx%d")
                ,cam->imgs.width, cam->imgs.height);
                return 1;
        }

        /*
         * If the netcam has different dimensions than in the config file
         * we need to restart Motion to re-allocate all the buffers
         */
        if (cam->imgs.width != cam->conf.width || cam->imgs.height != cam->conf.height) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Camera has finally become available\n"
                       "Camera image has different width and height"
                       "from what is in the config file. You should fix that\n"
                       "Restarting Motion thread to reinitialize all "
                       "image buffers to new picture dimensions"));
            cam->conf.width = cam->imgs.width;
            cam->conf.height = cam->imgs.height;
            /*
             * Break out of main loop terminating thread
             * watchdog will start us again
             */
            return 1;
        }
        /*
         * For high res, we check the size of buffer to determine whether to break out
         * the init_motion function allocated the buffer for high using the cam->imgs.size_high
         * and the vid_start ONLY re-populates the height/width so we can check the size here.
         */
        size_high = (cam->imgs.width_high * cam->imgs.height_high * 3) / 2;
        if (cam->imgs.size_high != size_high) return 1;
    }
    return 0;
}

static int mlp_capture(struct ctx_cam *cam){

    const char *tmpin;
    char tmpout[80];
    int vid_return_code = 0;        /* Return code used when calling vid_next */
    struct timespec ts1;

    /***** MOTION LOOP - IMAGE CAPTURE SECTION *****/
    /*
     * Fetch next frame from camera
     * If vid_next returns 0 all is well and we got a new picture
     * Any non zero value is an error.
     * 0 = OK, valid picture
     * <0 = fatal error - leave the thread by breaking out of the main loop
     * >0 = non fatal error - copy last image or show grey image with message
     */
    if (cam->video_dev >= 0)
        vid_return_code = vid_next(cam, cam->current_image);
    else
        vid_return_code = 1; /* Non fatal error */

    // VALID PICTURE
    if (vid_return_code == 0) {
        cam->lost_connection = 0;
        cam->connectionlosttime = 0;

        /* If all is well reset missing_frame_counter */
        if (cam->missing_frame_counter >= MISSING_FRAMES_TIMEOUT * cam->conf.framerate) {
            /* If we previously logged starting a grey image, now log video re-start */
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Video signal re-acquired"));
            // event for re-acquired video signal can be called here
            event(cam, EVENT_CAMERA_FOUND, NULL, NULL, NULL, NULL);
        }
        cam->missing_frame_counter = 0;

        /*
         * Save the newly captured still virgin image to a buffer
         * which we will not alter with text and location graphics
         */
        memcpy(cam->imgs.image_virgin, cam->current_image->image_norm, cam->imgs.size_norm);

        mlp_mask_privacy(cam);

        memcpy(cam->imgs.image_vprvcy, cam->current_image->image_norm, cam->imgs.size_norm);

    // FATAL ERROR - leave the thread by breaking out of the main loop
    } else if (vid_return_code < 0) {
        /* Fatal error - Close video device */
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Video device fatal error - Closing video device"));
        vid_close(cam);
        /*
         * Use virgin image, if we are not able to open it again next loop
         * a gray image with message is applied
         * flag lost_connection
         */
        memcpy(cam->current_image->image_norm, cam->imgs.image_virgin, cam->imgs.size_norm);
        cam->lost_connection = 1;
    /* NO FATAL ERROR -
    *        copy last image or show grey image with message
    *        flag on lost_connection if :
    *               vid_return_code == NETCAM_RESTART_ERROR
    *        cam->video_dev < 0
    *        cam->missing_frame_counter > (MISSING_FRAMES_TIMEOUT * cam->conf.framerate)
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
            cam->lost_connection = 1;
            return 1;
        }

        /*
         * First missed frame - store timestamp
         * Don't reset time when thread restarts
         */
        if (cam->connectionlosttime == 0){
            cam->connectionlosttime = cam->currenttime;
        }


        /*
         * Increase missing_frame_counter
         * The first MISSING_FRAMES_TIMEOUT seconds we copy previous virgin image
         * After MISSING_FRAMES_TIMEOUT seconds we put a grey error image in the buffer
         * If we still have not yet received the initial image from a camera
         * we go straight for the grey error image.
         */
        ++cam->missing_frame_counter;

        if (cam->video_dev >= 0 &&
            cam->missing_frame_counter < (MISSING_FRAMES_TIMEOUT * cam->conf.framerate)) {
            memcpy(cam->current_image->image_norm, cam->imgs.image_vprvcy, cam->imgs.size_norm);
        } else {
            cam->lost_connection = 1;

            if (cam->video_dev >= 0)
                tmpin = "CONNECTION TO CAMERA LOST\\nSINCE %Y-%m-%d %T";
            else
                tmpin = "UNABLE TO OPEN VIDEO DEVICE\\nSINCE %Y-%m-%d %T";

            ts1.tv_sec=cam->connectionlosttime;
            ts1.tv_nsec = 0;
            memset(cam->current_image->image_norm, 0x80, cam->imgs.size_norm);
            mystrftime(cam, tmpout, sizeof(tmpout), tmpin, &ts1, NULL, 0);
            draw_text(cam->current_image->image_norm, cam->imgs.width, cam->imgs.height,
                      10, 20 * cam->text_scale, tmpout, cam->text_scale);

            /* Write error message only once */
            if (cam->missing_frame_counter == MISSING_FRAMES_TIMEOUT * cam->conf.framerate) {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                    ,_("Video signal lost - Adding grey image"));
                // Event for lost video signal can be called from here
                event(cam, EVENT_CAMERA_LOST, NULL, NULL, NULL, &ts1);
            }

            /*
             * If we don't get a valid frame for a long time, try to close/reopen device
             * Only try this when a device is open
             */
            if ((cam->video_dev > 0) &&
                (cam->missing_frame_counter == (MISSING_FRAMES_TIMEOUT * 4) * cam->conf.framerate)) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Video signal still lost - "
                    "Trying to close video device"));
                vid_close(cam);
            }
        }
    }
    return 0;

}

static void mlp_detection(struct ctx_cam *cam){


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
    if (cam->process_thisframe) {
        if (cam->threshold && !cam->pause) {
            /*
             * If we've already detected motion and we want to see if there's
             * still motion, don't bother trying the fast one first. IF there's
             * motion, the alg_diff will trigger alg_diff_standard
             * anyway
             */
            if (cam->detecting_motion || cam->conf.setup_mode)
                cam->current_image->diffs = alg_diff_standard(cam, cam->imgs.image_vprvcy);
            else
                cam->current_image->diffs = alg_diff(cam, cam->imgs.image_vprvcy);

            /* Lightswitch feature - has light intensity changed?
             * This can happen due to change of light conditions or due to a sudden change of the camera
             * sensitivity. If alg_lightswitch detects lightswitch we suspend motion detection the next
             * 'lightswitch_frames' frames to allow the camera to settle.
             * Don't check if we have lost connection, we detect "Lost signal" frame as lightswitch
             */
            if (cam->conf.lightswitch_percent > 1 && !cam->lost_connection) {
                if (alg_lightswitch(cam, cam->current_image->diffs)) {
                    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("Lightswitch detected"));

                    if (cam->conf.lightswitch_frames < 1)
                        cam->conf.lightswitch_frames = 1;
                    else if (cam->conf.lightswitch_frames > 1000)
                        cam->conf.lightswitch_frames = 1000;

                    if (cam->moved < (unsigned int)cam->conf.lightswitch_frames)
                        cam->moved = (unsigned int)cam->conf.lightswitch_frames;

                    cam->current_image->diffs = 0;
                    alg_update_reference_frame(cam, RESET_REF_FRAME);
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
            if (cam->conf.roundrobin_switchfilter && cam->current_image->diffs > cam->threshold) {
                cam->current_image->diffs = alg_switchfilter(cam, cam->current_image->diffs,
                                                             cam->current_image->image_norm);

                if ((cam->current_image->diffs <= cam->threshold) ||
                    (cam->current_image->diffs > cam->threshold_maximum)) {

                    cam->current_image->diffs = 0;
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
            cam->current_image->total_labels = 0;
            cam->imgs.largest_label = 0;
            cam->olddiffs = 0;

            if (cam->conf.despeckle_filter && cam->current_image->diffs > 0) {
                cam->olddiffs = cam->current_image->diffs;
                cam->current_image->diffs = alg_despeckle(cam, cam->olddiffs);
            } else if (cam->imgs.labelsize_max) {
                cam->imgs.labelsize_max = 0; /* Disable labeling if enabled */
            }

        } else if (!cam->conf.setup_mode) {
            cam->current_image->diffs = 0;
        }
    }

    //TODO:  This section needs investigation for purpose, cause and effect
    /* Manipulate smart_mask sensitivity (only every smartmask_ratio seconds) */
    if ((cam->smartmask_speed && (cam->event_nr != cam->prev_event)) &&
        (!--cam->smartmask_count)) {
        alg_tune_smartmask(cam);
        cam->smartmask_count = cam->smartmask_ratio;
    }

    /*
     * cam->moved is set by the tracking code when camera has been asked to move.
     * When camera is moving we do not want motion to detect motion or we will
     * get our camera chasing itself like crazy and we will get motion detected
     * which is not really motion. So we pretend there is no motion by setting
     * cam->diffs = 0.
     * We also pretend to have a moving camera when we start Motion and when light
     * switch has been detected to allow camera to settle.
     */
    if (cam->moved) {
        cam->moved--;
        cam->current_image->diffs = 0;
    }

}

static void mlp_tuning(struct ctx_cam *cam){

    /***** MOTION LOOP - TUNING SECTION *****/

    /*
     * If noise tuning was selected, do it now. but only when
     * no frames have been recorded and only once per second
     */
    if ((cam->conf.noise_tune && cam->shots == 0) &&
         (!cam->detecting_motion && (cam->current_image->diffs <= cam->threshold)))
        alg_noise_tune(cam, cam->imgs.image_vprvcy);


    /*
     * If we are not noise tuning lets make sure that remote controlled
     * changes of noise_level are used.
     */
    if (cam->process_thisframe) {
        /*
         * threshold tuning if enabled
         * if we are not threshold tuning lets make sure that remote controlled
         * changes of threshold are used.
         */
        if (cam->conf.threshold_tune){
            alg_threshold_tune(cam, cam->current_image->diffs, cam->detecting_motion);
        }

        /*
         * If motion is detected (cam->current_image->diffs > cam->threshold) and before we add text to the pictures
         * we find the center and size coordinates of the motion to be used for text overlays and later
         * for adding the locate rectangle
         */
        if ((cam->current_image->diffs > cam->threshold) &&
            (cam->current_image->diffs < cam->threshold_maximum)){

            alg_locate_center_size(&cam->imgs
                , cam->imgs.width
                , cam->imgs.height
                , &cam->current_image->location);
            }

        /*
         * Update reference frame.
         * micro-lighswitch: trying to auto-detect lightswitch events.
         * frontdoor illumination. Updates are rate-limited to 3 per second at
         * framerates above 5fps to save CPU resources and to keep sensitivity
         * at a constant level.
         */

        if ((cam->current_image->diffs > cam->threshold) &&
            (cam->current_image->diffs < cam->threshold_maximum) &&
            (cam->conf.lightswitch_percent >= 1) &&
            (cam->lightswitch_framecounter < (cam->lastrate * 2)) && /* two seconds window only */
            /* number of changed pixels almost the same in two consecutive frames and */
            ((abs(cam->previous_diffs - cam->current_image->diffs)) < (cam->previous_diffs / 15)) &&
            /* center of motion in about the same place ? */
            ((abs(cam->current_image->location.x - cam->previous_location_x)) <= (cam->imgs.width / 150)) &&
            ((abs(cam->current_image->location.y - cam->previous_location_y)) <= (cam->imgs.height / 150))) {
            alg_update_reference_frame(cam, RESET_REF_FRAME);
            cam->current_image->diffs = 0;
            cam->lightswitch_framecounter = 0;

            MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("micro-lightswitch!"));
        } else {
            alg_update_reference_frame(cam, UPDATE_REF_FRAME);
        }
        cam->previous_diffs = cam->current_image->diffs;
        cam->previous_location_x = cam->current_image->location.x;
        cam->previous_location_y = cam->current_image->location.y;
    }


}

static void mlp_overlay(struct ctx_cam *cam){

    char tmp[PATH_MAX];

    /***** MOTION LOOP - TEXT AND GRAPHICS OVERLAY SECTION *****/
    /*
     * Some overlays on top of the motion image
     * Note that these now modifies the cam->imgs.out so this buffer
     * can no longer be used for motion detection features until next
     * picture frame is captured.
     */

    /* Smartmask overlay */
    if (cam->smartmask_speed &&
        (cam->conf.picture_output_motion || cam->conf.movie_output_motion ||
         cam->conf.setup_mode || (cam->stream.motion.cnct_count > 0)))
        overlay_smartmask(cam, cam->imgs.image_motion.image_norm);

    /* Largest labels overlay */
    if (cam->imgs.largest_label && (cam->conf.picture_output_motion || cam->conf.movie_output_motion ||
        cam->conf.setup_mode || (cam->stream.motion.cnct_count > 0)))
        overlay_largest_label(cam, cam->imgs.image_motion.image_norm);

    /* Fixed mask overlay */
    if (cam->imgs.mask && (cam->conf.picture_output_motion || cam->conf.movie_output_motion ||
        cam->conf.setup_mode || (cam->stream.motion.cnct_count > 0)))
        overlay_fixed_mask(cam, cam->imgs.image_motion.image_norm);

    /* Add changed pixels in upper right corner of the pictures */
    if (cam->conf.text_changes) {
        if (!cam->pause)
            sprintf(tmp, "%d", cam->current_image->diffs);
        else
            sprintf(tmp, "-");

        draw_text(cam->current_image->image_norm, cam->imgs.width, cam->imgs.height,
                  cam->imgs.width - 10, 10, tmp, cam->text_scale);
    }

    /*
     * Add changed pixels to motion-images (for stream) in setup_mode
     * and always overlay smartmask (not only when motion is detected)
     */
    if (cam->conf.setup_mode || (cam->stream.motion.cnct_count > 0)) {
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
    if (cam->conf.text_left) {
        mystrftime(cam, tmp, sizeof(tmp), cam->conf.text_left,
                   &cam->current_image->imgts, NULL, 0);
        draw_text(cam->current_image->image_norm, cam->imgs.width, cam->imgs.height,
                  10, cam->imgs.height - (10 * cam->text_scale), tmp, cam->text_scale);
    }

    /* Add text in lower right corner of the pictures */
    if (cam->conf.text_right) {
        mystrftime(cam, tmp, sizeof(tmp), cam->conf.text_right,
                   &cam->current_image->imgts, NULL, 0);
        draw_text(cam->current_image->image_norm, cam->imgs.width, cam->imgs.height,
                  cam->imgs.width - 10, cam->imgs.height - (10 * cam->text_scale),
                  tmp, cam->text_scale);
    }

}

static void mlp_actions(struct ctx_cam *cam){

    int indx;

    /***** MOTION LOOP - ACTIONS AND EVENT CONTROL SECTION *****/

    if ((cam->current_image->diffs > cam->threshold) &&
        (cam->current_image->diffs < cam->threshold_maximum)) {
        /* flag this image, it have motion */
        cam->current_image->flags |= IMAGE_MOTION;
        cam->lightswitch_framecounter++; /* micro lightswitch */
    } else {
        cam->lightswitch_framecounter = 0;
    }

    /*
     * If motion has been detected we take action and start saving
     * pictures and movies etc by calling motion_detected().
     * Is emulate_motion enabled we always call motion_detected()
     * If post_capture is enabled we also take care of this in the this
     * code section.
     */
    if ((cam->conf.emulate_motion || cam->event_user) && (cam->startup_frames == 0)) {
        /*  If we were previously detecting motion, started a movie, then got
         *  no motion then we reset the start movie time so that we do not
         *  get a pause in the movie.
        */
        if ( (cam->detecting_motion == 0) && (cam->movie_norm != NULL) )
            movie_reset_start_time(cam->movie_norm, &cam->current_image->imgts);
        cam->detecting_motion = 1;
        if (cam->conf.post_capture > 0) {
            /* Setup the postcap counter */
            cam->postcap = cam->conf.post_capture;
            // MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "(Em) Init post capture %d", cam->postcap);
        }

        cam->current_image->flags |= (IMAGE_TRIGGER | IMAGE_SAVE);
        /* Mark all images in image_ring to be saved */
        for (indx = 0; indx < cam->imgs.ring_size; indx++){
            cam->imgs.image_ring[indx].flags |= IMAGE_SAVE;
        }

        motion_detected(cam, cam->video_dev, cam->current_image);
    } else if ((cam->current_image->flags & IMAGE_MOTION) && (cam->startup_frames == 0)) {
        /*
         * Did we detect motion (like the cat just walked in :) )?
         * If so, ensure the motion is sustained if minimum_motion_frames
         */

        /* Count how many frames with motion there is in the last minimum_motion_frames in precap buffer */
        int frame_count = 0;
        int pos = cam->imgs.ring_in;

        for (indx = 0; indx < cam->conf.minimum_motion_frames; indx++) {
            if (cam->imgs.image_ring[pos].flags & IMAGE_MOTION)
                frame_count++;

            if (pos == 0)
                pos = cam->imgs.ring_size-1;
            else
                pos--;
        }

        if (frame_count >= cam->conf.minimum_motion_frames) {

            cam->current_image->flags |= (IMAGE_TRIGGER | IMAGE_SAVE);
            /*  If we were previously detecting motion, started a movie, then got
             *  no motion then we reset the start movie time so that we do not
             *  get a pause in the movie.
            */
            if ( (cam->detecting_motion == 0) && (cam->movie_norm != NULL) )
                movie_reset_start_time(cam->movie_norm, &cam->current_image->imgts);

            cam->detecting_motion = 1;

            /* Setup the postcap counter */
            cam->postcap = cam->conf.post_capture;
            //MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "Setup post capture %d", cam->postcap);

            /* Mark all images in image_ring to be saved */
            for (indx = 0; indx < cam->imgs.ring_size; indx++)
                cam->imgs.image_ring[indx].flags |= IMAGE_SAVE;

        } else if (cam->postcap > 0) {
           /* we have motion in this frame, but not enought frames for trigger. Check postcap */
            cam->current_image->flags |= (IMAGE_POSTCAP | IMAGE_SAVE);
            cam->postcap--;
            //MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "post capture %d", cam->postcap);
        } else {
            cam->current_image->flags |= IMAGE_PRECAP;
        }

        /* Always call motion_detected when we have a motion image */
        motion_detected(cam, cam->video_dev, cam->current_image);
    } else if (cam->postcap > 0) {
        /* No motion, doing postcap */
        cam->current_image->flags |= (IMAGE_POSTCAP | IMAGE_SAVE);
        cam->postcap--;
        //MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "post capture %d", cam->postcap);
    } else {
        /* Done with postcap, so just have the image in the precap buffer */
        cam->current_image->flags |= IMAGE_PRECAP;
        /* gapless movie feature */
        if ((cam->conf.event_gap == 0) && (cam->detecting_motion == 1))
            cam->event_stop = TRUE;
        cam->detecting_motion = 0;
    }

    /* Update last frame saved time, so we can end event after gap time */
    if (cam->current_image->flags & IMAGE_SAVE)
        cam->lasttime = cam->current_image->imgts.tv_sec;


    mlp_areadetect(cam);

    /*
     * Is the movie too long? Then make movies
     * First test for movie_max_time
     */
    if ((cam->conf.movie_max_time && cam->event_nr == cam->prev_event) &&
        (cam->currenttime - cam->eventtime >= cam->conf.movie_max_time))
        cam->event_stop = TRUE;

    /*
     * Now test for quiet longer than 'gap' OR make movie as decided in
     * previous statement.
     */
    if (((cam->currenttime - cam->lasttime >= cam->conf.event_gap) && cam->conf.event_gap > 0) ||
          cam->event_stop) {
        if (cam->event_nr == cam->prev_event || cam->event_stop) {

            /* Flush image buffer */
            process_image_ring(cam, IMAGE_BUFFER_FLUSH);

            /* Save preview_shot here at the end of event */
            if (cam->imgs.image_preview.diffs) {
                event(cam, EVENT_IMAGE_PREVIEW, NULL, NULL, NULL, &cam->current_image->imgts);
                cam->imgs.image_preview.diffs = 0;
            }

            event(cam, EVENT_ENDMOTION, NULL, NULL, NULL, &cam->current_image->imgts);

            /*
             * If tracking is enabled we center our camera so it does not
             * point to a place where it will miss the next action
             */
            if (cam->track.type)
                cam->moved = track_center(cam, cam->video_dev, 0, 0, 0);

            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("End of event %d"), cam->event_nr);

            cam->event_stop = FALSE;
            cam->event_user = FALSE;

            /* Reset post capture */
            cam->postcap = 0;

            /* Finally we increase the event number */
            cam->event_nr++;
            cam->lightswitch_framecounter = 0;

            /*
             * And we unset the text_event_string to avoid that buffered
             * images get a timestamp from previous event.
             */
            cam->text_event_string[0] = '\0';
        }
    }

    /* Save/send to movie some images */
    process_image_ring(cam, 2);


}

static void mlp_setupmode(struct ctx_cam *cam){
    /***** MOTION LOOP - SETUP MODE CONSOLE OUTPUT SECTION *****/

    /* If CAMERA_VERBOSE enabled output some numbers to console */
    if (cam->conf.setup_mode) {
        char msg[1024] = "\0";
        char part[100];

        if (cam->conf.despeckle_filter) {
            snprintf(part, 99, _("Raw changes: %5d - changes after '%s': %5d"),
                     cam->olddiffs, cam->conf.despeckle_filter, cam->current_image->diffs);
            strcat(msg, part);
            if (strchr(cam->conf.despeckle_filter, 'l')) {
                snprintf(part, 99,_(" - labels: %3d"), cam->current_image->total_labels);
                strcat(msg, part);
            }
        } else {
            snprintf(part, 99,_("Changes: %5d"), cam->current_image->diffs);
            strcat(msg, part);
        }

        if (cam->conf.noise_tune) {
            snprintf(part, 99,_(" - noise level: %2d"), cam->noise);
            strcat(msg, part);
        }

        if (cam->conf.threshold_tune) {
            snprintf(part, 99, _(" - threshold: %d"), cam->threshold);
            strcat(msg, part);
        }

        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "%s", msg);
    }

}

static void mlp_snapshot(struct ctx_cam *cam){
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
    cam->time_current_frame = cam->currenttime;

    if ((cam->conf.snapshot_interval > 0 && cam->shots == 0 &&
         cam->frame_curr_ts.tv_sec % cam->conf.snapshot_interval <=
         cam->frame_last_ts.tv_sec % cam->conf.snapshot_interval) ||
         cam->snapshot) {
        event(cam, EVENT_IMAGE_SNAPSHOT, cam->current_image, NULL, NULL, &cam->current_image->imgts);
        cam->snapshot = 0;
    }

}

static void mlp_timelapse(struct ctx_cam *cam){

    struct tm timestamp_tm;

    if (cam->conf.timelapse_interval) {
        localtime_r(&cam->current_image->imgts.tv_sec, &timestamp_tm);

        /*
         * Check to see if we should start a new timelapse file. We start one when
         * we are on the first shot, and and the seconds are zero. We must use the seconds
         * to prevent the timelapse file from getting reset multiple times during the minute.
         */
        if (timestamp_tm.tm_min == 0 &&
            (cam->frame_curr_ts.tv_sec % 60 < cam->frame_last_ts.tv_sec % 60) &&
            cam->shots == 0) {

            if (strcasecmp(cam->conf.timelapse_mode, "manual") == 0) {
                ;/* No action */

            /* If we are daily, raise timelapseend event at midnight */
            } else if (strcasecmp(cam->conf.timelapse_mode, "daily") == 0) {
                if (timestamp_tm.tm_hour == 0)
                    event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cam->current_image->imgts);

            /* handle the hourly case */
            } else if (strcasecmp(cam->conf.timelapse_mode, "hourly") == 0) {
                event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cam->current_image->imgts);

            /* If we are weekly-sunday, raise timelapseend event at midnight on sunday */
            } else if (strcasecmp(cam->conf.timelapse_mode, "weekly-sunday") == 0) {
                if (timestamp_tm.tm_wday == 0 &&
                    timestamp_tm.tm_hour == 0)
                    event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cam->current_image->imgts);
            /* If we are weekly-monday, raise timelapseend event at midnight on monday */
            } else if (strcasecmp(cam->conf.timelapse_mode, "weekly-monday") == 0) {
                if (timestamp_tm.tm_wday == 1 &&
                    timestamp_tm.tm_hour == 0)
                    event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cam->current_image->imgts);
            /* If we are monthly, raise timelapseend event at midnight on first day of month */
            } else if (strcasecmp(cam->conf.timelapse_mode, "monthly") == 0) {
                if (timestamp_tm.tm_mday == 1 &&
                    timestamp_tm.tm_hour == 0)
                    event(cam, EVENT_TIMELAPSEEND, NULL, NULL, NULL, &cam->current_image->imgts);
            /* If invalid we report in syslog once and continue in manual mode */
            } else {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Invalid timelapse_mode argument '%s'"), cam->conf.timelapse_mode);
                MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
                    ,_("%:s Defaulting to manual timelapse mode"));
                conf_cmdparse(&cam, (char *)"movie_timelapse_mode",(char *)"manual");
            }
        }

        if (cam->shots == 0 &&
            cam->frame_curr_ts.tv_sec % cam->conf.timelapse_interval <=
            cam->frame_last_ts.tv_sec % cam->conf.timelapse_interval) {
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

    cam->time_last_frame = cam->time_current_frame;


}

static void mlp_loopback(struct ctx_cam *cam){
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
    if (cam->conf.setup_mode) {

        event(cam, EVENT_IMAGE, &cam->imgs.image_motion, NULL, &cam->pipe, &cam->current_image->imgts);
        event(cam, EVENT_STREAM, &cam->imgs.image_motion, NULL, NULL, &cam->current_image->imgts);
    } else {
        event(cam, EVENT_IMAGE, cam->current_image, NULL,
              &cam->pipe, &cam->current_image->imgts);

        if (!cam->conf.stream_motion || cam->shots == 1)
            event(cam, EVENT_STREAM, cam->current_image, NULL, NULL,
                  &cam->current_image->imgts);
    }

    event(cam, EVENT_IMAGEM, &cam->imgs.image_motion, NULL, &cam->mpipe, &cam->current_image->imgts);

}

static void mlp_parmsupdate(struct ctx_cam *cam){
    /***** MOTION LOOP - ONCE PER SECOND PARAMETER UPDATE SECTION *****/

    /* Check for some config parameter changes but only every second */
    if (cam->shots != 0) return;

    init_text_scale(cam);  /* Initialize and validate text_scale */

    if (strcasecmp(cam->conf.picture_output, "on") == 0)
        cam->new_img = NEWIMG_ON;
    else if (strcasecmp(cam->conf.picture_output, "first") == 0)
        cam->new_img = NEWIMG_FIRST;
    else if (strcasecmp(cam->conf.picture_output, "best") == 0)
        cam->new_img = NEWIMG_BEST;
    else if (strcasecmp(cam->conf.picture_output, "center") == 0)
        cam->new_img = NEWIMG_CENTER;
    else
        cam->new_img = NEWIMG_OFF;

    if (strcasecmp(cam->conf.locate_motion_mode, "on") == 0)
        cam->locate_motion_mode = LOCATE_ON;
    else if (strcasecmp(cam->conf.locate_motion_mode, "preview") == 0)
        cam->locate_motion_mode = LOCATE_PREVIEW;
    else
        cam->locate_motion_mode = LOCATE_OFF;

    if (strcasecmp(cam->conf.locate_motion_style, "box") == 0)
        cam->locate_motion_style = LOCATE_BOX;
    else if (strcasecmp(cam->conf.locate_motion_style, "redbox") == 0)
        cam->locate_motion_style = LOCATE_REDBOX;
    else if (strcasecmp(cam->conf.locate_motion_style, "cross") == 0)
        cam->locate_motion_style = LOCATE_CROSS;
    else if (strcasecmp(cam->conf.locate_motion_style, "redcross") == 0)
        cam->locate_motion_style = LOCATE_REDCROSS;
    else
        cam->locate_motion_style = LOCATE_BOX;

    /* Sanity check for smart_mask_speed, silly value disables smart mask */
    if (cam->conf.smart_mask_speed < 0 || cam->conf.smart_mask_speed > 10)
        cam->conf.smart_mask_speed = 0;

    /* Has someone changed smart_mask_speed or framerate? */
    if (cam->conf.smart_mask_speed != cam->smartmask_speed ||
        cam->smartmask_lastrate != cam->lastrate) {
        if (cam->conf.smart_mask_speed == 0) {
            memset(cam->imgs.smartmask, 0, cam->imgs.motionsize);
            memset(cam->imgs.smartmask_final, 255, cam->imgs.motionsize);
        }

        cam->smartmask_lastrate = cam->lastrate;
        cam->smartmask_speed = cam->conf.smart_mask_speed;
        /*
            * Decay delay - based on smart_mask_speed (framerate independent)
            * This is always 5*smartmask_speed seconds
            */
        cam->smartmask_ratio = 5 * cam->lastrate * (11 - cam->smartmask_speed);
    }

    dbse_sqlmask_update(cam);

    cam->threshold = cam->conf.threshold;
    if (cam->conf.threshold_maximum > cam->conf.threshold ){
        cam->threshold_maximum = cam->conf.threshold_maximum;
    } else {
        cam->threshold_maximum = (cam->imgs.height * cam->imgs.width * 3) / 2;
    }

    if (!cam->conf.noise_tune){
        cam->noise = cam->conf.noise_level;
    }

}

static void mlp_frametiming(struct ctx_cam *cam){

    int indx;
    struct timespec ts2;
    int64_t avgtime;

    /* Shuffle the last wait times*/
    for (indx=0; indx<AVGCNT-1; indx++){
        cam->frame_wait[indx]=cam->frame_wait[indx+1];
    }

    if (cam->conf.framerate) {
        cam->frame_wait[AVGCNT-1] = 1000000L / cam->conf.framerate;
    } else {
        cam->frame_wait[AVGCNT-1] = 0;
    }

    clock_gettime(CLOCK_REALTIME, &ts2);

    cam->frame_wait[AVGCNT-1] = cam->frame_wait[AVGCNT-1] -
            (1000000L * (ts2.tv_sec - cam->frame_curr_ts.tv_sec)) -
            ((ts2.tv_nsec - cam->frame_curr_ts.tv_nsec)/1000);

    avgtime = 0;
    for (indx=0; indx<AVGCNT; indx++){
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

}

/**
 * motion_loop
 *
 *   Thread function for the motion handling threads.
 *
 */
static void *motion_loop(void *arg)
{
    struct ctx_cam *cam = arg;

    if (motion_init(cam) == 0){
        while (!cam->finish || cam->event_stop) {
            mlp_prepare(cam);
            if (cam->get_image) {
                mlp_resetimages(cam);
                if (mlp_retry(cam) == 1)  break;
                if (mlp_capture(cam) == 1)  break;
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

    motion_cleanup(cam);

    pthread_mutex_lock(&global_lock);
        threads_running--;
    pthread_mutex_unlock(&global_lock);

    cam->running = 0;
    cam->finish = 0;

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
 *   cam - current thread's context struct
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
    if (cam_list[0]->conf.pid_file) {
        pidf = myfopen(cam_list[0]->conf.pid_file, "w+");

        if (pidf) {
            (void)fprintf(pidf, "%d\n", getpid());
            myfclose(pidf);
        } else {
            MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO
                ,_("Exit motion, cannot create process"
                " id file (pid file) %s"), cam_list[0]->conf.pid_file);
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
            ,cam_list[0]->conf.pid_file, getpid());

    sigaction(SIGTTOU, &sig_ign_action, NULL);
    sigaction(SIGTTIN, &sig_ign_action, NULL);
    sigaction(SIGTSTP, &sig_ign_action, NULL);
}

static void camlist_create(int argc, char *argv[]){
    /*
     * cam_list is an array of pointers to the context structures cam for each thread.
     * First we reserve room for a pointer to thread 0's context structure
     * and a NULL pointer which indicates that end of the array of pointers to
     * thread context structures.
     */
    cam_list = mymalloc(sizeof(struct ctx_cam *) * 2);

    /* Now we reserve room for thread 0's context structure and let cam_list[0] point to it */
    cam_list[0] = mymalloc(sizeof(struct ctx_cam));

    /* Populate context structure with start/default values */
    context_init(cam_list[0]);

    /* Initialize some static and global string variables */
    gethostname (cam_list[0]->hostname, PATH_MAX);
    cam_list[0]->hostname[PATH_MAX-1] = '\0';
    /* end of variables */

    /* cam_list[1] pointing to zero indicates no more thread context structures - they get added later */
    cam_list[1] = NULL;

    /*
     * Command line arguments are being pointed to from cam_list[0] and we call conf_load which loads
     * the config options from motion.conf, thread config files and the command line.
     */
    cam_list[0]->conf.argv = argv;
    cam_list[0]->conf.argc = argc;
    cam_list = conf_load(cam_list);
}

static void motion_shutdown(void){
    int i = -1;

    motion_remove_pid();

    webu_stop(cam_list);

    while (cam_list[++i])
        context_destroy(cam_list[i]);

    free(cam_list);
    cam_list = NULL;

    vid_mutex_destroy();
}

static void motion_camera_ids(void){
    /* Set the camera id's on the context.  They must be unique */
    int indx, indx2, invalid_ids;

    /* Set defaults */
    indx = 0;
    while (cam_list[indx] != NULL){
        if (cam_list[indx]->conf.camera_id > 0){
            cam_list[indx]->camera_id = cam_list[indx]->conf.camera_id;
        } else {
            cam_list[indx]->camera_id = indx;
        }
        indx++;
    }

    invalid_ids = FALSE;
    indx = 0;
    while (cam_list[indx] != NULL){
        if (cam_list[indx]->camera_id > 32000) invalid_ids = TRUE;
        indx2 = indx + 1;
        while (cam_list[indx2] != NULL){
            if (cam_list[indx]->camera_id == cam_list[indx2]->camera_id) invalid_ids = TRUE;

            indx2++;
        }
        indx++;
    }
    if (invalid_ids){
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Camara IDs are not unique or have values over 32,000.  Falling back to thread numbers"));
        indx = 0;
        while (cam_list[indx] != NULL){
            cam_list[indx]->camera_id = indx;
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
    int indx;

    /* Initialize our global mutex */
    pthread_mutex_init(&global_lock, NULL);

    /*
     * Create the list of context structures and load the
     * configuration.
     */
    camlist_create(argc, argv);

    if ((cam_list[0]->conf.log_level > ALL) ||
        (cam_list[0]->conf.log_level == 0)) {
        cam_list[0]->conf.log_level = LEVEL_DEFAULT;
        cam_list[0]->log_level = cam_list[0]->conf.log_level;
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Using default log level (%s) (%d)")
            ,get_log_level_str(cam_list[0]->log_level)
            ,SHOW_LEVEL_VALUE(cam_list[0]->log_level));
    } else {
        cam_list[0]->log_level = cam_list[0]->conf.log_level - 1; // Let's make syslog compatible
    }


    if ((cam_list[0]->conf.log_file) && (strncmp(cam_list[0]->conf.log_file, "syslog", 6))) {
        set_log_mode(LOGMODE_FILE);
        ptr_logfile = set_logfile(cam_list[0]->conf.log_file);

        if (ptr_logfile) {
            set_log_mode(LOGMODE_SYSLOG);
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Logging to file (%s)"),cam_list[0]->conf.log_file);
            set_log_mode(LOGMODE_FILE);
        } else {
            MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO
                ,_("Exit motion, cannot create log file %s")
                ,cam_list[0]->conf.log_file);
            exit(0);
        }
    } else {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Logging to syslog"));
    }

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "Motion %s Started",VERSION);

    if ((cam_list[0]->conf.log_type == NULL) ||
        !(cam_list[0]->log_type = get_log_type(cam_list[0]->conf.log_type))) {
        cam_list[0]->log_type = TYPE_DEFAULT;
        cam_list[0]->conf.log_type = mystrcpy(cam_list[0]->conf.log_type, "ALL");
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Using default log type (%s)"),
                   get_log_type_str(cam_list[0]->log_type));
    }

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Using log type (%s) log level (%s)"),
               get_log_type_str(cam_list[0]->log_type), get_log_level_str(cam_list[0]->log_level));

    set_log_level(cam_list[0]->log_level);
    set_log_type(cam_list[0]->log_type);

    indx= 0;
    while (cam_list[indx] != NULL){
        cam_list[indx]->cam_list = cam_list;
        indx++;
    }

    if (daemonize) {
        /*
         * If daemon mode is requested, and we're not going into setup mode,
         * become daemon.
         */
        if (cam_list[0]->daemon && cam_list[0]->conf.setup_mode == 0) {
            become_daemon();
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motion running as daemon process"));
        }
    }

    if (cam_list[0]->conf.setup_mode)
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Motion running in setup mode."));

    conf_output_parms(cam_list);

    motion_ntc();

    motion_camera_ids();

    initialize_chars();

    webu_start(cam_list);

    vid_mutex_init();

}

/**
 * motion_start_thread
 *
 *   Called from main when start a motion thread
 *
 * Parameters:
 *
 *   cam - Thread context pointer
 *   thread_attr - pointer to thread attributes
 *
 * Returns: nothing
 */
static void motion_start_thread(struct ctx_cam *cam){
    int i;
    char service[6];
    pthread_attr_t thread_attr;

    if (strcmp(cam->conf_filename, "")){
        cam->conf_filename[sizeof(cam->conf_filename) - 1] = '\0';
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Camera ID: %d is from %s")
            ,cam->camera_id, cam->conf_filename);
    }

    if (cam->conf.netcam_url){
        snprintf(service,6,"%s",cam->conf.netcam_url);
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Camera ID: %d Camera Name: %s Service: %s")
            ,cam->camera_id, cam->conf.camera_name,service);
    } else {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Camera ID: %d Camera Name: %s Device: %s")
            ,cam->camera_id, cam->conf.camera_name,cam->conf.video_device);
    }

    /*
     * Check the stream port number for conflicts.
     * First we check for conflict with the control port.
     * Second we check for that two threads does not use the same port number
     * for the stream. If a duplicate port is found the stream feature gets disabled (port = 0)
     * for this thread and a warning is written to console and syslog.
     */

    if (cam->conf.stream_port != 0) {
        /* Compare against the control port. */
        if (cam_list[0]->conf.webcontrol_port == cam->conf.stream_port) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("Stream port number %d for thread %d conflicts with the control port")
                ,cam->conf.stream_port, cam->threadnr);
            MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
                ,_("Stream feature for thread %d is disabled.")
                ,cam->threadnr);
            cam->conf.stream_port = 0;
        }
        /* Compare against stream ports of other threads. */
        for (i = 1; cam_list[i]; i++) {
            if (cam_list[i] == cam) continue;

            if (cam_list[i]->conf.stream_port == cam->conf.stream_port) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Stream port number %d for thread %d conflicts with thread %d")
                    ,cam->conf.stream_port, cam->threadnr, cam_list[i]->threadnr);
                MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
                    ,_("Stream feature for thread %d is disabled.")
                    ,cam->threadnr);
                cam->conf.stream_port = 0;
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
    cam->restart = 1;

    /* Give the thread WATCHDOG_TMO to start */
    cam->watchdog = WATCHDOG_TMO;

    /* Flag it as running outside of the thread, otherwise if the main loop
     * checked if it is was running before the thread set it to 1, it would
     * start another thread for this device. */
    cam->running = 1;

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&cam->thread_id, &thread_attr, &motion_loop, cam)) {
        /* thread create failed, undo running state */
        cam->running = 0;
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

    if (!cam_list[indx]->running) return;

    cam_list[indx]->watchdog--;
    if (cam_list[indx]->watchdog == 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Thread %d - Watchdog timeout. Trying to do a graceful restart")
            , cam_list[indx]->threadnr);
        cam_list[indx]->event_stop = TRUE; /* Trigger end of event */
        cam_list[indx]->finish = 1;
    }

    if (cam_list[indx]->watchdog == WATCHDOG_KILL) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Thread %d - Watchdog timeout did NOT restart, killing it!")
            , cam_list[indx]->threadnr);
        if ((cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (cam_list[indx]->netcam != NULL)){
            pthread_cancel(cam_list[indx]->netcam->thread_id);
        }
        if ((cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (cam_list[indx]->netcam_high != NULL)){
            pthread_cancel(cam_list[indx]->netcam_high->thread_id);
        }
        pthread_cancel(cam_list[indx]->thread_id);
    }

    if (cam_list[indx]->watchdog < WATCHDOG_KILL) {
        if ((cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (cam_list[indx]->netcam != NULL)){
            if (!cam_list[indx]->netcam->handler_finished &&
                pthread_kill(cam_list[indx]->netcam->thread_id, 0) == ESRCH) {
                cam_list[indx]->netcam->handler_finished = TRUE;
                pthread_mutex_lock(&global_lock);
                    threads_running--;
                pthread_mutex_unlock(&global_lock);
                netcam_cleanup(cam_list[indx],FALSE);
            } else {
                pthread_kill(cam_list[indx]->netcam->thread_id, SIGVTALRM);
            }
        }
        if ((cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (cam_list[indx]->netcam_high != NULL)){
            if (!cam_list[indx]->netcam_high->handler_finished &&
                pthread_kill(cam_list[indx]->netcam_high->thread_id, 0) == ESRCH) {
                cam_list[indx]->netcam_high->handler_finished = TRUE;
                pthread_mutex_lock(&global_lock);
                    threads_running--;
                pthread_mutex_unlock(&global_lock);
                netcam_cleanup(cam_list[indx],FALSE);
            } else {
                pthread_kill(cam_list[indx]->netcam_high->thread_id, SIGVTALRM);
            }
        }
        if (cam_list[indx]->running &&
            pthread_kill(cam_list[indx]->thread_id, 0) == ESRCH){
            MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO
                ,_("Thread %d - Cleaning thread.")
                , cam_list[indx]->threadnr);
            pthread_mutex_lock(&global_lock);
                threads_running--;
            pthread_mutex_unlock(&global_lock);
            motion_cleanup(cam_list[indx]);
            cam_list[indx]->running = 0;
            cam_list[indx]->finish = 0;
        } else {
            pthread_kill(cam_list[indx]->thread_id,SIGVTALRM);
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

    for (indx = (cam_list[1] != NULL ? 1 : 0); cam_list[indx]; indx++) {
        if (cam_list[indx]->running || cam_list[indx]->restart)
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
    while (cam_list[indx] != NULL){
        if ((cam_list[indx]->webcontrol_finish == FALSE) &&
            ((cam_list[indx]->webcontrol_daemon != NULL) ||
             (cam_list[indx]->stream.daemon != NULL))) {
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

    movie_global_init();

    dbse_global_init(cam_list);

    translate_init();

    do {
        if (restart) motion_restart(argc, argv);

        for (i = cam_list[1] != NULL ? 1 : 0; cam_list[i]; i++) {
            cam_list[i]->threadnr = i ? i : 1;
            motion_start_thread(cam_list[i]);
        }

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Waiting for threads to finish, pid: %d"), getpid());

        while (1) {
            SLEEP(1, 0);
            if (motion_check_threadcount()) break;

            for (i = (cam_list[1] != NULL ? 1 : 0); cam_list[i]; i++) {
                /* Check if threads wants to be restarted */
                if ((!cam_list[i]->running) && (cam_list[i]->restart)) {
                    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                        ,_("Motion thread %d restart"), cam_list[i]->threadnr);
                    motion_start_thread(cam_list[i]);
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

    movie_global_deinit();

    dbse_global_deinit(cam_list);

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
 *   cam  - current thread's context structure (for logging)
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
 *   cam        - current thread's context structure.
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
static void mystrftime_long (const struct ctx_cam *cam,
                             int width, const char *word, int l, char *out)
{
#define SPECIFIERWORD(k) ((strlen(k)==l) && (!strncmp (k, word, l)))

    if (SPECIFIERWORD("host")) {
        snprintf (out, PATH_MAX, "%*s", width, cam->hostname);
        return;
    }
    if (SPECIFIERWORD("fps")) {
        sprintf(out, "%*d", width, cam->movie_fps);
        return;
    }
    if (SPECIFIERWORD("dbeventid")) {
        sprintf(out, "%*llu", width, cam->database_event_id);
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
 *   cam        - current thread's context structure
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
size_t mystrftime(const struct ctx_cam *cam, char *s, size_t max, const char *userformat,
                  const struct timespec *ts1, const char *filename, int sqltype)
{
    char formatstring[PATH_MAX] = "";
    char tempstring[PATH_MAX] = "";
    char *format, *tempstr;
    const char *pos_userformat;
    int width;
    struct tm timestamp_tm;

    localtime_r(&ts1->tv_sec, &timestamp_tm);

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
                sprintf(tempstr, "%0*d", width ? width : 2, cam->event_nr);
                break;

            case 'q': // shots
                sprintf(tempstr, "%0*d", width ? width : 2,
                    cam->current_image->shot);
                break;

            case 'D': // diffs
                sprintf(tempstr, "%*d", width, cam->current_image->diffs);
                break;

            case 'N': // noise
                sprintf(tempstr, "%*d", width, cam->noise);
                break;

            case 'i': // motion width
                sprintf(tempstr, "%*d", width,
                    cam->current_image->location.width);
                break;

            case 'J': // motion height
                sprintf(tempstr, "%*d", width,
                    cam->current_image->location.height);
                break;

            case 'K': // motion center x
                sprintf(tempstr, "%*d", width, cam->current_image->location.x);
                break;

            case 'L': // motion center y
                sprintf(tempstr, "%*d", width, cam->current_image->location.y);
                break;

            case 'o': // threshold
                sprintf(tempstr, "%*d", width, cam->threshold);
                break;

            case 'Q': // number of labels
                sprintf(tempstr, "%*d", width,
                    cam->current_image->total_labels);
                break;

            case 't': // camera id
                sprintf(tempstr, "%*d", width, cam->camera_id);
                break;

            case 'C': // text_event
                if (cam->text_event_string[0])
                    snprintf(tempstr, PATH_MAX, "%*s", width,
                        cam->text_event_string);
                else
                    ++pos_userformat;
                break;

            case 'w': // picture width
                sprintf(tempstr, "%*d", width, cam->imgs.width);
                break;

            case 'h': // picture height
                sprintf(tempstr, "%*d", width, cam->imgs.height);
                break;

            case 'f': // filename -- or %fps
                if ((*(pos_userformat+1) == 'p') && (*(pos_userformat+2) == 's')) {
                    sprintf(tempstr, "%*d", width, cam->movie_fps);
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
                    mystrftime_long (cam, width, word, (int)(pos_userformat-word), tempstr);
                    if (*pos_userformat == '\0') --pos_userformat;
                }
                break;

            case '$': // thread name
                if (cam->conf.camera_name && cam->conf.camera_name[0])
                    snprintf(tempstr, PATH_MAX, "%s", cam->conf.camera_name);
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
int util_check_passthrough(struct ctx_cam *cam){
    #if (LIBAVFORMAT_VERSION_MAJOR < 55)
        if (cam->movie_passthrough)
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("FFMPEG version too old. Disabling pass-through processing."));
        return 0;
    #else
        if (cam->movie_passthrough){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("pass-through is enabled but is still experimental."));
            return 1;
        } else {
            return 0;
        }
    #endif

}

/** Non case sensitive equality check for strings*/
int mystrceq(const char* var1, const char* var2){
    return (strcasecmp(var1,var2) ? 0 : 1);
}

/** Non case sensitive inequality check for strings*/
int mystrcne(const char* var1, const char* var2){
    return (strcasecmp(var1,var2) ? 1: 0);
}

/** Case sensitive equality check for strings*/
int mystreq(const char* var1, const char* var2){
    return (strcmp(var1,var2) ? 0 : 1);
}

/** Case sensitive inequality check for strings*/
int mystrne(const char* var1, const char* var2){
    return (strcmp(var1,var2) ? 1: 0);
}
