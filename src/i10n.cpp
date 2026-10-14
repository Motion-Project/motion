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
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","daemon",_("daemon"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pid_file",_("pid_file"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_file",_("log_file"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_level",_("log_level"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_fflevel",_("log_fflevel"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_type",_("log_type"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","native_language",_("native_language"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","native_language_code",_("native_language_code"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","device_name",_("device_name"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","device_id",_("device_id"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","device_tmo",_("device_tmo"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pause",_("pause"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","schedule_params",_("schedule_params"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","cleandir_params",_("cleandir_params"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","config_dir",_("config_dir"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","target_dir",_("target_dir"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","watchdog_tmo",_("watchdog_tmo"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","watchdog_kill",_("watchdog_kill"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","v4l2_device",_("v4l2_device"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","v4l2_params",_("v4l2_params"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_url",_("netcam_url"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_params",_("netcam_params"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_high_url",_("netcam_high_url"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_high_params",_("netcam_high_params"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_userpass",_("netcam_userpass"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","libcam_device",_("libcam_device"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","libcam_params",_("libcam_params"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","width",_("width"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","height",_("height"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","framerate",_("framerate"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","rotate",_("rotate"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","flip_axis",_("flip_axis"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","locate_motion_mode",_("locate_motion_mode"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","locate_motion_style",_("locate_motion_style"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_left",_("text_left"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_right",_("text_right"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_changes",_("text_changes"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_scale",_("text_scale"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_event",_("text_event"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","emulate_motion",_("emulate_motion"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold",_("threshold"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_maximum",_("threshold_maximum"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_tune",_("threshold_tune"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_method",_("secondary_method"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_params",_("secondary_params"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","noise_level",_("noise_level"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","noise_tune",_("noise_tune"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","despeckle_filter",_("despeckle_filter"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","area_detect",_("area_detect"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mask_file",_("mask_file"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mask_privacy",_("mask_privacy"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","smart_mask_speed",_("smart_mask_speed"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","lightswitch_percent",_("lightswitch_percent"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","lightswitch_frames",_("lightswitch_frames"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","minimum_motion_frames",_("minimum_motion_frames"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","static_object_time",_("static_object_time"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","event_gap",_("event_gap"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pre_capture",_("pre_capture"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","post_capture",_("post_capture"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_event_start",_("on_event_start"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_event_end",_("on_event_end"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_picture_save",_("on_picture_save"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_area_detected",_("on_area_detected"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_motion_detected",_("on_motion_detected"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_movie_start",_("on_movie_start"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_movie_end",_("on_movie_end"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_camera_lost",_("on_camera_lost"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_camera_found",_("on_camera_found"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_secondary_detect",_("on_secondary_detect"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_action_user",_("on_action_user"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_sound_alert",_("on_sound_alert"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_output",_("picture_output"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_output_motion",_("picture_output_motion"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_type",_("picture_type"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_quality",_("picture_quality"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_exif",_("picture_exif"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_filename",_("picture_filename"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snapshot_interval",_("snapshot_interval"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snapshot_filename",_("snapshot_filename"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_output",_("movie_output"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_output_motion",_("movie_output_motion"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_max_time",_("movie_max_time"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_bps",_("movie_bps"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_quality",_("movie_quality"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_container",_("movie_container"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_passthrough",_("movie_passthrough"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_filename",_("movie_filename"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_retain",_("movie_retain"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_all_frames",_("movie_all_frames"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_extpipe_use",_("movie_extpipe_use"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_extpipe",_("movie_extpipe"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_interval",_("timelapse_interval"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_mode",_("timelapse_mode"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_fps",_("timelapse_fps"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_container",_("timelapse_container"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_filename",_("timelapse_filename"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","video_pipe",_("video_pipe"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","video_pipe_motion",_("video_pipe_motion"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_port",_("webcontrol_port"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_base_path",_("webcontrol_base_path"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_ipv6",_("webcontrol_ipv6"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_localhost",_("webcontrol_localhost"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_access",_("webcontrol_access"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_interface",_("webcontrol_interface"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_auth_method",_("webcontrol_auth_method"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_auth_admin",_("webcontrol_auth_admin"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_auth_user",_("webcontrol_auth_user"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_tls",_("webcontrol_tls"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_cert",_("webcontrol_cert"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_key",_("webcontrol_key"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_headers",_("webcontrol_headers"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_html",_("webcontrol_html"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_actions",_("webcontrol_actions"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_lock_minutes",_("webcontrol_lock_minutes"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_lock_attempts",_("webcontrol_lock_attempts"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_lock_script",_("webcontrol_lock_script"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_allcam_params",_("stream_allcam_params"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_quality",_("stream_quality"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_grey",_("stream_grey"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_motion",_("stream_motion"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_maxrate",_("stream_maxrate"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_type",_("database_type"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_dbname",_("database_dbname"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_host",_("database_host"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_port",_("database_port"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_user",_("database_user"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_password",_("database_password"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_busy_timeout",_("database_busy_timeout"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_event_start",_("sql_event_start"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_event_end",_("sql_event_end"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_movie_start",_("sql_movie_start"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_movie_end",_("sql_movie_end"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_pic_save",_("sql_pic_save"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_auto_track",_("ptz_auto_track"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_wait",_("ptz_wait"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_move_track",_("ptz_move_track"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_pan_left",_("ptz_pan_left"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_pan_right",_("ptz_pan_right"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_tilt_up",_("ptz_tilt_up"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_tilt_down",_("ptz_tilt_down"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_zoom_in",_("ptz_zoom_in"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_zoom_out",_("ptz_zoom_out"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_device",_("snd_device"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_params",_("snd_params"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_alerts",_("snd_alerts"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_window",_("snd_window"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_show",_("snd_show"));
}

void i10n_config_options()
{
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","schedule",_("schedule"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on",_("on"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","off",_("off"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","none",_("none"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","vertical",_("vertical"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","horizontal",_("horizontal"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","off",_("off"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on",_("on"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","preview",_("preview"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","box",_("box"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","redbox",_("redbox"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","cross",_("cross"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","redcross",_("redcross"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on",_("on"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","off",_("off"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","first",_("first"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","best",_("best"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","center",_("center"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s",_("on"),_("on"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","off",_("off"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","roi",_("roi"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","all",_("all"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary",_("secondary"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","hourly",_("hourly"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","daily",_("daily"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","weekly-sunday",_("weekly-sunday"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","weekly-monday",_("weekly-monday"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","monthly",_("monthly"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","manual",_("manual"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","default",_("default"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream",_("stream"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","none",_("none"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","basic",_("basic"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","digest",_("digest"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","params_file",_("params_file"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","detection",_("detection"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Detection on/off(checked/unchecked)",_("Detection on/off(checked/unchecked)"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","action",_("action"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pause",_("pause"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stop",_("stop"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sun",_("sun"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Sunday",_("Sunday"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mon",_("mon"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Monday",_("Monday"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","tue",_("tue"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Tuesday",_("Tuesday"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","wed",_("wed"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Wednesday",_("Wednesday"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","thu",_("thu"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Thursday",_("Thursday"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","fri",_("fri"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Friday",_("Friday"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sat",_("sat"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Saturday",_("Saturday"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","action",_("action"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","delete",_("delete"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","script",_("script"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","freq",_("freq"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","hourly",_("hourly"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","daily",_("daily"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","weekly",_("weekly"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","runtime",_("runtime"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","dur_unit",_("dur_unit"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","m",_("m"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","h",_("h"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","d",_("d"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","w",_("w"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","duration units(minutes,hours,days,weeks)",_("duration units(minutes,hours,days,weeks)"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","dur_val",_("dur_val"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","duration value (expressed in duration units)",_("duration value (expressed in duration units)"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","removedir",_("removedir"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Remove directory",_("Remove directory"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","script",_("script"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","script to execute",_("script to execute"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","input",_("input"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","palette",_("palette"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","norm",_("norm"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","frequency",_("frequency"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","decoder",_("decoder"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","capture_rate",_("capture_rate"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","interrupt",_("interrupt"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pixelformat",_("pixelformat"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Orientation",_("Orientation"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Rotate0",_("Rotate0"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Rotate0Mirror",_("Rotate0Mirror"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Rotate180",_("Rotate180"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Rotate180Mirror",_("Rotate180Mirror"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Rotate90",_("Rotate90"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Rotate90Mirror",_("Rotate90Mirror"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Rotate270",_("Rotate270"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Rotate270Mirror",_("Rotate270Mirror"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AeConstraintMode",_("AeConstraintMode"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Normal",_("0-Normal"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Highlight",_("1-Highlight"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","2-Shadows",_("2-Shadows"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","3-Custom",_("3-Custom"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AeEnable",_("AeEnable"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AeExposureMode",_("AeExposureMode"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Normal",_("0-Normal"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Short",_("1-Short"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","2-Long",_("2-Long"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","3-Custom",_("3-Custom"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AeFlickerMode",_("AeFlickerMode"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Off",_("0-Off"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Manual",_("1-Manual"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","2-Auto",_("2-Auto"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AeFlickerPeriod",_("AeFlickerPeriod"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AeMeteringMode",_("AeMeteringMode"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-CentreWeighted",_("0-CentreWeighted"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Spot",_("1-Spot"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","2-Matrix",_("2-Matrix"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","3-Custom",_("3-Custom"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AeState",_("AeState"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Idle",_("0-Idle"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Searching",_("1-Searching"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","2-Converged",_("2-Converged"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AfMetering",_("AfMetering"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Auto",_("0-Auto"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Windows",_("1-Windows"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AfMode",_("AfMode"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Manual",_("0-Manual"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Auto",_("1-Auto"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","2-Continuous",_("2-Continuous"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AfPause",_("AfPause"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Immediate",_("0-Immediate"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Deferred",_("1-Deferred"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","2-Resume",_("2-Resume"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AfPauseState",_("AfPauseState"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Running",_("0-Running"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Pausing",_("1-Pausing"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","2-Paused",_("2-Paused"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AfRange",_("AfRange"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Normal",_("0-Normal"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Macro",_("1-Macro"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","2-Full",_("2-Full"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AfSpeed",_("AfSpeed"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Normal",_("0-Normal"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Fast",_("1-Fast"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AfState",_("AfState"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Idle",_("0-Idle"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Scanning",_("1-Scanning"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","2-Focused",_("2-Focused"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","3-Failed",_("3-Failed"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AfTrigger",_("AfTrigger"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Start",_("0-Start"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Cancel",_("1-Cancel"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AfWindows",_("AfWindows"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Pipe delimited as x|y|h|w",_("Pipe delimited as x|y|h|w"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AnalogueGain",_("AnalogueGain"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AnalogueGainMode",_("AnalogueGainMode"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Auto",_("0-Auto"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Manual",_("1-Manual"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AwbEnable",_("AwbEnable"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AwbLocked",_("AwbLocked"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","AwbMode",_("AwbMode"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Auto",_("0-Auto"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Incandescent",_("1-Incandescent"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","2-Tungsten",_("2-Tungsten"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","3-Fluorescent",_("3-Fluorescent"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","4-Indoor",_("4-Indoor"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","5-Daylight",_("5-Daylight"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","6-Cloudy",_("6-Cloudy"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","7-Custom",_("7-Custom"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Brightness",_("Brightness"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ColourCorrectionMatrix",_("ColourCorrectionMatrix"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Pipe delimited as var1|var2|...|var8|var9",_("Pipe delimited as var1|var2|...|var8|var9"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ColourGains",_("ColourGains"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Pipe delimited as Red|Blue)",_("Pipe delimited as Red|Blue)"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ColourTemperature",_("ColourTemperature"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Contrast",_("Contrast"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","DigitalGain",_("DigitalGain"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ExposureTime",_("ExposureTime"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ExposureTimeMode",_("ExposureTimeMode"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Auto",_("0-Auto"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Manual",_("1-Manual"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ExposureValue",_("ExposureValue"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","FocusFoM",_("FocusFoM"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","FrameDuration",_("FrameDuration"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","FrameDurationLimits",_("FrameDurationLimits"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Pipe delimited as min|max",_("Pipe delimited as min|max"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Gamma",_("Gamma"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","HdrChannel",_("HdrChannel"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-None",_("0-None"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Short",_("1-Short"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","2-Medium",_("2-Medium"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","3-Long",_("3-Long"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","HdrMode",_("HdrMode"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Off",_("0-Off"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-MultiExposureUnmerged",_("1-MultiExposureUnmerged"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","2-MultiExposure",_("2-MultiExposure"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","3-SingleExposure",_("3-SingleExposure"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","4-Night",_("4-Night"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Hue",_("Hue"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","LensPosition",_("LensPosition"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Lux",_("Lux"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Saturation",_("Saturation"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ScalerCrop",_("ScalerCrop"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Pipe delimited as x|y|h|w",_("Pipe delimited as x|y|h|w"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","SensorBlackLevels",_("SensorBlackLevels"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Pipe delimited as var1|var2|var3|var4",_("Pipe delimited as var1|var2|var3|var4"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","SensorTemperature",_("SensorTemperature"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","SensorTimestamp",_("SensorTimestamp"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","Sharpness",_("Sharpness"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","WdrMode",_("WdrMode"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","0-Off",_("0-Off"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","1-Linear",_("1-Linear"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","2-Power",_("2-Power"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","3-Exponential",_("3-Exponential"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","4-HistogramEqualization",_("4-HistogramEqualization"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","frame_interval",_("frame_interval"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","model_file",_("model_file"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","image_type",_("image_type"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","full",_("full"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","grey",_("grey"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","roi",_("roi"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","rotate",_("rotate"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold",_("threshold"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","scalefactor",_("scalefactor"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","minsize",_("minsize"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","maxsize",_("maxsize"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","padding",_("padding"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_model",_("threshold_model"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","winstride",_("winstride"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","flags",_("flags"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","minneighbors",_("minneighbors"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","config",_("config"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","classes_file",_("classes_file"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","framework",_("framework"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","backend",_("backend"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","target",_("target"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","scale",_("scale"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","width",_("width"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","height",_("height"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","scale",_("scale"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","row",_("row"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","col",_("col"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","offset_row",_("offset_row"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","offset_col",_("offset_col"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pause",_("pause"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","event",_("event"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snapshot",_("snapshot"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera_add",_("camera_add"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera_delete",_("camera_delete"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz",_("ptz"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stop",_("stop"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","restart",_("restart"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","action_user",_("action_user"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","config_write",_("config_write"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","config",_("config"));
    MOTION_SHT(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movies",_("movies"));

}
