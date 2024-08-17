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
static void motpls_signal_process(ctx_motapp *motapp)
{
    int indx;

    switch(motsignal){
    case MOTPLS_SIGNAL_ALARM:       /* Trigger snapshot */
        for (indx=0; indx<motapp->cam_cnt; indx++) {
            motapp->cam_list[indx]->action_snapshot = true;
        }
        break;
    case MOTPLS_SIGNAL_USR1:        /* Trigger the end of a event */
        for (indx=0; indx<motapp->cam_cnt; indx++) {
            motapp->cam_list[indx]->event_stop = true;
        }
        break;
    case MOTPLS_SIGNAL_SIGHUP:      /* Reload the parameters and restart*/
        motapp->reload_all = true;
        motapp->webu->wb_finish = true;
        for (indx=0; indx<motapp->cam_cnt; indx++) {
            motapp->cam_list[indx]->event_stop = true;
            motapp->cam_list[indx]->stop();
        }
        for (indx=0; indx<motapp->snd_cnt; indx++) {
            motapp->snd_list[indx]->stop();
        }
        break;
    case MOTPLS_SIGNAL_SIGTERM:     /* Quit application */
        motapp->webu->wb_finish = true;
        for (indx=0; indx<motapp->cam_cnt; indx++) {
            motapp->cam_list[indx]->event_stop = true;
            motapp->cam_list[indx]->stop();
        }
        for (indx=0; indx<motapp->snd_cnt; indx++) {
            motapp->snd_list[indx]->stop();
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
static void motpls_pid_write(ctx_motapp *motapp)
{
    FILE *pidf = NULL;

    if (motapp->cfg->pid_file != "") {
        pidf = myfopen(motapp->cfg->pid_file.c_str(), "w+e");
        if (pidf) {
            (void)fprintf(pidf, "%d\n", getpid());
            myfclose(pidf);
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Created process id file %s. Process ID is %d")
                ,motapp->cfg->pid_file.c_str(), getpid());
        } else {
            MOTPLS_LOG(EMG, TYPE_ALL, SHOW_ERRNO
                , _("Cannot create process id file (pid file) %s")
                , motapp->cfg->pid_file.c_str());
        }
    }

    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Motionplus pid: %d"), getpid());

}

/** Remove the process id file ( pid file ) before MotionPlus exit. */
static void motpls_pid_remove(ctx_motapp *motapp)
{
    if ((motapp->cfg->pid_file != "") &&
        (motapp->reload_all == false)) {
        if (!unlink(motapp->cfg->pid_file.c_str())) {
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
static void motpls_allcams_init(ctx_motapp *motapp)
{
    int indx, indx1;
    int row, col, mx_row, mx_col, col_chk;
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
            motapp->cam_list[indx]->cfg->stream_preview_scale;

        params_loc->update_params = true;
        util_parms_parse(params_loc
            , "stream_preview_location"
            , motapp->cam_list[indx]->cfg->stream_preview_location);
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
                , motapp->cam_list[indx]->cfg->device_id);
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
                        , motapp->cam_list[indx]->cfg->device_id
                        , motapp->cam_list[indx1]->cfg->device_id
                        , motapp->cam_list[indx]->all_loc.row
                        , motapp->cam_list[indx]->all_loc.col);
                    cfg_valid = false;
                }
            }
        }
        if (motapp->cam_list[indx]->all_loc.row == 0) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "Invalid stream_preview_location row cam %d, row %d"
                , motapp->cam_list[indx]->cfg->device_id
                , motapp->cam_list[indx]->all_loc.row);
            cfg_valid = false;
        }
        if (motapp->cam_list[indx]->all_loc.col == 0) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "Invalid stream_preview_location col cam %d, col %d"
                , motapp->cam_list[indx]->cfg->device_id
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

static void motpls_device_ids(ctx_motapp *motapp)
{
    int indx, indx2;
    int invalid_ids;

     /* Defaults */
    for (indx=0; indx<motapp->cam_cnt; indx++) {
        if (motapp->cam_list[indx]->cfg->device_id != 0) {
            motapp->cam_list[indx]->device_id = motapp->cam_list[indx]->cfg->device_id;
        } else {
            motapp->cam_list[indx]->device_id = (int)indx + 1;
        }
    }
    for (indx=0; indx<motapp->snd_cnt; indx++) {
        if (motapp->snd_list[indx]->cfg->device_id != 0) {
            motapp->snd_list[indx]->device_id = motapp->snd_list[indx]->cfg->device_id;
        } else {
            motapp->snd_list[indx]->device_id =  (int)(motapp->cam_cnt + indx + 1);
        }
    }

    /*Check for unique values*/
    invalid_ids = false;
    for (indx=0; indx<motapp->cam_cnt; indx++) {
        for (indx2=indx+1; indx2<motapp->cam_cnt; indx2++) {
           if (motapp->cam_list[indx]->device_id == motapp->cam_list[indx2]->device_id) {
                invalid_ids = true;
            }
        }
        for (indx2=0; indx2<motapp->snd_cnt; indx2++) {
           if (motapp->cam_list[indx]->device_id == motapp->snd_list[indx2]->device_id) {
                invalid_ids = true;
            }
        }
    }
    for (indx=0; indx<motapp->snd_cnt; indx++) {
        for (indx2=indx+1; indx2<motapp->snd_cnt; indx2++) {
           if (motapp->snd_list[indx]->device_id == motapp->snd_list[indx2]->device_id) {
                invalid_ids = true;
            }
        }
    }

    if (invalid_ids) {
        MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Device IDs are not unique."));
        MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Falling back to sequence numbers"));
        for (indx=0; indx<motapp->cam_cnt; indx++) {
            motapp->cam_list[indx]->device_id = indx + 1;
        }
        for (indx=0; indx<motapp->snd_cnt; indx++) {
            motapp->snd_list[indx]->device_id = motapp->cam_cnt+ indx + 1;
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
static void motpls_watchdog(ctx_motapp *motapp, uint camindx)
{
    int indx;

    if (motapp->cam_list[camindx]->handler_finished == true) {
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
        motapp->cam_list[indx]->event_stop = true;
        pthread_mutex_unlock(&motapp->mutex_camlst);
        pthread_mutex_unlock(&motapp->mutex_parms);
        pthread_mutex_unlock(&motapp->mutex_post);
        pthread_mutex_unlock(&motapp->dbse->mutex_dbse);
        pthread_mutex_unlock(&motapp->global_lock);
        pthread_mutex_unlock(&motapp->cam_list[indx]->stream.mutex);

        if ((motapp->cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (motapp->cam_list[indx]->netcam != nullptr)) {
            pthread_mutex_unlock(&motapp->cam_list[indx]->netcam->mutex);
            pthread_mutex_unlock(&motapp->cam_list[indx]->netcam->mutex_pktarray);
            pthread_mutex_unlock(&motapp->cam_list[indx]->netcam->mutex_transfer);
            motapp->cam_list[indx]->netcam->handler_stop = true;
        }
        if ((motapp->cam_list[indx]->camera_type == CAMERA_TYPE_NETCAM) &&
            (motapp->cam_list[indx]->netcam_high != nullptr)) {
            pthread_mutex_unlock(&motapp->cam_list[indx]->netcam_high->mutex);
            pthread_mutex_unlock(&motapp->cam_list[indx]->netcam_high->mutex_pktarray);
            pthread_mutex_unlock(&motapp->cam_list[indx]->netcam_high->mutex_transfer);
            motapp->cam_list[indx]->netcam_high->handler_stop = true;
        }

        motapp->cam_list[indx]->stop();
        if (motsignal != MOTPLS_SIGNAL_SIGTERM) {
            motapp->cam_list[indx]->handler_stop = false;   /*Trigger a restart*/
        }
    }

}

static bool motpls_check_devices(ctx_motapp *motapp)
{
    int indx;
    bool retcd;

    for (indx=0; indx<motapp->cam_cnt; indx++) {
        motpls_watchdog(motapp, indx);
    }

    retcd = false;
    for (indx=0; indx<motapp->cam_cnt; indx++) {
        if (motapp->cam_list[indx]->handler_finished == false) {
            retcd = true;
        } else if (motapp->cam_list[indx]->handler_stop == false) {
            motapp->cam_list[indx]->start();
            retcd = true;
        }
    }
    for (indx=0; indx<motapp->snd_cnt; indx++) {
        if (motapp->snd_list[indx]->handler_finished == false) {
            retcd = true;
        } else if (motapp->snd_list[indx]->handler_stop == false) {
            motapp->snd_list[indx]->start();
            retcd = true;
        }
    }

    if ((motapp->webu->wb_finish == false) &&
        (motapp->webu->wb_daemon != NULL)) {
        retcd = true;
    }

    return retcd;

}

static void motpls_init(ctx_motapp *motapp, int argc, char *argv[])
{
    int indx;

    motapp->argc = argc;
    motapp->argv = argv;

    motapp->reload_all = false;
    motapp->parms_changed = false;
    motapp->pause = false;
    motapp->cam_add = false;
    motapp->cam_delete = -1;
    motapp->cam_cnt = 0;
    motapp->snd_cnt = 0;
    motapp->conf_src = nullptr;
    motapp->cfg = nullptr;
    motapp->dbse = nullptr;
    motapp->webu = nullptr;

    pthread_mutex_init(&motapp->global_lock, NULL);
    pthread_mutex_init(&motapp->mutex_parms, NULL);
    pthread_mutex_init(&motapp->mutex_camlst, NULL);
    pthread_mutex_init(&motapp->mutex_post, NULL);

    motapp->conf_src = new cls_config;
    motapp->conf_src->init(motapp);

    motapp->cfg = new cls_config;
    motapp->cfg->parms_copy(motapp->conf_src);

    motlog->log_level = motapp->cfg->log_level;
    motlog->log_fflevel = motapp->cfg->log_fflevel;
    motlog->set_log_file(motapp->cfg->log_file);

    mytranslate_init();

    mytranslate_text("",motapp->cfg->native_language);

    if (motapp->cfg->daemon) {
        motpls_daemon();
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("MotionPlus running as daemon process"));
    }

    motapp->cfg->parms_log(motapp);

    motpls_pid_write(motapp);

    motpls_ntc();

    motpls_device_ids(motapp);

    motapp->dbse = new cls_dbse(motapp);
    motapp->webu = new cls_webu(motapp);

    motpls_allcams_init(motapp);

    motpls_av_init();

    if ((motapp->cam_cnt > 0) || (motapp->snd_cnt > 0)) {
        for (indx=0; indx<motapp->cam_cnt; indx++) {
            motapp->cam_list[indx]->start();
        }
        for (indx=0; indx<motapp->snd_cnt; indx++) {
            motapp->snd_list[indx]->start();
        }
    } else {
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("No camera or sound configuration files specified."));
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Waiting for camera or sound configuration to be added via web control."));
    }

}

static void motpls_deinit(ctx_motapp *motapp)
{
    int indx;

    motpls_av_deinit();
    motpls_pid_remove(motapp);

    mydelete(motapp->webu);
    mydelete(motapp->dbse);
    mydelete(motapp->conf_src);
    mydelete(motapp->cfg);
    mydelete(motapp->all_sizes);

    for (indx = 0; indx < motapp->cam_cnt;indx++) {
        mydelete(motapp->cam_list[indx]);
    }

    for (indx = 0; indx < motapp->snd_cnt;indx++) {
        mydelete(motapp->snd_list[indx]);
    }

    pthread_mutex_destroy(&motapp->global_lock);
    pthread_mutex_destroy(&motapp->mutex_parms);
    pthread_mutex_destroy(&motapp->mutex_camlst);
    pthread_mutex_destroy(&motapp->mutex_post);

}
/* Check for whether to add a new cam */
static void motpls_cam_add(ctx_motapp *motapp)
{
    if (motapp->cam_add == false) {
        return;
    }

    pthread_mutex_lock(&motapp->mutex_camlst);
        motapp->cfg->camera_add(motapp, "", false);
    pthread_mutex_unlock(&motapp->mutex_camlst);

    motapp->cam_add = false;

}
/* Check for whether to delete a new cam */
static void motpls_cam_delete(ctx_motapp *motapp)
{
    cls_camera *cam;

    if ((motapp->cam_delete == -1) ||
        (motapp->cam_cnt == 0)) {
        motapp->cam_delete = -1;
        return;
    }

    if (motapp->cam_delete >= (int)motapp->cam_cnt) {
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Invalid camera specified for deletion. %d"), motapp->cam_delete);
        motapp->cam_delete = -1;
        return;
    }

    cam = motapp->cam_list[motapp->cam_delete];

    MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Stopping %s device_id %d")
        , cam->cfg->device_name.c_str(), cam->device_id);

    cam->stop();

    if (cam->handler_finished == false) {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO, "Error stopping camera.  Timed out shutting down");
        motapp->cam_delete = -1;
        return;
    }
    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, "Camera stopped");

    pthread_mutex_lock(&motapp->mutex_camlst);
        mydelete(motapp->cam_list[motapp->cam_delete]);
        motapp->cam_list.erase(motapp->cam_list.begin() + motapp->cam_delete);
    pthread_mutex_unlock(&motapp->mutex_camlst);

    motapp->cam_delete = -1;

}

/** Main entry point of MotionPlus. */
int main (int argc, char **argv)
{
    ctx_motapp *motapp;

    motapp = new ctx_motapp;
    motlog = new cls_log(motapp);

    setup_signals();
    mythreadname_set("mp",0,"");

    while (true) {
        motpls_init(motapp, argc, argv);
        while (motpls_check_devices(motapp)) {
            SLEEP(1, 0);
            if (motsignal != MOTPLS_SIGNAL_NONE) {
                motpls_signal_process(motapp);
            }
            motpls_cam_add(motapp);
            motpls_cam_delete(motapp);
        }
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Motionplus devices finished"));
        if (motapp->reload_all) {
            motpls_deinit(motapp);
            motapp->reload_all = false;
        } else {
            break;
        }
    }

    motpls_deinit(motapp);

    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("MotionPlus terminating"));

    mydelete(motlog);
    mydelete(motapp);

    return 0;
}

