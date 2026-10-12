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
 *
*/

#include "motion.hpp"
#include "util.hpp"
#include "i10n.hpp"
#include "logger.hpp"

static bool nls_enabled = true;

void i10n_locale_chg(std::string langcd)
{
    #ifdef HAVE_GETTEXT
        char *rslt, *curr;
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("setting language code to %s"),langcd.c_str());
        curr = setlocale (LC_ALL, NULL);
        if (langcd == "da")         {rslt = setlocale (LC_ALL, "da_DK.utf8");
        } else if (langcd == "de")  {rslt = setlocale (LC_ALL, "de_DE.utf8");
        } else if (langcd == "es")  {rslt = setlocale (LC_ALL, "es_ES.utf8");
        } else if (langcd == "en")  {rslt = setlocale (LC_ALL, "en_US.utf8");
        } else if (langcd == "fi")  {rslt = setlocale (LC_ALL, "fi_FI.utf8");
        } else if (langcd == "fr")  {rslt = setlocale (LC_ALL, "fr_FR.utf8");
        } else if (langcd == "it")  {rslt = setlocale (LC_ALL, "it_IT.utf8");
        } else if (langcd == "ja")  {rslt = setlocale (LC_ALL, "ja_JP.utf8");
        } else if (langcd == "ko")  {rslt = setlocale (LC_ALL, "ko_KR.utf8");
        } else if (langcd == "nl")  {rslt = setlocale (LC_ALL, "nl_NL.utf8");
        } else if (langcd == "no")  {rslt = setlocale (LC_ALL, "no_NO.utf8");
        } else if (langcd == "pt")  {rslt = setlocale (LC_ALL, "pt_PT.utf8");
        } else if (langcd == "sv")  {rslt = setlocale (LC_ALL, "sv_SE.utf8");
        } else if (langcd == "zh")  {rslt = setlocale (LC_ALL, "zh_CN.utf8");
        } else if (langcd == "")    {rslt = setlocale (LC_ALL, "");
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("language code %s not supported for override")
                ,langcd.c_str());
            rslt = setlocale (LC_ALL, "");
        }
        if (rslt == nullptr) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("unable to set locale for %s"),langcd.c_str());
        } else {
            if (curr != nullptr) {
                if(std::string(rslt) != std::string(curr)) {
                    i10n_log();
                }
            }
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("set locale to %s"),rslt);
        }
        ++_nl_msg_cat_cntr;

        return;

        /* This section is for development testing only.  It is not used for
        * regular users because once this locale is change, it changes the
        * whole computer over to the new locale.
        */

        setenv ("LANGUAGE", langcd.c_str(), 1);
        /* Invoke external function to change locale*/
        ++_nl_msg_cat_cntr;
    #else
        if (langcd != "") {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,"No native language support");
        }
    #endif
}

void i10n_init(std::string langcd)
{
    #ifdef HAVE_GETTEXT
        setlocale (LC_ALL, "");
        i10n_set(true);
        i10n_locale_chg(langcd);
        bindtextdomain ("motion", LOCALEDIR);
        bind_textdomain_codeset ("motion", "UTF-8");
        textdomain ("motion");

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Language: English"));
        return;

    #else
        /* Disable native language support */
        i10n_set(false);

        /* This avoids a unused function warning */
        i10n_locale_chg("");
    #endif
}

void i10n_set(bool setnls)
{
    if (setnls == true) {
        if (nls_enabled == false) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Enabling native language support"));
            i10n_log();
        }
        nls_enabled = true;
    } else {
        if (nls_enabled == true) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,_("Disabling native language support"));
        }
        nls_enabled = false;
    }
}

std::string i10n_text(std::string msgid)
{
    std::string tmp;

    if (msgid == "") {
        return "";
    }
    #ifdef HAVE_GETTEXT
        if (nls_enabled) {
            tmp = (char*)gettext(msgid.c_str());
            return tmp;
        } else {
            return msgid;
        }
    #else
        return msgid;
    #endif
}

char* i10n_text(const char *msgid)
{
    if (msgid == nullptr) {
        return nullptr;
    }
    #ifdef HAVE_GETTEXT
        if (nls_enabled) {
            return (char*)gettext(msgid);
        } else {
            return (char*)msgid;
        }
    #else
        return (char*)msgid;
    #endif
}

void i10n_log()
{
    bool showmsg = false;

    #ifdef SHOWI10N
        showmsg = true;
    #endif

    if (showmsg == false) {
        return;
    }

    i10n_config_names();
    i10n_config_options();
}

void i10n_config_names()
{
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","daemon",_("daemon"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pid_file",_("pid_file"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_file",_("log_file"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_level",_("log_level"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_fflevel",_("log_fflevel"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_type",_("log_type"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","native_language",_("native_language"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","native_language_code",_("native_language_code"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","device_name",_("device_name"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","device_id",_("device_id"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","device_tmo",_("device_tmo"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pause",_("pause"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","schedule_params",_("schedule_params"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","cleandir_params",_("cleandir_params"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","config_dir",_("config_dir"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","target_dir",_("target_dir"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","watchdog_tmo",_("watchdog_tmo"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","watchdog_kill",_("watchdog_kill"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","v4l2_device",_("v4l2_device"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","v4l2_params",_("v4l2_params"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_url",_("netcam_url"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_params",_("netcam_params"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_high_url",_("netcam_high_url"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_high_params",_("netcam_high_params"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_userpass",_("netcam_userpass"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","libcam_device",_("libcam_device"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","libcam_params",_("libcam_params"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","width",_("width"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","height",_("height"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","framerate",_("framerate"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","rotate",_("rotate"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","flip_axis",_("flip_axis"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","locate_motion_mode",_("locate_motion_mode"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","locate_motion_style",_("locate_motion_style"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_left",_("text_left"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_right",_("text_right"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_changes",_("text_changes"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_scale",_("text_scale"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_event",_("text_event"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","emulate_motion",_("emulate_motion"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold",_("threshold"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_maximum",_("threshold_maximum"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_tune",_("threshold_tune"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_method",_("secondary_method"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_params",_("secondary_params"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","noise_level",_("noise_level"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","noise_tune",_("noise_tune"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","despeckle_filter",_("despeckle_filter"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","area_detect",_("area_detect"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mask_file",_("mask_file"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mask_privacy",_("mask_privacy"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","smart_mask_speed",_("smart_mask_speed"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","lightswitch_percent",_("lightswitch_percent"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","lightswitch_frames",_("lightswitch_frames"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","minimum_motion_frames",_("minimum_motion_frames"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","static_object_time",_("static_object_time"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","event_gap",_("event_gap"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pre_capture",_("pre_capture"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","post_capture",_("post_capture"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_event_start",_("on_event_start"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_event_end",_("on_event_end"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_picture_save",_("on_picture_save"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_area_detected",_("on_area_detected"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_motion_detected",_("on_motion_detected"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_movie_start",_("on_movie_start"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_movie_end",_("on_movie_end"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_camera_lost",_("on_camera_lost"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_camera_found",_("on_camera_found"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_secondary_detect",_("on_secondary_detect"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_action_user",_("on_action_user"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_sound_alert",_("on_sound_alert"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_output",_("picture_output"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_output_motion",_("picture_output_motion"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_type",_("picture_type"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_quality",_("picture_quality"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_exif",_("picture_exif"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_filename",_("picture_filename"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snapshot_interval",_("snapshot_interval"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snapshot_filename",_("snapshot_filename"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_output",_("movie_output"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_output_motion",_("movie_output_motion"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_max_time",_("movie_max_time"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_bps",_("movie_bps"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_quality",_("movie_quality"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_container",_("movie_container"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_passthrough",_("movie_passthrough"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_filename",_("movie_filename"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_retain",_("movie_retain"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_all_frames",_("movie_all_frames"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_extpipe_use",_("movie_extpipe_use"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_extpipe",_("movie_extpipe"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_interval",_("timelapse_interval"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_mode",_("timelapse_mode"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_fps",_("timelapse_fps"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_container",_("timelapse_container"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_filename",_("timelapse_filename"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","video_pipe",_("video_pipe"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","video_pipe_motion",_("video_pipe_motion"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_port",_("webcontrol_port"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_base_path",_("webcontrol_base_path"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_ipv6",_("webcontrol_ipv6"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_localhost",_("webcontrol_localhost"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_access",_("webcontrol_access"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_interface",_("webcontrol_interface"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_auth_method",_("webcontrol_auth_method"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_auth_admin",_("webcontrol_auth_admin"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_auth_user",_("webcontrol_auth_user"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_tls",_("webcontrol_tls"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_cert",_("webcontrol_cert"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_key",_("webcontrol_key"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_headers",_("webcontrol_headers"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_html",_("webcontrol_html"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_actions",_("webcontrol_actions"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_lock_minutes",_("webcontrol_lock_minutes"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_lock_attempts",_("webcontrol_lock_attempts"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_lock_script",_("webcontrol_lock_script"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_allcam_params",_("stream_allcam_params"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_quality",_("stream_quality"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_grey",_("stream_grey"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_motion",_("stream_motion"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_maxrate",_("stream_maxrate"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_type",_("database_type"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_dbname",_("database_dbname"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_host",_("database_host"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_port",_("database_port"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_user",_("database_user"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_password",_("database_password"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_busy_timeout",_("database_busy_timeout"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_event_start",_("sql_event_start"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_event_end",_("sql_event_end"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_movie_start",_("sql_movie_start"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_movie_end",_("sql_movie_end"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_pic_save",_("sql_pic_save"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_auto_track",_("ptz_auto_track"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_wait",_("ptz_wait"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_move_track",_("ptz_move_track"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_pan_left",_("ptz_pan_left"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_pan_right",_("ptz_pan_right"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_tilt_up",_("ptz_tilt_up"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_tilt_down",_("ptz_tilt_down"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_zoom_in",_("ptz_zoom_in"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_zoom_out",_("ptz_zoom_out"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_device",_("snd_device"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_params",_("snd_params"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_alerts",_("snd_alerts"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_window",_("snd_window"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_show",_("snd_show"));
}

void i10n_config_options()
{
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("schedule"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("on"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("off"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("none"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("vertical"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("horizontal"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("off"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("on"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("preview"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("box"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("redbox"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("cross"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("redcross"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("on"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("off"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("first"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("best"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("center"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("on"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("off"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("roi"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("all"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("secondary"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("hourly"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("daily"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("weekly-sunday"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("weekly-monday"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("monthly"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("manual"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("default"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("stream"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("none"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("basic"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("digest"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("params_file"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("detection"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Detection on/off(checked/unchecked)"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("action"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("pause"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("stop"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("sun"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Sunday"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("mon"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Monday"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("tue"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Tuesday"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("wed"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Wednesday"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("thu"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Thursday"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("fri"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Friday"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("sat"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Saturday"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("action"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("delete"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("script"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("freq"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("hourly"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("daily"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("weekly"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("runtime"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("dur_unit"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("m"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("h"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("d"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("w"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("duration units(minutes,hours,days,weeks)"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("dur_val"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("duration value (expressed in duration units)"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("removedir"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Remove directory"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("script"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("script to execute"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("input"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("palette"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("norm"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("frequency"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("decoder"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("capture_rate"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("interrupt"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("pixelformat"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Orientation"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Rotate0"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Rotate0Mirror"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Rotate180"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Rotate180Mirror"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Rotate90"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Rotate90Mirror"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Rotate270"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Rotate270Mirror"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AeConstraintMode"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Normal"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Highlight"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("2-Shadows"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("3-Custom"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AeEnable"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AeExposureMode"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Normal"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Short"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("2-Long"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("3-Custom"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AeFlickerMode"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Off"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Manual"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("2-Auto"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AeFlickerPeriod"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AeMeteringMode"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-CentreWeighted"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Spot"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("3-Matrix"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("4-Custom"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AeState"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Idle"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Searching"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("2-Converged"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AfMetering"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Auto"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Windows"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AfMode"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Manual"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Auto"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("2-Continuous"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AfPause"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Immediate"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Deferred"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("2-Resume"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AfPauseState"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Running"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Pausing"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("2-Paused"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AfRange"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Normal"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Macro"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("2-Full"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AfSpeed"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Normal"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Fast"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AfState"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Idle"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Scanning"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("2-Focused"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("3-Failed"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AfTrigger"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Start"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Cancel"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AfWindows"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Pipe delimited as x|y|h|w"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AnalogueGain"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AnalogueGainMode"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Auto"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Manual"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AwbEnable"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AwbLocked"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("AwbMode"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Auto"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Incandescent"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("2-Tungsten"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("3-Fluorescent"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("4-Indoor"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("5-Daylight"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("6-Cloudy"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("7-Custom"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Brightness"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("ColourCorrectionMatrix"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Pipe delimited as var1|var2|...|var8|var9"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("ColourGains"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Pipe delimited as Red|Blue)"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("ColourTemperature"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Contrast"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("DigitalGain"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("ExposureTime"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("ExposureTimeMode"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Auto"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Manual"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("ExposureValue"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("FocusFoM"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("FrameDuration"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("FrameDurationLimits"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Pipe delimited as min|max"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Gamma"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("HdrChannel"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-None"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Short"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("2-Medium"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("3-Long"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("HdrMode"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Off"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-MultiExposureUnmerged"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("2-MultiExposure"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("3-SingleExposure"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("4-Night"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Hue"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("LensPosition"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Lux"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Saturation"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("ScalerCrop"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Pipe delimited as x|y|h|w"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("SensorBlackLevels"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Pipe delimited as var1|var2|var3|var4"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("SensorTemperature"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("SensorTimestamp"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("Sharpness"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("WdrMode"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("0-Off"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("1-Linear"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("2-Power"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("3-Exponential"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("4-HistogramEqualization"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("frame_interval"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("model_file"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("image_type"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("full"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("grey"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("roi"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("rotate"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("threshold"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("scalefactor"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("minsize"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("maxsize"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("padding"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("threshold_model"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("winstride"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("flags"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("minneighbors"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("config"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("classes_file"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("framework"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("backend"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("target"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("scale"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("width"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("height"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("scale"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("row"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("col"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("offset_row"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("offset_col"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("pause"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("event"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("snapshot"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("camera_add"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("camera_delete"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("ptz"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("stop"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("restart"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("action_user"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("config_write"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("config"));
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s",_("movies"));

}
