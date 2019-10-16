/*    motion.cpp
 *
 *    Detect changes in a video stream.
 *    Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */
#include "motion.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "motion_loop.hpp"
#include "dbse.hpp"
#include "webu.hpp"
#include "video_common.hpp"
#include "movie.hpp"
#include "netcam.hpp"
#include "draw.hpp"

pthread_key_t tls_key_threadnr;
volatile enum MOTION_SIGNAL motsignal;

/** Process signals sent */
static void motion_signal_process(struct ctx_motapp *motapp){
    int indx;

    switch(motsignal){
    case MOTION_SIGNAL_ALARM:       /* Trigger snapshot */
        if (motapp->cam_list != NULL) {
            indx = 0;
            while (motapp->cam_list[indx] != NULL) {
                if (motapp->cam_list[indx]->conf.snapshot_interval){
                    motapp->cam_list[indx]->snapshot = TRUE;
                }
                indx++;
            }
        }
        break;
    case MOTION_SIGNAL_USR1:        /* Trigger the end of a event */
        if (motapp->cam_list != NULL) {
            indx = 0;
            while (motapp->cam_list[indx] != NULL){
                motapp->cam_list[indx]->event_stop = TRUE;
                indx++;
            }
        }
        break;
    case MOTION_SIGNAL_SIGHUP:      /* Restart the threads */
        motapp->restart_all = TRUE;
        /*FALLTHROUGH*/
    case MOTION_SIGNAL_SIGTERM:     /* Quit application */
        if (motapp->cam_list != NULL) {
            indx = 0;
            while (motapp->cam_list[indx]) {
                motapp->webcontrol_finish = TRUE;
                motapp->cam_list[indx]->event_stop = TRUE;
                motapp->cam_list[indx]->finish_cam = TRUE;
                motapp->cam_list[indx]->restart_cam = FALSE;
                indx++;
            }
        }
        motapp->finish_all = TRUE;
    default:
        break;
    }
    motsignal = MOTION_SIGNAL_NONE;
}

/** Handle signals sent */
static void sig_handler(int signo) {

    /*The FALLTHROUGH is a special comment required by compiler.  Do not edit it*/
    switch(signo) {
    case SIGALRM:
        motsignal = MOTION_SIGNAL_ALARM;
        break;
    case SIGUSR1:
        motsignal = MOTION_SIGNAL_USR1;
        break;
    case SIGHUP:
        motsignal = MOTION_SIGNAL_SIGHUP;
        break;
    case SIGINT:
        /*FALLTHROUGH*/
    case SIGQUIT:
        /*FALLTHROUGH*/
    case SIGTERM:
        motsignal = MOTION_SIGNAL_SIGTERM;
        break;
    case SIGSEGV:
        exit(0);
    case SIGVTALRM:
        printf("SIGVTALRM went off\n");
        break;
    }
}

/**  POSIX compliant replacement of the signal(SIGCHLD, SIG_IGN). */
static void sigchild_handler(int signo)
{
    (void)signo;

    #ifdef WNOHANG
        while (waitpid(-1, NULL, WNOHANG) > 0) {};
    #endif /* WNOHANG */

    return;
}

/** Attach handlers to a number of signals that Motion need to catch. */
static void setup_signals(void){
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

/** Remove the process id file ( pid file ) before motion exit. */
static void motion_remove_pid(struct ctx_motapp *motapp) {

    if ((motapp->daemon) &&
        (motapp->pid_file) &&
        (motapp->restart_all == FALSE)) {
        if (!unlink(motapp->pid_file)){
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Removed process id file (pid file)."));
        } else{
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Error removing pid file"));
        }
    }

}

/**  Turn Motion into a daemon through forking. */
static void motion_daemon(struct ctx_motapp *motapp) {
    int fd;
    FILE *pidf = NULL;
    struct sigaction sig_ign_action;

    #ifdef SA_RESTART
        sig_ign_action.sa_flags = SA_RESTART;
    #else
        sig_ign_action.sa_flags = 0;
    #endif

    sig_ign_action.sa_handler = SIG_IGN;
    sigemptyset(&sig_ign_action.sa_mask);

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
    if (motapp->pid_file[0]) {
        pidf = myfopen(motapp->pid_file, "w+");

        if (pidf) {
            (void)fprintf(pidf, "%d\n", getpid());
            myfclose(pidf);
        } else {
            MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO
                ,_("Exit motion, cannot create process"
                " id file (pid file) %s"),motapp->pid_file);
            log_deinit(motapp);
            exit(0);
        }
    }

    /*
     * Changing dir to root enables people to unmount a disk
     * without having to stop Motion
     */
    if (chdir("/")){
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Could not change directory"));
    }

    #if (defined(BSD) && !defined(__APPLE__))
        setpgrp(0, getpid());
    #else
        setpgrp();
    #endif

    if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
        ioctl(fd, TIOCNOTTY, NULL);
        close(fd);
    }

    setsid();

    fd = open("/dev/null", O_RDONLY);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    fd = open("/dev/null", O_WRONLY);
    if (fd != -1) {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
    }

    /* Now it is safe to add the PID creation to the logs */
    if (pidf){
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Created process id file %s. Process ID is %d")
            ,motapp->pid_file, getpid());
    }

    sigaction(SIGTTOU, &sig_ign_action, NULL);
    sigaction(SIGTTIN, &sig_ign_action, NULL);
    sigaction(SIGTSTP, &sig_ign_action, NULL);
}

static void motion_shutdown(struct ctx_motapp *motapp){

    motion_remove_pid(motapp);

    log_deinit(motapp);

    webu_deinit(motapp);

    dbse_global_deinit(motapp->cam_list);

    conf_deinit(motapp);

    vid_mutex_destroy();
}

static void motion_camera_ids(struct ctx_cam **cam_list){
    /* Set the camera id's on the ctx_cam.  They must be unique */
    int indx, indx2;
    int invalid_ids;

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

    #ifdef ENABLE_NLS
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("nls    : available"));
    #else
        MOTION_LOG(DBG, TYPE_DB, NO_ERRNO,_("nls    : not available"));
    #endif

}

/** Initialize upon start up or restart */
static void motion_startup(struct ctx_motapp *motapp, int daemonize, int argc, char *argv[]) {

    conf_init_app(motapp, argc, argv);

    log_init(motapp);

    conf_init_cams(motapp);

    mytranslate_init();

    mytranslate_text("",motapp->native_language);

    if (daemonize) {
        if (motapp->daemon && motapp->setup_mode == 0) {
            motion_daemon(motapp);
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motion running as daemon process"));
        }
    }

    if (motapp->setup_mode){
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Motion running in setup mode."));
    }

    conf_parms_log(motapp->cam_list);

    motion_ntc();

    motion_camera_ids(motapp->cam_list);

    dbse_global_init(motapp->cam_list);

    draw_init_chars();

    webu_init(motapp);

    vid_mutex_init();

}

/** Start a camera thread */
static void motion_start_thread(struct ctx_motapp *motapp, int indx){
    pthread_attr_t thread_attr;

    pthread_mutex_lock(&motapp->global_lock);
        motapp->threads_running++;
    pthread_mutex_unlock(&motapp->global_lock);

    motapp->cam_list[indx]->restart_cam = TRUE;
    motapp->cam_list[indx]->watchdog = WATCHDOG_TMO;
    motapp->cam_list[indx]->running_cam = TRUE;

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&motapp->cam_list[indx]->thread_id
            , &thread_attr, &motion_loop, motapp->cam_list[indx])) {
        /* thread create failed, undo running state */
        motapp->cam_list[indx]->running_cam = FALSE;
        pthread_mutex_lock(&motapp->global_lock);
            motapp->threads_running--;
        pthread_mutex_unlock(&motapp->global_lock);
    }
    pthread_attr_destroy(&thread_attr);

}

static void motion_restart(struct ctx_motapp *motapp, int argc, char **argv){

    MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Restarting motion."));

    motion_shutdown(motapp);

    SLEEP(2, 0);

    motion_startup(motapp, FALSE, argc, argv);
    MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Motion restarted"));

    motapp->restart_all = FALSE;

}

static void motion_watchdog(struct ctx_motapp *motapp, int indx){

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

    if (!motapp->cam_list[indx]->running_cam) return;

    motapp->cam_list[indx]->watchdog--;
    if (motapp->cam_list[indx]->watchdog == 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Thread %d - Watchdog timeout. Trying to do a graceful restart")
            , motapp->cam_list[indx]->threadnr);
        motapp->cam_list[indx]->event_stop = TRUE; /* Trigger end of event */
        motapp->cam_list[indx]->finish_cam = TRUE;
    }

    if (motapp->cam_list[indx]->watchdog == WATCHDOG_KILL) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Thread %d - Watchdog timeout did NOT restart, killing it!")
            , motapp->cam_list[indx]->threadnr);
        if ((motapp->cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (motapp->cam_list[indx]->netcam != NULL)){
            pthread_cancel(motapp->cam_list[indx]->netcam->thread_id);
        }
        if ((motapp->cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (motapp->cam_list[indx]->netcam_high != NULL)){
            pthread_cancel(motapp->cam_list[indx]->netcam_high->thread_id);
        }
        pthread_cancel(motapp->cam_list[indx]->thread_id);
    }

    if (motapp->cam_list[indx]->watchdog < WATCHDOG_KILL) {
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO
            ,_("Thread %d - Watchdog kill!")
            , motapp->cam_list[indx]->threadnr);

        if ((motapp->cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (motapp->cam_list[indx]->netcam != NULL)){
            if (!motapp->cam_list[indx]->netcam->handler_finished &&
                pthread_kill(motapp->cam_list[indx]->netcam->thread_id, 0) == ESRCH) {
                motapp->cam_list[indx]->netcam->handler_finished = TRUE;
                pthread_mutex_lock(&motapp->global_lock);
                    motapp->threads_running--;
                pthread_mutex_unlock(&motapp->global_lock);
                netcam_cleanup(motapp->cam_list[indx],FALSE);
            } else {
                pthread_kill(motapp->cam_list[indx]->netcam->thread_id, SIGVTALRM);
            }
        }
        if ((motapp->cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (motapp->cam_list[indx]->netcam_high != NULL)){
            if (!motapp->cam_list[indx]->netcam_high->handler_finished &&
                pthread_kill(motapp->cam_list[indx]->netcam_high->thread_id, 0) == ESRCH) {
                motapp->cam_list[indx]->netcam_high->handler_finished = TRUE;
                pthread_mutex_lock(&motapp->global_lock);
                    motapp->threads_running--;
                pthread_mutex_unlock(&motapp->global_lock);
                netcam_cleanup(motapp->cam_list[indx], FALSE);
            } else {
                pthread_kill(motapp->cam_list[indx]->netcam_high->thread_id, SIGVTALRM);
            }
        }
        if (motapp->cam_list[indx]->running_cam &&
            pthread_kill(motapp->cam_list[indx]->thread_id, 0) == ESRCH){
            MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO
                ,_("Thread %d - Cleaning thread.")
                , motapp->cam_list[indx]->threadnr);
            pthread_mutex_lock(&motapp->global_lock);
                motapp->threads_running--;
            pthread_mutex_unlock(&motapp->global_lock);
            mlp_cleanup(motapp->cam_list[indx]);
            motapp->cam_list[indx]->running_cam = FALSE;
            motapp->cam_list[indx]->finish_cam = FALSE;
        } else {
            pthread_kill(motapp->cam_list[indx]->thread_id,SIGVTALRM);
        }
    }

}

static int motion_check_threadcount(struct ctx_motapp *motapp){
    /* Return 1 if we should break out of loop */

    /* It has been observed that this is not counting every
     * thread running.  The netcams spawn handler threads which are not
     * counted here.  This is only counting ctx_cam threads and when they
     * all get to zero, then we are done.
     */

    int thrdcnt, indx;

    thrdcnt = 0;

    for (indx = (motapp->cam_list[1] != NULL ? 1 : 0); motapp->cam_list[indx]; indx++) {
        if (motapp->cam_list[indx]->running_cam || motapp->cam_list[indx]->restart_cam){
            thrdcnt++;
        }
    }

    /* If the web control/streams are in finish/shutdown, we
     * do not want to count them.  They will be completely closed
     * by the process outside of loop that is checking the counts
     * of threads.  If the webcontrol is not in a finish / shutdown
     * then we want to keep them in the tread count to allow user
     * to restart the cameras and keep Motion running.
     */
    indx = 0;
    while (motapp->cam_list[indx] != NULL){
        if ((motapp->webcontrol_finish == FALSE) &&
            ((motapp->webcontrol_daemon != NULL) ||
             (motapp->cam_list[indx]->stream.daemon != NULL))) {
            thrdcnt++;
        }
        indx++;
    }

    if (((thrdcnt == 0) && motapp->finish_all) ||
        ((thrdcnt == 0) && (motapp->threads_running == 0))) {
        MOTION_LOG(ALL, TYPE_ALL, NO_ERRNO
            ,_("DEBUG-1 threads_running %d motion_threads_running %d , finish %d")
            ,motapp->threads_running, thrdcnt, motapp->finish_all);
        return 1;
    } else {
        return 0;
    }

}

/** Main entry point of Motion. */
int main (int argc, char **argv) {

    int indx;
    struct ctx_motapp *motapp;

    motapp = (struct ctx_motapp*)mymalloc(sizeof(struct ctx_motapp));

    pthread_mutex_init(&motapp->global_lock, NULL);
    pthread_key_create(&tls_key_threadnr, NULL);
    pthread_setspecific(tls_key_threadnr, (void *)(0));

    setup_signals();

    motion_startup(motapp, TRUE, argc, argv);

    movie_global_init();

    do {
        if (motapp->restart_all) motion_restart(motapp, argc, argv);

        for (indx = motapp->cam_list[1] != NULL ? 1 : 0; motapp->cam_list[indx]; indx++) {
            motapp->cam_list[indx]->threadnr = indx ? indx : 1;
            motion_start_thread(motapp, indx);
        }

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Waiting for threads to finish, pid: %d"), getpid());

        while (1) {
            SLEEP(1, 0);

            if (motion_check_threadcount(motapp)) break;

            for (indx = (motapp->cam_list[1] != NULL ? 1 : 0); motapp->cam_list[indx]; indx++) {
                /* Check if threads wants to be restarted */
                if ((!motapp->cam_list[indx]->running_cam) &&
                    (motapp->cam_list[indx]->restart_cam)) {
                    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                        ,_("Motion thread %d restart"), motapp->cam_list[indx]->threadnr);
                    motion_start_thread(motapp, indx);
                }
                motion_watchdog(motapp, indx);

            }

            if (motsignal != MOTION_SIGNAL_NONE) motion_signal_process(motapp);

        }

        /* Reset end main loop flag */
        motapp->finish_all = FALSE;

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Threads finished"));

        /* Rest for a while if we're supposed to restart. */
        if (motapp->restart_all) SLEEP(1, 0);

    } while (motapp->restart_all); /* loop if we're supposed to restart */


    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motion terminating"));

    movie_global_deinit();

    motion_shutdown(motapp);

    pthread_key_delete(tls_key_threadnr);
    pthread_mutex_destroy(&motapp->global_lock);

    free(motapp);

    return 0;
}

