/*
 *    This file is part of Motion.
 *
 *    Motion is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motion is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "motion.hpp"
#include "util.hpp"
#include "conf.hpp"
#include "logger.hpp"

cls_log *motlog;

const char *log_type_str[]  = {NULL, "COR", "STR", "ENC", "NET", "DBS", "EVT", "TRK", "VID", "ALL"};
const char *log_level_str[] = {NULL, "EMG", "ALR", "CRT", "ERR", "WRN", "NTC", "INF", "DBG", "ALL"};

void ff_log(void *var1, int errnbr, const char *fmt, va_list vlist)
{
    (void)var1;
    char buff[1024];
    int fflvl;

    vsnprintf(buff, sizeof(buff), fmt, vlist);

    buff[strlen(buff)-1] = 0;

    if (strstr(buff, "forced frame type") != nullptr) {
        return;
    }

    /*
    AV_LOG_QUIET    -8  1
    AV_LOG_PANIC     0  2
    AV_LOG_FATAL     8  3
    AV_LOG_ERROR    16  4
    AV_LOG_WARNING  24  5
    AV_LOG_INFO     32  6
    AV_LOG_VERBOSE  40  7
    AV_LOG_DEBUG    48  8
    AV_LOG_TRACE    56  9
    */

    fflvl = ((motlog->log_fflevel -2) * 8);

    if (errnbr <= fflvl ) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,"%s",buff );
    }
}

void cls_log::log_history_init()
{
    int             indx;
    ctx_log_item    log_item;

    log_item.log_msg= "";
    for (indx=0;indx<200;indx++){
        log_item.log_nbr = indx;
        log_vec.push_back(log_item);
    }
}

void cls_log::log_history_add(std::string msg)
{
    int indx, mx;

    mx = (int)log_vec.size();
    for (indx=0;indx<mx-1;indx++) {
        log_vec[indx].log_nbr = log_vec[indx+1].log_nbr;
        log_vec[indx].log_msg = log_vec[indx+1].log_msg;
    }

    /*Arbritrary large number*/
    if (log_vec[mx-1].log_nbr >50000000L) {
        log_history_init();
    }
    log_vec[mx-1].log_nbr++;
    log_vec[mx-1].log_msg = msg;

}

void cls_log::write_flood(int loglvl)
{
    char flood_repeats[1024];

    if (flood_cnt <= 1) {
        return;
    }

    snprintf(flood_repeats, sizeof(flood_repeats)
        , "%s Above message repeats %d times\n"
        , msg_prefix, flood_cnt-1);

    if (log_mode == LOGMODE_FILE) {
        fputs(flood_repeats, log_file_ptr);
        fflush(log_file_ptr);

    } else {    /* The syslog level values are one less*/
        syslog(loglvl-1, "%s", flood_repeats);
        fputs(flood_repeats, stderr);
        fflush(stderr);
    }
    log_history_add(flood_repeats);
}

void cls_log::write_norm(int loglvl, uint prefixlen)
{
    flood_cnt = 1;

    if (snprintf(msg_flood, sizeof(msg_flood), "%s", &msg_full[prefixlen]) < 0) {
        return;
    }
    if (snprintf(msg_prefix, prefixlen, "%s", msg_full) < 0) {
        return;
    }

    if (log_mode == LOGMODE_FILE) {
        strcpy(msg_full + strlen(msg_full),"\n");
        fputs(msg_full, log_file_ptr);
        fflush(log_file_ptr);
    } else {
        syslog(loglvl-1, "%s", msg_full);
        strcpy(msg_full + strlen(msg_full),"\n");
        fputs(msg_full, stderr);
        fflush(stderr);
    }
    log_history_add(msg_full);
}

void cls_log::add_errmsg(int flgerr, int err_save)
{
    size_t errsz, msgsz;
    char err_buf[90];

    if (flgerr == NO_ERRNO) {
        return;
    }

    memset(err_buf, 0, sizeof(err_buf));
    #if not defined(_GNU_SOURCE) /* XSI-compliant strerror_r() */
        (void)strerror_r(err_save, err_buf, sizeof(err_buf));
    #else /* GNU-specific strerror_r() */
        (void)snprintf(err_buf, sizeof(err_buf),"%s"
            , strerror_r(err_save, err_buf, sizeof(err_buf)));
    #endif
    errsz = strlen(err_buf);
    msgsz = strlen(msg_full);

    if ((msgsz+errsz+2) >= sizeof(msg_full)) {
        msgsz = msgsz-errsz-2;
        memset(msg_full+msgsz, 0, sizeof(msg_full) - msgsz);
    }
    strcpy(msg_full+msgsz,": ");
    memcpy(msg_full+msgsz + 2, err_buf, errsz);

}

void cls_log::set_mode(int mode_new)
{
    if ((log_mode != LOGMODE_SYSLOG) && (mode_new == LOGMODE_SYSLOG)) {
        openlog("motion", LOG_PID, LOG_USER);
    }
    if ((log_mode == LOGMODE_SYSLOG) && (mode_new != LOGMODE_SYSLOG)) {
        closelog();
    }
    log_mode = mode_new;
}

void cls_log::set_log_file(std::string pname)
{
    if ((pname == "") || (pname == "syslog")) {
        if (log_file_ptr != nullptr) {
            myfclose(log_file_ptr);
            log_file_ptr = nullptr;
        }
        if (log_file_name == "") {
            set_mode(LOGMODE_SYSLOG);
            log_file_name = "syslog";
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "Logging to syslog");
        }

    } else if ((pname != log_file_name) || (log_file_ptr == nullptr)) {
        if (log_file_ptr != nullptr) {
            myfclose(log_file_ptr);
            log_file_ptr = nullptr;
        }
        log_file_ptr = myfopen(pname.c_str(), "ae");
        if (log_file_ptr != nullptr) {
            log_file_name = pname;
            set_mode(LOGMODE_SYSLOG);
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "Logging to file (%s)"
                ,pname.c_str());
            set_mode(LOGMODE_FILE);
        } else {
            log_file_name = "syslog";
            set_mode(LOGMODE_SYSLOG);
            MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO, "Cannot create log file %s"
                , pname.c_str());
        }
    }
}

void cls_log::write_msg(int loglvl, int msg_type, int flgerr, int flgfnc, ...)
{
    int err_save, n;
    uint prefixlen;
    std::string usrfmt;
    char msg_time[32];
    char threadname[32];
    va_list ap;
    time_t now;

    if (loglvl > log_level) {
        return;
    }

    pthread_mutex_lock(&mutex_log);

    err_save = errno;
    memset(msg_full, 0, sizeof(msg_full));

    mythreadname_get(threadname);

    now = time(NULL);
    strftime(msg_time, sizeof(msg_time)
        , "%b %d %H:%M:%S", localtime(&now));

    if (log_mode == LOGMODE_FILE) {
        n = snprintf(msg_full, sizeof(msg_full)
            , "%s [%s][%s][%s] ", msg_time
            , log_level_str[loglvl],log_type_str[msg_type], threadname );
    } else {
        n = snprintf(msg_full, sizeof(msg_full)
        , "[%s][%s][%s] "
        , log_level_str[loglvl],log_type_str[msg_type], threadname );
    }
    prefixlen = (uint)n;

    /* flgfnc must be an int.  Bool has compile error*/
    va_start(ap, flgfnc);
        usrfmt = va_arg(ap, char *);
        if (flgfnc == 1) {
            usrfmt.append(": ").append(va_arg(ap, char *));
        }
        n += vsnprintf(msg_full + n
            , sizeof(msg_full) - (uint)n - 1
            , usrfmt.c_str(), ap);
    va_end(ap);

    add_errmsg(flgerr, err_save);

    if ((flood_cnt <= 5000) &&
        mystreq(msg_flood, &msg_full[prefixlen])) {
        flood_cnt++;
        pthread_mutex_unlock(&mutex_log);
        return;
    }

    write_flood(loglvl);

    write_norm(loglvl, prefixlen);

    pthread_mutex_unlock(&mutex_log);

}

void cls_log::shutdown()
{
    if (log_file_ptr != nullptr) {
        myfclose(log_file_ptr);
        log_file_ptr = nullptr;
    }
}

void cls_log::startup()
{
    motlog->log_level = app->cfg->log_level;
    motlog->log_fflevel = app->cfg->log_fflevel;
    motlog->set_log_file(app->cfg->log_file);
}

cls_log::cls_log(cls_motapp *p_app)
{
    app = p_app;
    log_mode = LOGMODE_NONE;
    log_level = LEVEL_DEFAULT;
    log_fflevel = 4;
    log_file_ptr  = nullptr;
    log_file_name = "";
    flood_cnt = 0;
    restart = false;
    set_mode(LOGMODE_SYSLOG);
    pthread_mutex_init(&mutex_log, NULL);
    memset(msg_prefix,0,sizeof(msg_prefix));
    memset(msg_flood,0,sizeof(msg_flood));
    memset(msg_full,0,sizeof(msg_full));
    log_history_init();
    av_log_set_callback(ff_log);
}

cls_log::~cls_log()
{
    shutdown();
    pthread_mutex_destroy(&mutex_log);
}


