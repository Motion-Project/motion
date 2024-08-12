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
 *
*/
#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "motion_loop.hpp"
#include "sound.hpp"
#include "dbse.hpp"
#include "webu.hpp"
#include "video_v4l2.hpp"
#include "movie.hpp"
#include "netcam.hpp"

pthread_key_t tls_key_threadnr;
volatile enum MOTPLS_SIGNAL motsignal;

/** Process signals sent */
static void motpls_signal_process(ctx_motapp *motapp)
{
    uint indx;

    switch(motsignal){
    case MOTPLS_SIGNAL_ALARM:       /* Trigger snapshot */
        if (motapp->cam_list != NULL) {
            for (indx=0; indx<(uint)motapp->cam_cnt; indx++) {
                if (motapp->cam_list[indx]->conf->snapshot_interval) {
                    motapp->cam_list[indx]->snapshot = true;
                }
            }
        }
        break;
    case MOTPLS_SIGNAL_USR1:        /* Trigger the end of a event */
        if (motapp->cam_list != NULL) {
            for (indx=0; indx<(uint)motapp->cam_cnt; indx++) {
                motapp->cam_list[indx]->event_stop = true;
            }
        }
        break;
    case MOTPLS_SIGNAL_SIGHUP:      /* Reload the parameters and restart*/
        motapp->reload_all = true;
        motapp->webu->wb_finish = true;
        for (indx=0; indx<(uint)motapp->cam_cnt; indx++) {
            motapp->cam_list[indx]->event_stop = true;
            motapp->cam_list[indx]->finish_dev = true;
            motapp->cam_list[indx]->restart_dev = false;
        }
        for (indx=0; indx<motapp->snd_list.size(); indx++) {
            motapp->snd_list[indx]->handler_stop = true;
        }
        break;
    case MOTPLS_SIGNAL_SIGTERM:     /* Quit application */

        motapp->webu->wb_finish = true;
        for (indx=0; indx<(uint)motapp->cam_cnt; indx++) {
            motapp->cam_list[indx]->event_stop = true;
            motapp->cam_list[indx]->finish_dev = true;
            motapp->cam_list[indx]->restart_dev = false;
        }
        for (indx=0; indx<motapp->snd_list.size(); indx++) {
            motapp->snd_list[indx]->handler_stop = true;
        }
        motapp->finish_all = true;
    default:
        break;
    }
    motsignal = MOTPLS_SIGNAL_NONE;
}

/** Handle signals sent */
static void sig_handler(int signo)
{

    /*The FALLTHROUGH is a special comment required by compiler.  Do not edit it*/
    switch(signo) {
    case SIGALRM:
        motsignal = MOTPLS_SIGNAL_ALARM;
        break;
    case SIGUSR1:
        motsignal = MOTPLS_SIGNAL_USR1;
        break;
    case SIGHUP:
        motsignal = MOTPLS_SIGNAL_SIGHUP;
        break;
    case SIGINT:
        /*FALLTHROUGH*/
    case SIGQUIT:
        /*FALLTHROUGH*/
    case SIGTERM:
        motsignal = MOTPLS_SIGNAL_SIGTERM;
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
static void motpls_pid_write(ctx_motapp *motapp)
{
    FILE *pidf = NULL;

    if (motapp->conf->pid_file != "") {
        pidf = myfopen(motapp->conf->pid_file.c_str(), "w+e");
        if (pidf) {
            (void)fprintf(pidf, "%d\n", getpid());
            myfclose(pidf);
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Created process id file %s. Process ID is %d")
                ,motapp->conf->pid_file.c_str(), getpid());
        } else {
            MOTPLS_LOG(EMG, TYPE_ALL, SHOW_ERRNO
                , _("Cannot create process id file (pid file) %s")
                , motapp->conf->pid_file.c_str());
        }
    }
}

/** Remove the process id file ( pid file ) before MotionPlus exit. */
static void motpls_pid_remove(ctx_motapp *motapp)
{

    if ((motapp->conf->pid_file != "") &&
        (motapp->restart_all == false)) {
        if (!unlink(motapp->conf->pid_file.c_str())) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Removed process id file (pid file)."));
        } else{
            MOTPLS_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Error removing pid file"));
        }
    }

}

/**  Turn MotionPlus into a daemon through forking. */
static void motpls_daemon()
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
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("MotionPlus going to daemon mode"));
        exit(0);
    }

    /*
     * Changing dir to root enables people to unmount a disk
     * without having to stop MotionPlus
     */
    if (chdir("/")) {
        MOTPLS_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Could not change directory"));
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

void motpls_av_log(void *ignoreme, int errno_flag, const char *fmt, va_list vl)
{
    char buf[1024];
    char *end;
    int retcd;

    (void)ignoreme;

    /* Valgrind occasionally reports use of uninitialized values in here when we interrupt
     * some rtsp functions.  The offending value is either fmt or vl and seems to be from a
     * debug level of av functions.  To address it we flatten the message after we know
     * the log level.  Now we put the avcodec messages to INF level since their error
     * are not necessarily our errors.
     */

    if (errno_flag <= AV_LOG_WARNING) {
        retcd = vsnprintf(buf, sizeof(buf), fmt, vl);
        if (retcd >=1024) {
            MOTPLS_LOG(DBG, TYPE_ENCODER, NO_ERRNO, "av message truncated %d bytes",(retcd - 1024));
        }
        end = buf + strlen(buf);
        if (end > buf && end[-1] == '\n') {
            *--end = 0;
        }
        if (strstr(buf, "Will reconnect at") == NULL) {
            MOTPLS_LOG(INF, TYPE_ENCODER, NO_ERRNO, "%s", buf);
        }
    }

}

void motpls_av_init(void)
{
    MOTPLS_LOG(NTC, TYPE_ENCODER, NO_ERRNO, _("libavcodec  version %d.%d.%d")
        , LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO);
    MOTPLS_LOG(NTC, TYPE_ENCODER, NO_ERRNO, _("libavformat version %d.%d.%d")
        , LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO);

    #if (MYFFVER < 58000)
        av_register_all();
        avcodec_register_all();
    #endif

    avformat_network_init();
    avdevice_register_all();
    av_log_set_callback(motpls_av_log);

}

void motpls_av_deinit(void)
{
    avformat_network_deinit();
}

/* Validate or set the position on the all cameras image*/
static void motpls_allcams_init(ctx_motapp *motapp)
{
    int indx, indx1, row, col, mx_row, mx_col, col_chk;
    bool cfg_valid, chk;
    std::string cfg_row, cfg_col;
    ctx_params  *params_loc;
    p_lst *lst;
    p_it it;

    motapp->all_sizes = new ctx_all_sizes;
    motapp->all_sizes->height = 0;
    motapp->all_sizes->width = 0;
    motapp->all_sizes->img_sz = 0;
    motapp->all_sizes->reset = true;

    if (motapp->cam_cnt < 1) {
        return;
    }

    params_loc = new ctx_params;

    for (indx=0; indx<motapp->cam_cnt; indx++) {
        motapp->cam_list[indx]->all_loc.row = -1;
        motapp->cam_list[indx]->all_loc.col = -1;
        motapp->cam_list[indx]->all_loc.offset_user_col = 0;
        motapp->cam_list[indx]->all_loc.offset_user_row = 0;
        motapp->cam_list[indx]->all_loc.scale =
            motapp->cam_list[indx]->conf->stream_preview_scale;

        params_loc->update_params = true;
        util_parms_parse(params_loc
            , "stream_preview_location"
            , motapp->cam_list[indx]->conf->stream_preview_location);
        lst = &params_loc->params_array;

        for (it = lst->begin(); it != lst->end(); it++) {
            if (it->param_name == "row") {
                motapp->cam_list[indx]->all_loc.row = mtoi(it->param_value);
            }
            if (it->param_name == "col") {
                motapp->cam_list[indx]->all_loc.col = mtoi(it->param_value);
            }
            if (it->param_name == "offset_col") {
                motapp->cam_list[indx]->all_loc.offset_user_col =
                    mtoi(it->param_value);
            }
            if (it->param_name == "offset_row") {
                motapp->cam_list[indx]->all_loc.offset_user_row =
                    mtoi(it->param_value);
            }
        }
        params_loc->params_array.clear();
    }

    delete params_loc;

    mx_row = 0;
    mx_col = 0;
    for (indx=0; indx<motapp->cam_cnt; indx++) {
        if (mx_col < motapp->cam_list[indx]->all_loc.col) {
            mx_col = motapp->cam_list[indx]->all_loc.col;
        }
        if (mx_row < motapp->cam_list[indx]->all_loc.row) {
            mx_row = motapp->cam_list[indx]->all_loc.row;
        }
    }
    cfg_valid = true;
    for (indx=0; indx<motapp->cam_cnt; indx++) {
        if ((motapp->cam_list[indx]->all_loc.col == -1) ||
            (motapp->cam_list[indx]->all_loc.row == -1)) {
            cfg_valid = false;
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "No stream_preview_location for cam %d"
                , motapp->cam_list[indx]->conf->device_id);
        } else {
            for (indx1=0; indx1<motapp->cam_cnt; indx1++) {
                if ((motapp->cam_list[indx]->all_loc.col ==
                    motapp->cam_list[indx1]->all_loc.col) &&
                    (motapp->cam_list[indx]->all_loc.row ==
                    motapp->cam_list[indx1]->all_loc.row) &&
                    (indx != indx1)) {
                    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                        , "Duplicate stream_preview_location "
                        " cam %d, cam %d row %d col %d"
                        , motapp->cam_list[indx]->conf->device_id
                        , motapp->cam_list[indx1]->conf->device_id
                        , motapp->cam_list[indx]->all_loc.row
                        , motapp->cam_list[indx]->all_loc.col);
                    cfg_valid = false;
                }
            }
        }
        if (motapp->cam_list[indx]->all_loc.row == 0) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "Invalid stream_preview_location row cam %d, row %d"
                , motapp->cam_list[indx]->conf->device_id
                , motapp->cam_list[indx]->all_loc.row);
            cfg_valid = false;
        }
        if (motapp->cam_list[indx]->all_loc.col == 0) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "Invalid stream_preview_location col cam %d, col %d"
                , motapp->cam_list[indx]->conf->device_id
                , motapp->cam_list[indx]->all_loc.col);
            cfg_valid = false;
        }
    }

    for (row=1; row<=mx_row; row++) {
        chk = false;
        for (indx=0; indx<motapp->cam_cnt; indx++) {
            if (row == motapp->cam_list[indx]->all_loc.row) {
                chk = true;
            }
        }
        if (chk == false) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "Invalid stream_preview_location combination. "
                " Missing row %d", row);
            cfg_valid = false;
        }
        col_chk = 0;
        for (col=1; col<=mx_col; col++) {
            for (indx=0; indx<motapp->cam_cnt; indx++) {
                if ((row == motapp->cam_list[indx]->all_loc.row) &&
                    (col == motapp->cam_list[indx]->all_loc.col)) {
                    if ((col_chk+1) == col) {
                        col_chk = col;
                    } else {
                        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                            , "Invalid stream_preview_location combination. "
                            " Missing row %d column %d", row, col_chk+1);
                        cfg_valid = false;
                    }
                }
            }
        }
    }

    if (cfg_valid == false) {
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,"Creating default stream preview values");
        row = 0;
        col = 0;
        for (indx=0; indx<motapp->cam_cnt; indx++) {
            if (col == 1) {
                col++;
            } else {
                row++;
                col = 1;
            }
            motapp->cam_list[indx]->all_loc.col = col;
            motapp->cam_list[indx]->all_loc.row = row;
            motapp->cam_list[indx]->all_loc.scale = -1;
        }
    }

    for (indx=0; indx<motapp->cam_cnt; indx++) {
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO
            ,"stream_preview_location values. Device %d row %d col %d"
            , motapp->cam_list[indx]->device_id
            , motapp->cam_list[indx]->all_loc.row
            , motapp->cam_list[indx]->all_loc.col);
    }

}

static void motpls_shutdown(ctx_motapp *motapp)
{
    motpls_pid_remove(motapp);

    mydelete(motapp->webu);
    mydelete(motapp->dbse);
    mydelete(motapp->conf);
    mydelete(motapp->all_sizes);
    mydelete(motlog);

}

static void motpls_device_ids(ctx_motapp *motapp)
{
    uint indx, indx2, sndmx, cammx;
    int invalid_ids;

    sndmx = (uint)motapp->snd_list.size();
    cammx = (uint)motapp->cam_cnt;

    /* Defaults */
    for (indx=0; indx<cammx; indx++) {
        if (motapp->cam_list[indx]->conf->device_id != 0) {
            motapp->cam_list[indx]->device_id = motapp->cam_list[indx]->conf->device_id;
        } else {
            motapp->cam_list[indx]->device_id = (int)indx + 1;
        }
    }
    for (indx=0; indx<sndmx; indx++) {
        if (motapp->snd_list[indx]->conf->device_id != 0) {
            motapp->snd_list[indx]->device_id = motapp->snd_list[indx]->conf->device_id;
        } else {
            motapp->snd_list[indx]->device_id = motapp->cam_cnt + (int)indx + 1;
        }
    }

    /*Check for unique values*/
    invalid_ids = false;
    for (indx=0; indx<cammx; indx++) {
        for (indx2=indx+1; indx2<cammx; indx2++) {
           if (motapp->cam_list[indx]->device_id == motapp->cam_list[indx2]->device_id) {
                invalid_ids = true;
            }
        }
        for (indx2=0; indx2<sndmx; indx2++) {
           if (motapp->cam_list[indx]->device_id == motapp->snd_list[indx2]->device_id) {
                invalid_ids = true;
            }
        }
    }
    for (indx=0; indx<sndmx; indx++) {
        for (indx2=indx+1; indx2<sndmx; indx2++) {
           if (motapp->snd_list[indx]->device_id == motapp->snd_list[indx2]->device_id) {
                invalid_ids = true;
            }
        }
    }

    if (invalid_ids) {
        MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Device IDs are not unique."));
        MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Falling back to sequence numbers"));
        for (indx=0; indx<cammx; indx++) {
            motapp->cam_list[indx]->device_id = (int)indx + 1;
        }
        for (indx=0; indx<sndmx; indx++) {
            motapp->snd_list[indx]->device_id = motapp->cam_cnt + (int)indx + 1;
        }
    }

}

static void motpls_ntc(void)
{

    #ifdef HAVE_V4L2
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("v4l2   : available"));
    #else
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("v4l2   : not available"));
    #endif

    #ifdef HAVE_WEBP
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("webp   : available"));
    #else
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("webp   : not available"));
    #endif

    #ifdef HAVE_LIBCAM
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("libcam : available"));
    #else
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("libcam : not available"));
    #endif

    #ifdef HAVE_MYSQL
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("mysql  : available"));
    #else
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("mysql  : not available"));
    #endif

    #ifdef HAVE_MARIADB
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("MariaDB: available"));
    #else
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("MariaDB: not available"));
    #endif

    #ifdef HAVE_SQLITE3
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("sqlite3: available"));
    #else
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("sqlite3: not available"));
    #endif

    #ifdef HAVE_PGSQL
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("pgsql  : available"));
    #else
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("pgsql  : not available"));
    #endif

    #ifdef ENABLE_NLS
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("nls    : available"));
    #else
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("nls    : not available"));
    #endif

    #ifdef HAVE_ALSA
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("alsa   : available"));
    #else
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("alsa   : not available"));
    #endif

    #ifdef HAVE_FFTW3
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("fftw3  : available"));
    #else
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,_("fftw3  : not available"));
    #endif

}

/** Initialize upon start up or restart */
static void motpls_startup(ctx_motapp *motapp, int daemonize)
{
    motapp->conf = new cls_config;

    motlog = new cls_log(motapp);

    motapp->conf->init(motapp);

    motlog->log_level = motapp->conf->log_level;
    motlog->log_fflevel = motapp->conf->log_fflevel;
    motlog->set_log_file(motapp->conf->log_file);

    mytranslate_init();

    mytranslate_text("",motapp->conf->native_language);

    if (daemonize) {
        if (motapp->conf->daemon) {
            motpls_daemon();
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("MotionPlus running as daemon process"));
        }
    }

    motapp->conf->parms_log(motapp);

    motpls_pid_write(motapp);

    motpls_ntc();

    motpls_device_ids(motapp);

    motapp->dbse = new cls_dbse(motapp);
    motapp->webu = new cls_webu(motapp);

    motpls_allcams_init(motapp);

}

/** Start a camera thread */
static void motpls_start_thread_cam(ctx_dev *cam)
{
    int retcd;
    pthread_attr_t thread_attr;

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

    cam->restart_dev = true;
    retcd = pthread_create(&cam->thread_id, &thread_attr, &mlp_main, cam);
    if (retcd != 0) {
        MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Unable to start camera thread."));
    }
    pthread_attr_destroy(&thread_attr);

}

static void motpls_restart(ctx_motapp *motapp)
{

    MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Restarting MotionPlus."));

    motpls_shutdown(motapp);

    SLEEP(2, 0);

    motpls_startup(motapp, false);

    MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO,_("MotionPlus restarted"));

    motapp->restart_all = false;

}

/* Check for whether any cams are locked */
static void motpls_watchdog(ctx_motapp *motapp, uint camindx)
{
    int indx;

    if (motapp->cam_list[camindx]->running_dev == false) {
        return;
    }

    motapp->cam_list[camindx]->watchdog--;
    if (motapp->cam_list[camindx]->watchdog > 0) {
        return;
    }

    MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
        , _("Camera %d - Watchdog timeout.")
        , motapp->cam_list[camindx]->device_id);

    /* Shut down all the cameras */
    for (indx=0; indx<motapp->cam_cnt; indx++) {
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
        motapp->cam_list[indx]->finish_dev = true;
    }

    SLEEP(motapp->cam_list[camindx]->conf->watchdog_kill, 0);

    /* When in a watchdog timeout and we get to a kill situation,
     * we WILL have to leak memory because the freeing/deinit
     * processes could lock this thread which would stop everything.
    */
    for (indx=0; indx<motapp->cam_cnt; indx++) {
        if (motapp->cam_list[indx]->netcam != NULL) {
            if (motapp->cam_list[indx]->netcam->handler_finished == false) {
                MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                    , _("Camera %d - Watchdog netcam kill.")
                    , motapp->cam_list[indx]->device_id);
                pthread_kill(motapp->cam_list[indx]->netcam->net_thread.native_handle(), SIGVTALRM);
            }
        }
        if (motapp->cam_list[indx]->netcam_high != NULL) {
            if (motapp->cam_list[indx]->netcam_high->handler_finished == false) {
                MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                    , _("Camera %d - Watchdog netcam_high kill.")
                    , motapp->cam_list[indx]->device_id);
                pthread_kill(motapp->cam_list[indx]->netcam_high->net_thread.native_handle(), SIGVTALRM);
            }
        }
        if (motapp->cam_list[indx]->running_dev == true) {
            MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                , _("Camera %d - Watchdog kill.")
                , motapp->cam_list[indx]->device_id);
            pthread_kill(motapp->cam_list[indx]->thread_id, SIGVTALRM);
        };
        motapp->cam_list[indx]->running_dev = false;
        motapp->cam_list[indx]->restart_dev = false;
    }
    motapp->restart_all = true;
    motapp->finish_all = true;
    motapp->webu->wb_finish = true;
    motapp->threads_running = 0;

}

static int motpls_check_threadcount(ctx_motapp *motapp)
{
    uint thrdcnt, indx;

    thrdcnt = 0;

    for (indx=0; indx<(uint)motapp->cam_cnt; indx++) {
        if (motapp->cam_list[indx]->running_dev || motapp->cam_list[indx]->restart_dev) {
            thrdcnt++;
        }
    }
    for (indx=0; indx<motapp->snd_list.size(); indx++) {
        if (motapp->snd_list[indx]->handler_finished == false) {
            thrdcnt++;
        }
    }

    if ((motapp->webu->wb_finish == false) &&
        (motapp->webu->wb_daemon != NULL)) {
        thrdcnt++;
    }

    if (((thrdcnt == 0) && motapp->finish_all) ||
        ((thrdcnt == 0) && (motapp->threads_running == 0))) {
        return 1;
    } else {
        return 0;
    }

}

static void motpls_init(ctx_motapp *motapp, int argc, char *argv[])
{
    motapp->argc = argc;
    motapp->argv = argv;

    motapp->cam_list = (ctx_dev **)mymalloc(sizeof(ctx_dev *));
    motapp->cam_list[0] = nullptr;

    motapp->threads_running = 0;
    motapp->finish_all = false;
    motapp->restart_all = false;
    motapp->reload_all = false;
    motapp->parms_changed = false;
    motapp->pause = false;
    motapp->cam_add = false;
    motapp->cam_delete = -1;
    motapp->cam_cnt = 0;

    motapp->conf = nullptr;
    motapp->dbse = nullptr;
    motapp->webu = nullptr;

    pthread_key_create(&tls_key_threadnr, NULL);
    pthread_setspecific(tls_key_threadnr, (void *)(0));

    pthread_mutex_init(&motapp->global_lock, NULL);
    pthread_mutex_init(&motapp->mutex_parms, NULL);
    pthread_mutex_init(&motapp->mutex_camlst, NULL);
    pthread_mutex_init(&motapp->mutex_post, NULL);

    motpls_startup(motapp, true);

    motpls_av_init();

}

static void motpls_deinit(ctx_motapp *motapp)
{
    uint indx;

    motpls_av_deinit();

    motpls_shutdown(motapp);

    indx = 0;
    while (motapp->cam_list[indx] != nullptr) {
        mydelete(motapp->cam_list[indx]->conf);
        mydelete(motapp->cam_list[indx]);
        indx++;
    }
    myfree(motapp->cam_list);

    for (indx = 0; indx < motapp->snd_list.size();indx++) {
        mydelete(motapp->snd_list[indx]);
    }

    pthread_key_delete(tls_key_threadnr);
    pthread_mutex_destroy(&motapp->global_lock);
    pthread_mutex_destroy(&motapp->mutex_parms);
    pthread_mutex_destroy(&motapp->mutex_camlst);
    pthread_mutex_destroy(&motapp->mutex_post);

}
/* Check for whether to add a new cam */
static void motpls_cam_add(ctx_motapp *motapp)
{
    int indx_cam, indx;

    if (motapp->cam_add == false) {
        return;
    }

    pthread_mutex_lock(&motapp->mutex_camlst);
        motapp->conf->camera_add(motapp);
    pthread_mutex_unlock(&motapp->mutex_camlst);

    indx = 1;
    for (indx_cam=0; indx_cam<motapp->cam_cnt; indx_cam++) {
        if (indx < motapp->cam_list[indx_cam]->device_id) {
            indx = motapp->cam_list[indx_cam]->device_id;
        }
    }
    indx++;

    motapp->cam_list[motapp->cam_cnt-1]->device_id = indx;
    motapp->cam_list[motapp->cam_cnt-1]->conf->device_id = indx;
    motapp->cam_list[motapp->cam_cnt-1]->conf->webcontrol_port = 0;

    motapp->cam_add = false;

}

/* Check for whether to delete a new cam */
static void motpls_cam_delete(ctx_motapp *motapp)
{
    int indx1, indx2, maxcnt;
    ctx_dev **tmp, *cam;

    if ((motapp->cam_delete == -1) || (motapp->cam_cnt == 0)) {
        motapp->cam_delete = -1;
        return;
    }

    if (motapp->cam_delete >= motapp->cam_cnt) {
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Invalid camera specified for deletion. %d"), motapp->cam_delete);
        motapp->cam_delete = -1;
        return;
    }

    cam = motapp->cam_list[motapp->cam_delete];

    MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Stopping %s device_id %d")
        , cam->conf->device_name.c_str(), cam->device_id);
    cam->restart_dev = false;
    cam->finish_dev = true;

    maxcnt = 100;
    indx1 = 0;
    while ((cam->running_dev) && (indx1 < maxcnt)) {
        SLEEP(0, 50000000)
        indx1++;
    }
    if (indx1 == maxcnt) {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO, "Error stopping camera.  Timed out shutting down");
        motapp->cam_delete = -1;
        return;
    }
    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, "Camera stopped");

    tmp = (ctx_dev **)mymalloc(sizeof(ctx_dev *) * (uint)(motapp->cam_cnt));
    tmp[motapp->cam_cnt-1] = NULL;

    indx2 = 0;
    for (indx1=0; indx1<motapp->cam_cnt; indx1++) {
        if (indx1 != motapp->cam_delete) {
            tmp[indx2] = motapp->cam_list[indx1];
            indx2++;
        }
    }

    pthread_mutex_lock(&motapp->mutex_camlst);
        delete motapp->cam_list[motapp->cam_delete]->conf;
        delete motapp->cam_list[motapp->cam_delete];
        myfree(motapp->cam_list);
        motapp->cam_cnt--;
        motapp->cam_list = tmp;
    pthread_mutex_unlock(&motapp->mutex_camlst);

    motapp->cam_delete = -1;

}

/** Main entry point of MotionPlus. */
int main (int argc, char **argv)
{
    uint indx;
    ctx_motapp *motapp;

    motapp = new ctx_motapp;

    setup_signals();

    motpls_init(motapp, argc, argv);

    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
        ,_("Motionplus pid: %d"), getpid());


    while (true) {
        if (motapp->restart_all) {
            motpls_restart(motapp);
        }

        for (indx=0; indx<(uint)motapp->cam_cnt; indx++) {
            motpls_start_thread_cam(motapp->cam_list[indx]);
        }
        for (indx=0; indx<motapp->snd_list.size(); indx++) {
            motapp->snd_list[indx]->start();
        }

        if ((motapp->cam_cnt == 0) &&
            (motapp->snd_list.size() ==0)) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("No camera or sound configuration files specified."));
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Waiting for camera or sound configuration to be added via web control."));
        }

        while (true) {
            SLEEP(1, 0);

            if (motpls_check_threadcount(motapp)) {
                break;
            }

            for (indx=0; indx<(uint)motapp->cam_cnt; indx++) {
                /* Check if threads wants to be restarted */
                if ((motapp->cam_list[indx]->running_dev == false) &&
                    (motapp->cam_list[indx]->restart_dev == true)) {
                    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                        ,_("MotionPlus camera %d restart")
                        , motapp->cam_list[indx]->device_id);
                    motpls_start_thread_cam(motapp->cam_list[indx]);
                }
                motpls_watchdog(motapp, indx);
            }
            for (indx=0; indx<motapp->snd_list.size(); indx++) {
                if ((motapp->snd_list[indx]->handler_finished == true) &&
                    (motapp->snd_list[indx]->restart == true)) {
                    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                        ,_("MotionPlus sound %d restart")
                        , motapp->snd_list[indx]->device_id);
                    motapp->snd_list[indx]->start();
                }
                if ((motapp->snd_list[indx]->handler_finished == false) &&
                    (motapp->snd_list[indx]->handler_stop == true)) {
                    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                        ,_("MotionPlus stop sound %d")
                        , motapp->snd_list[indx]->device_id);
                    motapp->snd_list[indx]->stop();
                }
            }

            if (motsignal != MOTPLS_SIGNAL_NONE) {
                motpls_signal_process(motapp);
            }

            motpls_cam_add(motapp);
            motpls_cam_delete(motapp);

        }

        /* If there are no cameras running, this allows for adding */
        motpls_cam_add(motapp);

        motapp->finish_all = false;

        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motionplus devices finished"));

        if (motapp->restart_all) {
            SLEEP(1, 0);    /* Rest before restarting */
        } else if (motapp->reload_all) {
            motpls_deinit(motapp);
            motpls_init(motapp, argc, argv);
        } else {
            break;
        }
    }

    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("MotionPlus terminating"));
    motpls_deinit(motapp);

    delete motapp;

    return 0;
}

