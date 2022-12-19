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
 *    Copyright 2020-2022 MotionMrDave@gmail.com
 *
*/
#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "motion_loop.hpp"
#include "dbse.hpp"
#include "webu.hpp"
#include "video_v4l2.hpp"
#include "movie.hpp"
#include "netcam.hpp"
#include "draw.hpp"

pthread_key_t tls_key_threadnr;
volatile enum MOTION_SIGNAL motsignal;

/** Process signals sent */
static void motion_signal_process(ctx_motapp *motapp)
{
    int indx;

    switch(motsignal){
    case MOTION_SIGNAL_ALARM:       /* Trigger snapshot */
        if (motapp->cam_list != NULL) {
            indx = 0;
            while (motapp->cam_list[indx] != NULL) {
                if (motapp->cam_list[indx]->conf->snapshot_interval) {
                    motapp->cam_list[indx]->snapshot = true;
                }
                indx++;
            }
        }
        break;
    case MOTION_SIGNAL_USR1:        /* Trigger the end of a event */
        if (motapp->cam_list != NULL) {
            indx = 0;
            while (motapp->cam_list[indx] != NULL){
                motapp->cam_list[indx]->event_stop = true;
                indx++;
            }
        }
        break;
    case MOTION_SIGNAL_SIGHUP:      /* Restart the threads */
        motapp->restart_all = true;
        /*FALLTHROUGH*/
    case MOTION_SIGNAL_SIGTERM:     /* Quit application */

        motapp->webcontrol_finish = true;

        if (motapp->cam_list != NULL) {
            indx = 0;
            while (motapp->cam_list[indx]) {
                motapp->cam_list[indx]->event_stop = true;
                motapp->cam_list[indx]->finish_cam = true;
                motapp->cam_list[indx]->restart_cam = false;
                indx++;
            }
        }
        motapp->finish_all = true;
    default:
        break;
    }
    motsignal = MOTION_SIGNAL_NONE;
}

/** Handle signals sent */
static void sig_handler(int signo)
{

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
        pthread_exit(NULL);
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

/** Attach handlers to a number of signals that MotionPlus need to catch. */
static void setup_signals(void)
{
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

/* Write out the pid file*/
static void motion_pid_write(ctx_motapp *motapp)
{
    FILE *pidf = NULL;

    if (motapp->pid_file != "") {
        pidf = myfopen(motapp->pid_file.c_str(), "w+e");
        if (pidf) {
            (void)fprintf(pidf, "%d\n", getpid());
            myfclose(pidf);
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Created process id file %s. Process ID is %d")
                ,motapp->pid_file.c_str(), getpid());
        } else {
            MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO
                , _("Cannot create process id file (pid file) %s")
                , motapp->pid_file.c_str());
        }
    }
}

/** Remove the process id file ( pid file ) before MotionPlus exit. */
static void motion_pid_remove(ctx_motapp *motapp)
{

    if ((motapp->pid_file != "") &&
        (motapp->restart_all == false)) {
        if (!unlink(motapp->pid_file.c_str())) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Removed process id file (pid file)."));
        } else{
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Error removing pid file"));
        }
    }

}

/**  Turn MotionPlus into a daemon through forking. */
static void motion_daemon(ctx_motapp *motapp)
{
    int fd;
    struct sigaction sig_ign_action;

    #ifdef SA_RESTART
        sig_ign_action.sa_flags = SA_RESTART;
    #else
        sig_ign_action.sa_flags = 0;
    #endif

    sig_ign_action.sa_handler = SIG_IGN;
    sigemptyset(&sig_ign_action.sa_mask);

    if (fork()) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("MotionPlus going to daemon mode"));
        exit(0);
    }

    /*
     * Changing dir to root enables people to unmount a disk
     * without having to stop MotionPlus
     */
    if (chdir("/")) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Could not change directory"));
    }

    #if (defined(BSD) && !defined(__APPLE__))
        setpgrp(0, getpid());
    #else
        setpgrp();
    #endif

    if ((fd = open("/dev/tty", O_RDWR|O_CLOEXEC)) >= 0) {
        ioctl(fd, TIOCNOTTY, NULL);
        close(fd);
    }

    setsid();

    fd = open("/dev/null", O_RDONLY|O_CLOEXEC);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    fd = open("/dev/null", O_WRONLY|O_CLOEXEC);
    if (fd != -1) {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
    }

    sigaction(SIGTTOU, &sig_ign_action, NULL);
    sigaction(SIGTTIN, &sig_ign_action, NULL);
    sigaction(SIGTSTP, &sig_ign_action, NULL);
}

static void motion_shutdown(ctx_motapp *motapp)
{
    motion_pid_remove(motapp);

    log_deinit(motapp);

    webu_deinit(motapp);

    dbse_deinit(motapp);

    conf_deinit(motapp);

}

static void motion_camera_ids(ctx_cam **cam_list)
{
    /* Set the camera id's on the ctx_cam.  They must be unique */
    int indx, indx2;
    int invalid_ids;

    /* Set defaults */
    indx = 0;
    while (cam_list[indx] != NULL){
        if (cam_list[indx]->conf->camera_id > 0) {
            cam_list[indx]->camera_id = cam_list[indx]->conf->camera_id;
        } else {
            cam_list[indx]->camera_id = indx;
        }
        indx++;
    }

    invalid_ids = false;
    indx = 0;
    while (cam_list[indx] != NULL){
        if (cam_list[indx]->camera_id > 32000) {
            invalid_ids = true;
        }
        indx2 = indx + 1;
        while (cam_list[indx2] != NULL){
            if (cam_list[indx]->camera_id == cam_list[indx2]->camera_id) {
                invalid_ids = true;
            }
            indx2++;
        }
        indx++;
    }
    if (invalid_ids) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Camara IDs are not unique or have values over 32,000.  Falling back to thread numbers"));
        indx = 0;
        while (cam_list[indx] != NULL){
            cam_list[indx]->camera_id = indx;
            indx++;
        }
    }
}

static void motion_ntc(void)
{

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

    #ifdef HAVE_LIBCAM
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("libcam : available"));
    #else
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,_("libcam : not available"));
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
static void motion_startup(ctx_motapp *motapp, int daemonize, int argc, char *argv[])
{

    log_set_motapp(motapp);  /* This is needed prior to any function possibly calling motion_log*/

    conf_init_app(motapp, argc, argv);

    log_init(motapp);

    conf_init_cams(motapp);

    mytranslate_init();

    mytranslate_text("",motapp->native_language);

    if (daemonize) {
        if (motapp->daemon && motapp->setup_mode == 0) {
            motion_daemon(motapp);
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("MotionPlus running as daemon process"));
        }
    }

    if (motapp->setup_mode) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("MotionPlus running in setup mode."));
    }

    conf_parms_log(motapp->cam_list);

    motion_ntc();

    motion_camera_ids(motapp->cam_list);

    dbse_init(motapp);

    draw_init_chars();

    webu_init(motapp);

}

/** Start a camera thread */
static void motion_start_thread(ctx_motapp *motapp, int indx)
{
    int retcd;

    pthread_attr_t thread_attr;

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

    motapp->cam_list[indx]->restart_cam = true;

    retcd = pthread_create(&motapp->cam_list[indx]->thread_id
                , &thread_attr, &motion_loop, motapp->cam_list[indx]);
    if (retcd != 0) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Unable to start thread for MotionPlus loop."));
    }

    pthread_attr_destroy(&thread_attr);

}

static void motion_restart(ctx_motapp *motapp, int argc, char **argv)
{

    MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Restarting MotionPlus."));

    motion_shutdown(motapp);

    SLEEP(2, 0);

    motion_startup(motapp, false, argc, argv);
    MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,_("MotionPlus restarted"));

    motapp->restart_all = false;

}

/* Check for whether any cams are locked */
static void motion_watchdog(ctx_motapp *motapp, int camindx)
{
    int indx;

    if (motapp->cam_list[camindx]->running_cam == false) {
        return;
    }

    motapp->cam_list[camindx]->watchdog--;
    if (motapp->cam_list[camindx]->watchdog > 0) {
        return;
    }

    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
        , _("Camera %d - Watchdog timeout.")
        , motapp->cam_list[camindx]->camera_id);

    /* Shut down all the cameras */
    indx = 1;
    while (motapp->cam_list[indx] != NULL) {
        pthread_mutex_unlock(&motapp->mutex_camlst);
        pthread_mutex_unlock(&motapp->mutex_parms);
        pthread_mutex_unlock(&motapp->mutex_camlst);
        pthread_mutex_unlock(&motapp->mutex_post);
        if (motapp->dbse != NULL) {
            pthread_mutex_unlock(&motapp->dbse->mutex_dbse);
        }
        pthread_mutex_unlock(&motapp->global_lock);
        pthread_mutex_unlock(&motapp->cam_list[indx]->stream.mutex);
        pthread_mutex_unlock(&motapp->cam_list[indx]->parms_lock);

        if ((motapp->cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (motapp->cam_list[indx]->netcam != NULL)) {
            pthread_mutex_unlock(&motapp->cam_list[indx]->netcam->mutex);
            pthread_mutex_unlock(&motapp->cam_list[indx]->netcam->mutex_pktarray);
            pthread_mutex_unlock(&motapp->cam_list[indx]->netcam->mutex_transfer);
            motapp->cam_list[indx]->netcam->finish = true;
        }
        if ((motapp->cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (motapp->cam_list[indx]->netcam_high != NULL)) {
            pthread_mutex_unlock(&motapp->cam_list[indx]->netcam_high->mutex);
            pthread_mutex_unlock(&motapp->cam_list[indx]->netcam_high->mutex_pktarray);
            pthread_mutex_unlock(&motapp->cam_list[indx]->netcam_high->mutex_transfer);
            motapp->cam_list[indx]->netcam_high->finish = true;
        }
        motapp->cam_list[indx]->event_stop = true;
        motapp->cam_list[indx]->finish_cam = true;
        indx++;
    }

    SLEEP(motapp->cam_list[camindx]->conf->watchdog_kill, 0);

    /* When in a watchdog timeout and we get to a kill situation,
     * we WILL have to leak memory because the freeing/deinit
     * processes could lock this thread which would stop everything.
    */
    indx = 1;
    while (motapp->cam_list[indx] != NULL) {
        if (motapp->cam_list[indx]->netcam != NULL) {
            if (motapp->cam_list[indx]->netcam->handler_finished == false) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    , _("Camera %d - Watchdog netcam kill.")
                    , motapp->cam_list[indx]->camera_id);
                pthread_kill(motapp->cam_list[indx]->netcam->thread_id, SIGVTALRM);
            }
        }
        if (motapp->cam_list[indx]->netcam_high != NULL) {
            if (motapp->cam_list[indx]->netcam_high->handler_finished == false) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    , _("Camera %d - Watchdog netcam_high kill.")
                    , motapp->cam_list[indx]->camera_id);
                pthread_kill(motapp->cam_list[indx]->netcam_high->thread_id, SIGVTALRM);
            }
        }
        if (motapp->cam_list[indx]->running_cam == true) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                , _("Camera %d - Watchdog kill.")
                , motapp->cam_list[indx]->camera_id);
            pthread_kill(motapp->cam_list[indx]->thread_id, SIGVTALRM);
        };
        motapp->cam_list[indx]->running_cam = false;
        motapp->cam_list[indx]->restart_cam = false;
        indx++;
    }
    motapp->restart_all = true;
    motapp->finish_all = true;
    motapp->webcontrol_finish = true;
    motapp->threads_running = 0;

}

static int motion_check_threadcount(ctx_motapp *motapp)
{
    int thrdcnt, indx;

    thrdcnt = 0;

    for (indx = (motapp->cam_list[1] != NULL ? 1 : 0); motapp->cam_list[indx]; indx++) {
        if (motapp->cam_list[indx]->running_cam || motapp->cam_list[indx]->restart_cam) {
            thrdcnt++;
        }
    }

    if ((motapp->webcontrol_finish == false) &&
        (motapp->webcontrol_daemon != NULL)) {
        thrdcnt++;
    }

    if (((thrdcnt == 0) && motapp->finish_all) ||
        ((thrdcnt == 0) && (motapp->threads_running == 0))) {
        MOTION_LOG(ALL, TYPE_ALL, NO_ERRNO
            ,_("DEBUG-1 threads_running %d thrdcnt %d , finish %d")
            ,motapp->threads_running, thrdcnt, motapp->finish_all);
        return 1;
    } else {
        return 0;
    }

}

static void motion_init(ctx_motapp *motapp)
{

    motapp->cam_list = NULL;
    pthread_mutex_init(&motapp->global_lock, NULL);
    pthread_mutex_init(&motapp->mutex_parms, NULL);
    pthread_mutex_init(&motapp->mutex_camlst, NULL);
    pthread_mutex_init(&motapp->mutex_post, NULL);

    motapp->threads_running = 0;
    motapp->finish_all = false;
    motapp->restart_all = false;

    motapp->argc = 0;
    motapp->argv = NULL;

    motapp->daemon = false;
    motapp->conf_filename="";
    motapp->pid_file="";
    motapp->log_file="";
    motapp->log_type_str="";
    motapp->log_level=0;
    motapp->log_type=0;
    motapp->setup_mode = false;
    motapp->pause = false;
    motapp->native_language = false;

    motapp->cam_add = false;
    motapp->cam_delete = 0;

    motapp->webcontrol_running = false;
    motapp->webcontrol_finish = false;
    motapp->webcontrol_daemon = NULL;
    memset(motapp->webcontrol_digest_rand, 0, sizeof(motapp->webcontrol_digest_rand));

    pthread_key_create(&tls_key_threadnr, NULL);
    pthread_setspecific(tls_key_threadnr, (void *)(0));

}

/* Check for whether to add a new cam */
static void motion_cam_add(ctx_motapp *motapp)
{
    int indx_cam, indx;

    if (motapp->cam_add == false) {
        return;
    }

    pthread_mutex_lock(&motapp->mutex_camlst);
        conf_camera_add(motapp);
    pthread_mutex_unlock(&motapp->mutex_camlst);

    indx_cam = 0;
    indx = 0;
    while (motapp->cam_list[indx_cam] != NULL) {
        if (indx < motapp->cam_list[indx_cam]->camera_id) {
            indx = motapp->cam_list[indx_cam]->camera_id;
        }
        indx_cam++;
    }
    indx++;
    indx_cam--;

    motapp->cam_list[indx_cam]->camera_id = indx;
    motapp->cam_list[indx_cam]->conf->camera_id = indx;
    motapp->cam_list[indx_cam]->conf->webcontrol_port = 0;

    motapp->cam_add = false;

}

/* Check for whether to delete a new cam */
static void motion_cam_delete(ctx_motapp *motapp)
{
    int indx_cam, indx;
    ctx_cam **tmp;

    if (motapp->cam_delete == 0) {
        return;
    }
    /* motapp->cam_delete contains the index of the cam to delete */
    indx_cam = 0;
    while (motapp->cam_list[indx_cam] != NULL) {
        indx_cam++;
    }

    if (motapp->cam_delete > indx_cam) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Invalid camera specified for deletion. %d"), motapp->cam_delete);
        return;
    }

    /* Delete the config context */
    delete motapp->cam_list[motapp->cam_delete]->conf;
    delete motapp->cam_list[motapp->cam_delete];
    motapp->cam_list[motapp->cam_delete] = NULL;

    /* Set up a new cam_list */
    tmp = (ctx_cam **)mymalloc(sizeof(ctx_cam *) * indx_cam);
    tmp[indx_cam-1] = NULL;

    /* Copy all the other cam pointers */
    indx_cam = 0;
    indx = 0;
    while (motapp->cam_list[indx_cam] != NULL) {
        if (indx_cam != motapp->cam_delete) {
            tmp[indx] = motapp->cam_list[indx_cam];
            indx++;
        }
        indx_cam++;
    }

    /* Swap out the old list with the new */
    pthread_mutex_lock(&motapp->mutex_camlst);
        myfree(&motapp->cam_list);
        motapp->cam_list = tmp;
    pthread_mutex_unlock(&motapp->mutex_camlst);

    /* Reset the delete flag */
    motapp->cam_delete = 0;

}


/** Main entry point of MotionPlus. */
int main (int argc, char **argv)
{
    int indx;
    ctx_motapp *motapp;

    motapp = new ctx_motapp;

    motion_init(motapp);

    setup_signals();

    motion_startup(motapp, true, argc, argv);

    movie_global_init();

    while (true) {

        if (motapp->restart_all) {
            motion_restart(motapp, argc, argv);
        }

        for (indx = motapp->cam_list[1] != NULL ? 1 : 0; motapp->cam_list[indx]; indx++) {
            motapp->cam_list[indx]->threadnr = indx ? indx : 1;
            motion_start_thread(motapp, indx);
        }

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Waiting for threads to finish, pid: %d"), getpid());

        while (true) {
            SLEEP(1, 0);

            if (motion_check_threadcount(motapp)) {
                break;
            }

            for (indx = (motapp->cam_list[1] != NULL ? 1 : 0); motapp->cam_list[indx]; indx++) {
                /* Check if threads wants to be restarted */
                if ((motapp->cam_list[indx]->running_cam == false) &&
                    (motapp->cam_list[indx]->restart_cam == true)) {
                    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                        ,_("MotionPlus camera %d restart")
                        , motapp->cam_list[indx]->camera_id);
                    motion_start_thread(motapp, indx);
                }
                motion_watchdog(motapp, indx);

            }

            if (motsignal != MOTION_SIGNAL_NONE) {
                motion_signal_process(motapp);
            }

            motion_cam_add(motapp);
            motion_cam_delete(motapp);

        }

        /* If there are no cameras running, this allows for adding */
        motion_cam_add(motapp);

        motapp->finish_all = false;

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Threads finished"));

        if (motapp->restart_all) {
            SLEEP(1, 0);    /* Rest before restarting */
        } else {
            break;
        }
    }

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("MotionPlus terminating"));

    movie_global_deinit();

    motion_shutdown(motapp);

    pthread_key_delete(tls_key_threadnr);
    pthread_mutex_destroy(&motapp->global_lock);
    pthread_mutex_destroy(&motapp->mutex_parms);
    pthread_mutex_destroy(&motapp->mutex_camlst);
    pthread_mutex_destroy(&motapp->mutex_post);

    delete motapp;

    return 0;
}

