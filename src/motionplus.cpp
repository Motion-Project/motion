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
#include "camera.hpp"
#include "sound.hpp"
#include "dbse.hpp"
#include "webu.hpp"
#include "video_v4l2.hpp"
#include "movie.hpp"
#include "netcam.hpp"

volatile enum MOTPLS_SIGNAL motsignal;

/** Process signals sent */
static void motpls_signal_process(ctx_motapp *app)
{
    int indx;

    switch(motsignal){
    case MOTPLS_SIGNAL_ALARM:       /* Trigger snapshot */
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->action_snapshot = true;
        }
        break;
    case MOTPLS_SIGNAL_USR1:        /* Trigger the end of a event */
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->event_stop = true;
        }
        break;
    case MOTPLS_SIGNAL_SIGHUP:      /* Reload the parameters and restart*/
        app->reload_all = true;
        app->webu->wb_finish = true;
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->event_stop = true;
            app->cam_list[indx]->handler_shutdown();
        }
        for (indx=0; indx<app->snd_cnt; indx++) {
            app->snd_list[indx]->handler_shutdown();
        }
        break;
    case MOTPLS_SIGNAL_SIGTERM:     /* Quit application */
        app->webu->wb_finish = true;
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->event_stop = true;
            app->cam_list[indx]->handler_shutdown();
        }
        for (indx=0; indx<app->snd_cnt; indx++) {
            app->snd_list[indx]->handler_shutdown();
        }
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
static void motpls_pid_write(ctx_motapp *app)
{
    FILE *pidf = NULL;

    if (app->cfg->pid_file != "") {
        pidf = myfopen(app->cfg->pid_file.c_str(), "w+e");
        if (pidf) {
            (void)fprintf(pidf, "%d\n", getpid());
            myfclose(pidf);
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Created process id file %s. Process ID is %d")
                ,app->cfg->pid_file.c_str(), getpid());
        } else {
            MOTPLS_LOG(EMG, TYPE_ALL, SHOW_ERRNO
                , _("Cannot create process id file (pid file) %s")
                , app->cfg->pid_file.c_str());
        }
    }

    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Motionplus pid: %d"), getpid());

}

/** Remove the process id file ( pid file ) before MotionPlus exit. */
static void motpls_pid_remove(ctx_motapp *app)
{
    if ((app->cfg->pid_file != "") &&
        (app->reload_all == false)) {
        if (!unlink(app->cfg->pid_file.c_str())) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Removed process id file (pid file)."));
        } else{
            MOTPLS_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Error removing pid file"));
        }
    }
}

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
}

void motpls_av_deinit(void)
{
    avformat_network_deinit();
}

/* Validate or set the position on the all cameras image*/
static void motpls_allcams_init(ctx_motapp *app)
{
    int indx, indx1;
    int row, col, mx_row, mx_col, col_chk;
    bool cfg_valid, chk;
    std::string cfg_row, cfg_col;
    ctx_params  *params_loc;
    p_lst *lst;
    p_it it;

    app->all_sizes = new ctx_all_sizes;
    app->all_sizes->height = 0;
    app->all_sizes->width = 0;
    app->all_sizes->img_sz = 0;
    app->all_sizes->reset = true;

    if (app->cam_cnt < 1) {
        return;
    }

    params_loc = new ctx_params;

    for (indx=0; indx<app->cam_cnt; indx++) {
        app->cam_list[indx]->all_loc.row = -1;
        app->cam_list[indx]->all_loc.col = -1;
        app->cam_list[indx]->all_loc.offset_user_col = 0;
        app->cam_list[indx]->all_loc.offset_user_row = 0;
        app->cam_list[indx]->all_loc.scale =
            app->cam_list[indx]->cfg->stream_preview_scale;

        params_loc->update_params = true;
        util_parms_parse(params_loc
            , "stream_preview_location"
            , app->cam_list[indx]->cfg->stream_preview_location);
        lst = &params_loc->params_array;

        for (it = lst->begin(); it != lst->end(); it++) {
            if (it->param_name == "row") {
                app->cam_list[indx]->all_loc.row = mtoi(it->param_value);
            }
            if (it->param_name == "col") {
                app->cam_list[indx]->all_loc.col = mtoi(it->param_value);
            }
            if (it->param_name == "offset_col") {
                app->cam_list[indx]->all_loc.offset_user_col =
                    mtoi(it->param_value);
            }
            if (it->param_name == "offset_row") {
                app->cam_list[indx]->all_loc.offset_user_row =
                    mtoi(it->param_value);
            }
        }
        params_loc->params_array.clear();
    }

    delete params_loc;

    mx_row = 0;
    mx_col = 0;
    for (indx=0; indx<app->cam_cnt; indx++) {
        if (mx_col < app->cam_list[indx]->all_loc.col) {
            mx_col = app->cam_list[indx]->all_loc.col;
        }
        if (mx_row < app->cam_list[indx]->all_loc.row) {
            mx_row = app->cam_list[indx]->all_loc.row;
        }
    }
    cfg_valid = true;
    for (indx=0; indx<app->cam_cnt; indx++) {
        if ((app->cam_list[indx]->all_loc.col == -1) ||
            (app->cam_list[indx]->all_loc.row == -1)) {
            cfg_valid = false;
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "No stream_preview_location for cam %d"
                , app->cam_list[indx]->cfg->device_id);
        } else {
            for (indx1=0; indx1<app->cam_cnt; indx1++) {
                if ((app->cam_list[indx]->all_loc.col ==
                    app->cam_list[indx1]->all_loc.col) &&
                    (app->cam_list[indx]->all_loc.row ==
                    app->cam_list[indx1]->all_loc.row) &&
                    (indx != indx1)) {
                    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                        , "Duplicate stream_preview_location "
                        " cam %d, cam %d row %d col %d"
                        , app->cam_list[indx]->cfg->device_id
                        , app->cam_list[indx1]->cfg->device_id
                        , app->cam_list[indx]->all_loc.row
                        , app->cam_list[indx]->all_loc.col);
                    cfg_valid = false;
                }
            }
        }
        if (app->cam_list[indx]->all_loc.row == 0) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "Invalid stream_preview_location row cam %d, row %d"
                , app->cam_list[indx]->cfg->device_id
                , app->cam_list[indx]->all_loc.row);
            cfg_valid = false;
        }
        if (app->cam_list[indx]->all_loc.col == 0) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "Invalid stream_preview_location col cam %d, col %d"
                , app->cam_list[indx]->cfg->device_id
                , app->cam_list[indx]->all_loc.col);
            cfg_valid = false;
        }
    }

    for (row=1; row<=mx_row; row++) {
        chk = false;
        for (indx=0; indx<app->cam_cnt; indx++) {
            if (row == app->cam_list[indx]->all_loc.row) {
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
            for (indx=0; indx<app->cam_cnt; indx++) {
                if ((row == app->cam_list[indx]->all_loc.row) &&
                    (col == app->cam_list[indx]->all_loc.col)) {
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
        for (indx=0; indx<app->cam_cnt; indx++) {
            if (col == 1) {
                col++;
            } else {
                row++;
                col = 1;
            }
            app->cam_list[indx]->all_loc.col = col;
            app->cam_list[indx]->all_loc.row = row;
            app->cam_list[indx]->all_loc.scale = -1;
        }
    }

    for (indx=0; indx<app->cam_cnt; indx++) {
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO
            ,"stream_preview_location values. Device %d row %d col %d"
            , app->cam_list[indx]->device_id
            , app->cam_list[indx]->all_loc.row
            , app->cam_list[indx]->all_loc.col);
    }

}

static void motpls_device_ids(ctx_motapp *app)
{
    int indx, indx2;
    int invalid_ids;

     /* Defaults */
    for (indx=0; indx<app->cam_cnt; indx++) {
        if (app->cam_list[indx]->cfg->device_id != 0) {
            app->cam_list[indx]->device_id = app->cam_list[indx]->cfg->device_id;
        } else {
            app->cam_list[indx]->device_id = (int)indx + 1;
        }
    }
    for (indx=0; indx<app->snd_cnt; indx++) {
        if (app->snd_list[indx]->cfg->device_id != 0) {
            app->snd_list[indx]->device_id = app->snd_list[indx]->cfg->device_id;
        } else {
            app->snd_list[indx]->device_id =  (int)(app->cam_cnt + indx + 1);
        }
    }

    /*Check for unique values*/
    invalid_ids = false;
    for (indx=0; indx<app->cam_cnt; indx++) {
        for (indx2=indx+1; indx2<app->cam_cnt; indx2++) {
           if (app->cam_list[indx]->device_id == app->cam_list[indx2]->device_id) {
                invalid_ids = true;
            }
        }
        for (indx2=0; indx2<app->snd_cnt; indx2++) {
           if (app->cam_list[indx]->device_id == app->snd_list[indx2]->device_id) {
                invalid_ids = true;
            }
        }
    }
    for (indx=0; indx<app->snd_cnt; indx++) {
        for (indx2=indx+1; indx2<app->snd_cnt; indx2++) {
           if (app->snd_list[indx]->device_id == app->snd_list[indx2]->device_id) {
                invalid_ids = true;
            }
        }
    }

    if (invalid_ids) {
        MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Device IDs are not unique."));
        MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Falling back to sequence numbers"));
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->device_id = indx + 1;
        }
        for (indx=0; indx<app->snd_cnt; indx++) {
            app->snd_list[indx]->device_id = app->cam_cnt+ indx + 1;
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

/* Check for whether any cams are locked */
static void motpls_watchdog(ctx_motapp *app, uint camindx)
{
    int indx;

    if (app->cam_list[camindx]->handler_finished == true) {
        return;
    }

    app->cam_list[camindx]->watchdog--;
    if (app->cam_list[camindx]->watchdog > 0) {
        return;
    }

    MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
        , _("Camera %d - Watchdog timeout.")
        , app->cam_list[camindx]->device_id);

    /* Shut down all the cameras */
    for (indx=0; indx<app->cam_cnt; indx++) {
        app->cam_list[indx]->event_stop = true;
        pthread_mutex_unlock(&app->mutex_camlst);
        pthread_mutex_unlock(&app->mutex_parms);
        pthread_mutex_unlock(&app->mutex_post);
        pthread_mutex_unlock(&app->dbse->mutex_dbse);
        pthread_mutex_unlock(&app->global_lock);
        pthread_mutex_unlock(&app->cam_list[indx]->stream.mutex);

        if ((app->cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (app->cam_list[indx]->netcam != nullptr)) {
            pthread_mutex_unlock(&app->cam_list[indx]->netcam->mutex);
            pthread_mutex_unlock(&app->cam_list[indx]->netcam->mutex_pktarray);
            pthread_mutex_unlock(&app->cam_list[indx]->netcam->mutex_transfer);
            app->cam_list[indx]->netcam->handler_stop = true;
        }
        if ((app->cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (app->cam_list[indx]->netcam_high != nullptr)) {
            pthread_mutex_unlock(&app->cam_list[indx]->netcam_high->mutex);
            pthread_mutex_unlock(&app->cam_list[indx]->netcam_high->mutex_pktarray);
            pthread_mutex_unlock(&app->cam_list[indx]->netcam_high->mutex_transfer);
            app->cam_list[indx]->netcam_high->handler_stop = true;
        }

        app->cam_list[indx]->handler_shutdown();
        if (motsignal != MOTPLS_SIGNAL_SIGTERM) {
            app->cam_list[indx]->handler_stop = false;   /*Trigger a restart*/
        }
    }

}

static bool motpls_check_devices(ctx_motapp *app)
{
    int indx;
    bool retcd;

    for (indx=0; indx<app->cam_cnt; indx++) {
        motpls_watchdog(app, indx);
    }

    retcd = false;
    for (indx=0; indx<app->cam_cnt; indx++) {
        if (app->cam_list[indx]->handler_finished == false) {
            retcd = true;
        } else if (app->cam_list[indx]->handler_stop == false) {
            app->cam_list[indx]->handler_startup();
            retcd = true;
        }
    }
    for (indx=0; indx<app->snd_cnt; indx++) {
        if (app->snd_list[indx]->handler_finished == false) {
            retcd = true;
        } else if (app->snd_list[indx]->handler_stop == false) {
            app->snd_list[indx]->handler_startup();
            retcd = true;
        }
    }

    if ((app->webu->wb_finish == false) &&
        (app->webu->wb_daemon != NULL)) {
        retcd = true;
    }

    return retcd;

}

static void motpls_init(ctx_motapp *app, int argc, char *argv[])
{
    int indx;

    app->argc = argc;
    app->argv = argv;

    app->reload_all = false;
    app->parms_changed = false;
    app->pause = false;
    app->cam_add = false;
    app->cam_delete = -1;
    app->cam_cnt = 0;
    app->snd_cnt = 0;
    app->conf_src = nullptr;
    app->cfg = nullptr;
    app->dbse = nullptr;
    app->webu = nullptr;

    pthread_mutex_init(&app->global_lock, NULL);
    pthread_mutex_init(&app->mutex_parms, NULL);
    pthread_mutex_init(&app->mutex_camlst, NULL);
    pthread_mutex_init(&app->mutex_post, NULL);

    app->conf_src = new cls_config;
    app->conf_src->init(app);

    app->cfg = new cls_config;
    app->cfg->parms_copy(app->conf_src);

    motlog->log_level = app->cfg->log_level;
    motlog->log_fflevel = app->cfg->log_fflevel;
    motlog->set_log_file(app->cfg->log_file);

    mytranslate_init();

    mytranslate_text("",app->cfg->native_language);

    if (app->cfg->daemon) {
        motpls_daemon();
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("MotionPlus running as daemon process"));
    }

    app->cfg->parms_log(app);

    motpls_pid_write(app);

    motpls_ntc();

    motpls_device_ids(app);

    app->dbse = new cls_dbse(app);

    motpls_allcams_init(app);

    motpls_av_init();

    if ((app->cam_cnt > 0) || (app->snd_cnt > 0)) {
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->handler_startup();
        }
        for (indx=0; indx<app->snd_cnt; indx++) {
            app->snd_list[indx]->handler_startup();
        }
    } else {
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("No camera or sound configuration files specified."));
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Waiting for camera or sound configuration to be added via web control."));
    }

    /* Start web control last */
    app->webu = new cls_webu(app);

}

static void motpls_deinit(ctx_motapp *app)
{
    int indx;

    motpls_av_deinit();
    motpls_pid_remove(app);

    mydelete(app->webu);
    mydelete(app->dbse);
    mydelete(app->conf_src);
    mydelete(app->cfg);
    mydelete(app->all_sizes);

    for (indx = 0; indx < app->cam_cnt;indx++) {
        mydelete(app->cam_list[indx]);
    }

    for (indx = 0; indx < app->snd_cnt;indx++) {
        mydelete(app->snd_list[indx]);
    }

    pthread_mutex_destroy(&app->global_lock);
    pthread_mutex_destroy(&app->mutex_parms);
    pthread_mutex_destroy(&app->mutex_camlst);
    pthread_mutex_destroy(&app->mutex_post);

}
/* Check for whether to add a new cam */
static void motpls_cam_add(ctx_motapp *app)
{
    if (app->cam_add == false) {
        return;
    }

    pthread_mutex_lock(&app->mutex_camlst);
        app->cfg->camera_add(app, "", false);
    pthread_mutex_unlock(&app->mutex_camlst);

    app->cam_add = false;

}
/* Check for whether to delete a new cam */
static void motpls_cam_delete(ctx_motapp *app)
{
    cls_camera *cam;

    if ((app->cam_delete == -1) ||
        (app->cam_cnt == 0)) {
        app->cam_delete = -1;
        return;
    }

    if (app->cam_delete >= (int)app->cam_cnt) {
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Invalid camera specified for deletion. %d"), app->cam_delete);
        app->cam_delete = -1;
        return;
    }

    cam = app->cam_list[app->cam_delete];

    MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Stopping %s device_id %d")
        , cam->cfg->device_name.c_str(), cam->device_id);

    cam->handler_shutdown();

    if (cam->handler_finished == false) {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO, "Error stopping camera.  Timed out shutting down");
        app->cam_delete = -1;
        return;
    }
    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, "Camera stopped");

    pthread_mutex_lock(&app->mutex_camlst);
        mydelete(app->cam_list[app->cam_delete]);
        app->cam_list.erase(app->cam_list.begin() + app->cam_delete);
        app->cam_cnt--;
    pthread_mutex_unlock(&app->mutex_camlst);

    app->cam_delete = -1;

}

/** Main entry point of MotionPlus. */
int main (int argc, char **argv)
{
    ctx_motapp *app;

    app = new ctx_motapp;
    motlog = new cls_log(app);

    setup_signals();
    mythreadname_set("mp",0,"");

    while (true) {
        motpls_init(app, argc, argv);
        while (motpls_check_devices(app)) {
            SLEEP(1, 0);
            if (motsignal != MOTPLS_SIGNAL_NONE) {
                motpls_signal_process(app);
            }
            motpls_cam_add(app);
            motpls_cam_delete(app);
        }
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motionplus devices finished"));
        if (app->reload_all) {
            motpls_deinit(app);
            app->reload_all = false;
        } else {
            break;
        }
    }

    motpls_deinit(app);

    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("MotionPlus terminating"));

    mydelete(motlog);
    mydelete(app);

    return 0;
}

