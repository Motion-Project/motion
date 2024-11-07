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
#include "util.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "camera.hpp"
#include "netcam.hpp"
#include "schedule.hpp"

static void *schedule_handler(void *arg)
{
    ((cls_schedule *)arg)->handler();
    return nullptr;
}

void cls_schedule::check_schedule()
{
    int indxc, indx;
    int cur_dy;
    cls_camera *p_cam;
    struct tm c_tm;
    struct timespec curr_ts;
    bool stopcam;

    if ((restart == true) || (handler_stop == true)) {
        return;
    }

    clock_gettime(CLOCK_REALTIME, &curr_ts);
    localtime_r(&curr_ts.tv_sec, &c_tm);
    cur_dy = c_tm.tm_wday;

    for (indxc=0;indxc<app->cam_cnt;indxc++) {
        p_cam = app->cam_list[indxc];
        if (p_cam->schedule.size() == 7) {
            stopcam = false;
            for (indx=0; indx<p_cam->schedule[cur_dy].size(); indx++) {
                if ((p_cam->schedule[cur_dy][indx].action == "stop") &&
                    (c_tm.tm_hour >= p_cam->schedule[cur_dy][indx].st_hr) &&
                    (c_tm.tm_min  >= p_cam->schedule[cur_dy][indx].st_min) &&
                    (c_tm.tm_hour <= p_cam->schedule[cur_dy][indx].en_hr) &&
                    (c_tm.tm_min  <= p_cam->schedule[cur_dy][indx].en_min) ) {
                    if (p_cam->schedule[cur_dy][indx].detect) {
                        stopcam = false;
                    } else {
                        stopcam = true;
                    }
                }
            }

            if ((stopcam == true) && (p_cam->handler_stop == false)) {
                p_cam->event_stop = true;
                p_cam->restart = false;
                p_cam->handler_stop = true;
                p_cam->finish = true;
                if (p_cam->camera_type == CAMERA_TYPE_NETCAM) {
                    if (p_cam->netcam != nullptr) {
                        p_cam->netcam->idur = 0;
                    }
                    if (p_cam->netcam_high != nullptr) {
                        p_cam->netcam_high->idur = 0;
                    }
                }
                p_cam->handler_shutdown();
            } else if ((stopcam == false) && (p_cam->handler_running == false)) {
                p_cam->handler_startup();
            }
        }
    }
}

void cls_schedule::timing()
{
    int indx;
    for (indx=0; indx<30; indx++) {
        if ((restart == true) || (handler_stop == true)) {
            return;
        }
        SLEEP(1, 0);
    }
}

void cls_schedule::handler()
{
    mythreadname_set("sh", 0, "schedule");

    while (handler_stop == false) {
        check_schedule();
        timing();
    }

    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Schedule process closed"));
    handler_running = false;
    pthread_exit(NULL);
}

void cls_schedule::handler_startup()
{
    int retcd;
    pthread_attr_t thread_attr;

    if (handler_running == false) {
        handler_running = true;
        handler_stop = false;
        restart = false;
        pthread_attr_init(&thread_attr);
        pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
        retcd = pthread_create(&handler_thread, &thread_attr, &schedule_handler, this);
        if (retcd != 0) {
            MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Unable to start schedule thread."));
            handler_running = false;
            handler_stop = true;
        }
        pthread_attr_destroy(&thread_attr);
    }
}

void cls_schedule::handler_shutdown()
{
    int waitcnt;

    if (handler_running == true) {
        handler_stop = true;
        waitcnt = 0;
        while ((handler_running == true) && (waitcnt < app->cfg->watchdog_tmo)){
            SLEEP(1,0)
            waitcnt++;
        }
        if (waitcnt == app->cfg->watchdog_tmo) {
            MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                , _("Normal shutdown of schedule thread failed"));
            if (app->cfg->watchdog_kill > 0) {
                MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Waiting additional %d seconds (watchdog_kill).")
                    ,app->cfg->watchdog_kill);
                waitcnt = 0;
                while ((handler_running == true) && (waitcnt < app->cfg->watchdog_kill)){
                    SLEEP(1,0)
                    waitcnt++;
                }
                if (waitcnt == app->cfg->watchdog_kill) {
                    MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                        , _("No response to shutdown.  Killing it."));
                    MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                        , _("Memory leaks will occur."));
                    pthread_kill(handler_thread, SIGVTALRM);
                }
            } else {
                MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                    , _("watchdog_kill set to terminate application."));
                exit(1);
            }
        }
        handler_running = false;
        watchdog = app->cfg->watchdog_tmo;
    }
}

cls_schedule::cls_schedule(cls_motapp *p_app)
{
    app = p_app;

    handler_running = false;
    handler_stop = true;
    finish = false;
    watchdog = app->cfg->watchdog_tmo;

    handler_startup();
}

cls_schedule::~cls_schedule()
{
    finish = true;
    handler_shutdown();
}
