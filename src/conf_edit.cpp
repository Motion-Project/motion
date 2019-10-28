/*    conf_system.cpp
 *
 *    This file is part of the Motion application
 *    Copyright (C) 2019  Motion-Project Developers(motion-project.github.io)
 *
 *    This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Library General Public
 *    License as published by the Free Software Foundation; either
 *    version 2 of the License, or (at your option) any later version.
 *
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Library General Public License for more details.
 *
 *    You should have received a copy of the GNU Library General Public
 *    License along with this library; if not, write to the
 *    Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *    Boston, MA  02110-1301, USA.
*/

#include <regex.h>
#include <string>
#include "motion.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "conf.hpp"

enum PARM_ACT{
    PARM_ACT_DFLT
    ,PARM_ACT_SET
    ,PARM_ACT_GET
};

static void conf_edit_set_bool(int &parm_dest, std::string &parm_in) {
    if ((parm_in == "1") || (parm_in == "yes") || (parm_in == "on")) {
        parm_dest = TRUE;
    } else {
        parm_dest = FALSE;
    }
}
static void conf_edit_get_bool(std::string &parm_dest, int &parm_in) {
    if (parm_in == TRUE){
        parm_dest = "on";
    } else {
        parm_dest = "off";
    }
}

static void conf_edit_daemon(struct ctx_motapp *motapp, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        motapp->daemon = FALSE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(motapp->daemon, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, motapp->daemon);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","daemon",_("daemon"));
}
static void conf_edit_setup_mode(struct ctx_motapp *motapp, std::string &parm, int pact) {
    if (pact == PARM_ACT_DFLT){
        motapp->setup_mode = FALSE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(motapp->setup_mode, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, motapp->setup_mode);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","setup_mode",_("setup_mode"));
}
static void conf_edit_conf_filename(struct ctx_motapp *motapp, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        motapp->conf_filename = "";
    } else if (pact == PARM_ACT_SET){
        motapp->conf_filename = parm;
    } else if (pact == PARM_ACT_GET){
        parm = motapp->conf_filename;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_file",_("log_file"));
}
static void conf_edit_pid_file(struct ctx_motapp *motapp, std::string &parm, int pact) {
    if (pact == PARM_ACT_DFLT) {
        motapp->pid_file = "";
    } else if (pact == PARM_ACT_SET){
        motapp->pid_file = parm;
    } else if (pact == PARM_ACT_GET){
        parm = motapp->pid_file;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pid_file",_("pid_file"));
}
static void conf_edit_log_file(struct ctx_motapp *motapp, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        motapp->log_file = "";
    } else if (pact == PARM_ACT_SET){
        motapp->log_file = parm;
    } else if (pact == PARM_ACT_GET){
        parm = motapp->log_file;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_file",_("log_file"));
}
static void conf_edit_log_level(struct ctx_motapp *motapp, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        motapp->log_level = 6;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 9)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid log_level %d"),parm_in);
        } else {
            motapp->log_level = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(motapp->log_level);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_level",_("log_level"));
}
static void conf_edit_log_type(struct ctx_motapp *motapp, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        motapp->log_type_str = "ALL";
    } else if (pact == PARM_ACT_SET){
        if ((parm == "ALL") || (parm == "COR") ||
            (parm == "STR") || (parm == "ENC") ||
            (parm == "NET") || (parm == "DBL") ||
            (parm == "EVT") || (parm == "TRK") ||
            (parm == "VID") || (parm == "ALL")) {
            motapp->log_type_str = parm;
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid log_type %s"),parm);
        }
    } else if (pact == PARM_ACT_GET){
        parm = motapp->log_type_str;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_type",_("log_type"));
}
static void conf_edit_native_language(struct ctx_motapp *motapp, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        motapp->native_language = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(motapp->native_language, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, motapp->native_language);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","native_language",_("native_language"));
}

/************************************************************************/
/************************************************************************/
/************************************************************************/

static void conf_edit_quiet(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->quiet = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->quiet, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->quiet);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","quiet",_("quiet"));
}
static void conf_edit_camera_name(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->camera_name= "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->camera_name = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->camera_name;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera_name",_("camera_name"));
}
static void conf_edit_camera_id(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->camera_id = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if (parm_in < 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid camera_id %d"),parm_in);
        } else {
            cam->conf->camera_id = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->camera_id);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera_id",_("camera_id"));
}
static void conf_edit_camera_dir(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->camera_dir = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->camera_dir = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->camera_dir;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera_dir",_("camera_dir"));
}
static void conf_edit_target_dir(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->target_dir = ".";
    } else if (pact == PARM_ACT_SET){
        cam->conf->target_dir = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->target_dir;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","target_dir",_("target_dir"));
}
static void conf_edit_videodevice(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->videodevice = "/dev/video0";
    } else if (pact == PARM_ACT_SET){
        cam->conf->videodevice = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->videodevice;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","videodevice",_("videodevice"));
}
static void conf_edit_vid_control_params(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->vid_control_params = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->vid_control_params = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->vid_control_params;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","vid_control_params",_("vid_control_params"));
}
static void conf_edit_v4l2_palette(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->v4l2_palette = 17;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in >21)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid v4l2_palette %d"),parm_in);
        } else {
            cam->conf->v4l2_palette = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->v4l2_palette);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","v4l2_palette",_("v4l2_palette"));
}
static void conf_edit_input(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->input = -1;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < -1) || (parm_in > 7)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid input %d"),parm_in);
        } else {
            cam->conf->input = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->input);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","input",_("input"));
}
static void conf_edit_norm(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->norm = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 3)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid norm %d"),parm_in);
        } else {
            cam->conf->norm = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->norm);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","norm",_("norm"));
}
static void conf_edit_frequency(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->frequency = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 999999)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid frequency %d"),parm_in);
        } else {
            cam->conf->frequency = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->frequency);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","frequency",_("frequency"));
}
static void conf_edit_tuner_device(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->tuner_device = "/dev/tuner0";
    } else if (pact == PARM_ACT_SET){
        cam->conf->tuner_device = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->tuner_device;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","tuner_device",_("tuner_device"));
}
static void conf_edit_roundrobin_frames(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->roundrobin_frames = 1;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid roundrobin_frames %d"),parm_in);
        } else {
            cam->conf->roundrobin_frames = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->roundrobin_frames);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","roundrobin_frames",_("roundrobin_frames"));
}
static void conf_edit_roundrobin_skip(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->roundrobin_skip = 1;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid roundrobin_skip %d"),parm_in);
        } else {
            cam->conf->roundrobin_skip = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->roundrobin_skip);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","roundrobin_skip",_("roundrobin_skip"));
}
static void conf_edit_roundrobin_switchfilter(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->roundrobin_switchfilter = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->roundrobin_switchfilter, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->roundrobin_switchfilter);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","roundrobin_switchfilter",_("roundrobin_switchfilter"));
}
static void conf_edit_netcam_url(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->netcam_url = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->netcam_url = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->netcam_url;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_url",_("netcam_url"));
}
static void conf_edit_netcam_highres(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->netcam_highres = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->netcam_highres = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->netcam_highres;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_highres",_("netcam_highres"));
}
static void conf_edit_netcam_userpass(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->netcam_userpass = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->netcam_userpass = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->netcam_userpass;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_userpass",_("netcam_userpass"));
}
static void conf_edit_netcam_use_tcp(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->netcam_use_tcp = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->netcam_use_tcp, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->netcam_use_tcp);
    }
    return;
}
static void conf_edit_mmalcam_name(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->mmalcam_name = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->mmalcam_name = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->mmalcam_name;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mmalcam_name",_("mmalcam_name"));
}
static void conf_edit_mmalcam_control_params(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->mmalcam_control_params = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->mmalcam_control_params = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->mmalcam_control_params;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mmalcam_control_params",_("mmalcam_control_params"));
}
static void conf_edit_width(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->width = 640;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 64) || (parm_in > 9999)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid width %d"),parm_in);
        } else if (parm_in % 8) {
            MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("Image width (%d) requested is not modulo 8."), parm_in);
            parm_in = parm_in - (parm_in % 8) + 8;
            MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("Adjusting width to next higher multiple of 8 (%d)."), parm_in);
            cam->conf->width = parm_in;
        } else {
            cam->conf->width = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->width);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","width",_("width"));
}
static void conf_edit_height(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->height = 480;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 64) || (parm_in > 9999)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid height %d"),parm_in);
        } else if (parm_in % 8) {
            MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("Image height (%d) requested is not modulo 8."), parm_in);
            parm_in = parm_in - (parm_in % 8) + 8;
            MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("Adjusting height to next higher multiple of 8 (%d)."), parm_in);
            cam->conf->height = parm_in;
        } else {
            cam->conf->height = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->height);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","height",_("height"));
}
static void conf_edit_framerate(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->framerate = 15;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 2) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid framerate %d"),parm_in);
        } else {
            cam->conf->framerate = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->framerate);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","framerate",_("framerate"));
}
static void conf_edit_minimum_frame_time(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->minimum_frame_time = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid minimum_frame_time %d"),parm_in);
        } else {
            cam->conf->minimum_frame_time = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->minimum_frame_time);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","minimum_frame_time",_("minimum_frame_time"));
}
static void conf_edit_rotate(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->rotate = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in != 0) && (parm_in != 90) &&
            (parm_in != 180) && (parm_in != 270) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid rotate %d"),parm_in);
        } else {
            cam->conf->rotate = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->rotate);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","rotate",_("rotate"));
}
static void conf_edit_flip_axis(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->flip_axis = "none";
    } else if (pact == PARM_ACT_SET){
        if ((parm == "none") || (parm == "v") || (parm == "h")) {
            cam->conf->flip_axis = parm;
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid flip_axis %s"), parm);
        }
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->flip_axis;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","flip_axis",_("flip_axis"));
}
static void conf_edit_locate_motion_mode(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->locate_motion_mode = "off";
    } else if (pact == PARM_ACT_SET){
        if ((parm == "off") || (parm == "on") || (parm == "preview")) {
            cam->conf->locate_motion_mode = parm;
        } else {
          MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid locate_motion_mode %s"), parm);
        }
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->locate_motion_mode;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","locate_motion_mode",_("locate_motion_mode"));
}
static void conf_edit_locate_motion_style(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->locate_motion_style = "box";
    } else if (pact == PARM_ACT_SET){
        if ((parm == "box") || (parm == "redbox") ||
            (parm == "cross") || (parm == "redcross"))  {
            cam->conf->locate_motion_style = parm;
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid locate_motion_style %s"), parm);
        }
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->locate_motion_style;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","locate_motion_style",_("locate_motion_style"));
}
static void conf_edit_text_left(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->text_left = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->text_left = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->text_left;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_left",_("text_left"));
}
static void conf_edit_text_right(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->text_right = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->text_right = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->text_right;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_right",_("text_right"));
}
static void conf_edit_text_changes(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->text_changes = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->text_changes, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->text_changes);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_changes",_("text_changes"));
}
static void conf_edit_text_scale(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->text_scale = 1;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 10)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid text_scale %d"),parm_in);
        } else {
            cam->conf->text_scale = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->text_scale);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_scale",_("text_scale"));
}
static void conf_edit_text_event(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->text_event = "%Y%m%d%H%M%S";
    } else if (pact == PARM_ACT_SET){
        cam->conf->text_event = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->text_event;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_event",_("text_event"));
}

static void conf_edit_emulate_motion(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->emulate_motion = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->emulate_motion, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->emulate_motion);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","emulate_motion",_("emulate_motion"));
}
static void conf_edit_primary_method(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->threshold = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 1)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid primary method %d"),parm_in);
        } else {
            cam->conf->primary_method = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->primary_method);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","primary_method",_("primary_method"));
}
static void conf_edit_threshold(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->threshold = 1500;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold %d"),parm_in);
        } else {
            cam->conf->threshold = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->threshold);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold",_("threshold"));
}
static void conf_edit_threshold_maximum(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->threshold_maximum = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_maximum %d"),parm_in);
        } else {
            cam->conf->threshold_maximum = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->threshold_maximum);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_maximum",_("threshold_maximum"));
}
static void conf_edit_threshold_sdevx(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->threshold_sdevx = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_sdevx %d"),parm_in);
        } else {
            cam->conf->threshold_sdevx = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->threshold_sdevx);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_sdevx",_("threshold_sdevx"));
}
static void conf_edit_threshold_sdevy(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->threshold_sdevy = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_sdevy %d"),parm_in);
        } else {
            cam->conf->threshold_sdevy = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->threshold_sdevy);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_sdevy",_("threshold_sdevy"));
}
static void conf_edit_threshold_sdevxy(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->threshold_sdevxy = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_sdevxy %d"),parm_in);
        } else {
            cam->conf->threshold_sdevxy = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->threshold_sdevxy);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_sdevxy",_("threshold_sdevxy"));
}
static void conf_edit_threshold_tune(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->threshold_tune = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->threshold_tune, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->threshold_tune);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_tune",_("threshold_tune"));
}
static void conf_edit_secondary_interval(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->secondary_interval = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid secondary_interval %d"),parm_in);
        } else {
            cam->conf->secondary_interval = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->secondary_interval);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_interval",_("secondary_interval"));
}
static void conf_edit_secondary_method(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->secondary_method = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid secondary_method %d"),parm_in);
        } else {
            cam->conf->secondary_method = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->secondary_method);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_method",_("secondary_method"));
}
static void conf_edit_secondary_model(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->secondary_model = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->secondary_model = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->secondary_model;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_model",_("secondary_model"));
}
static void conf_edit_secondary_config(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->secondary_config = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->secondary_config = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->secondary_config;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_config",_("secondary_config"));
}
static void conf_edit_secondary_method2(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->secondary_method2 = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid secondary_method2 %d"),parm_in);
        } else {
            cam->conf->secondary_method2 = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->secondary_method2);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_method2",_("secondary_method2"));
}
static void conf_edit_secondary_model2(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->secondary_model2 = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->secondary_model2 = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->secondary_model2;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_model2",_("secondary_model2"));
}
static void conf_edit_secondary_config2(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->secondary_config2 = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->secondary_config2 = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->secondary_config2;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_config2",_("secondary_config2"));
}
static void conf_edit_secondary_method3(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->secondary_method3 = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid secondary_method3 %d"),parm_in);
        } else {
            cam->conf->secondary_method3 = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->secondary_method3);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_method3",_("secondary_method3"));
}
static void conf_edit_secondary_model3(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->secondary_model3 = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->secondary_model3 = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->secondary_model3;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_model3",_("secondary_model3"));
}
static void conf_edit_secondary_config3(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->secondary_config3 = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->secondary_config3 = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->secondary_config3;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_config3",_("secondary_config3"));
}
static void conf_edit_noise_level(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->noise_level = 32;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 255)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid noise_level %d"),parm_in);
        } else {
            cam->conf->noise_level = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->noise_level);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","noise_level",_("noise_level"));
}
static void conf_edit_noise_tune(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->noise_tune = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->noise_tune, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->noise_tune);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","noise_tune",_("noise_tune"));
}
static void conf_edit_despeckle_filter(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->despeckle_filter = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->despeckle_filter = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->despeckle_filter;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","despeckle_filter",_("despeckle_filter"));
}
static void conf_edit_area_detect(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->area_detect = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->area_detect = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->area_detect;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","area_detect",_("area_detect"));
}
static void conf_edit_mask_file(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->mask_file = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->mask_file = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->mask_file;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mask_file",_("mask_file"));
}
static void conf_edit_mask_privacy(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->mask_privacy = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->mask_privacy = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->mask_privacy;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mask_privacy",_("mask_privacy"));
}
static void conf_edit_smart_mask_speed(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->smart_mask_speed = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 10)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid smart_mask_speed %d"),parm_in);
        } else {
            cam->conf->smart_mask_speed = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->smart_mask_speed);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","smart_mask_speed",_("smart_mask_speed"));
}
static void conf_edit_lightswitch_percent(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->lightswitch_percent = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid lightswitch_percent %d"),parm_in);
        } else {
            cam->conf->lightswitch_percent = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->lightswitch_percent);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","lightswitch_percent",_("lightswitch_percent"));
}
static void conf_edit_lightswitch_frames(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->lightswitch_frames = 5;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 1000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid lightswitch_frames %d"),parm_in);
        } else {
            cam->conf->lightswitch_frames = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->lightswitch_frames);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","lightswitch_frames",_("lightswitch_frames"));
}
static void conf_edit_minimum_motion_frames(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->minimum_motion_frames = 1;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 10000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid minimum_motion_frames %d"),parm_in);
        } else {
            cam->conf->minimum_motion_frames = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->minimum_motion_frames);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","minimum_motion_frames",_("minimum_motion_frames"));
}
static void conf_edit_event_gap(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->event_gap = 60;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid event_gap %d"),parm_in);
        } else {
            cam->conf->event_gap = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->event_gap);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","event_gap",_("event_gap"));
}
static void conf_edit_pre_capture(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->pre_capture = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 10000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid pre_capture %d"),parm_in);
        } else {
            cam->conf->pre_capture = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->pre_capture);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pre_capture",_("pre_capture"));
}
static void conf_edit_post_capture(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->post_capture = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid post_capture %d"),parm_in);
        } else {
            cam->conf->post_capture = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->post_capture);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","post_capture",_("post_capture"));
}

static void conf_edit_on_event_start(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_event_start = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->on_event_start = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->on_event_start;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_event_start",_("on_event_start"));
}
static void conf_edit_on_event_end(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_event_end = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->on_event_end = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->on_event_end;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_event_end",_("on_event_end"));
}
static void conf_edit_on_picture_save(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_picture_save = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->on_picture_save = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->on_picture_save;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_picture_save",_("on_picture_save"));
}
static void conf_edit_on_area_detected(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_area_detected = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->on_area_detected = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->on_area_detected;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_area_detected",_("on_area_detected"));
}
static void conf_edit_on_motion_detected(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_motion_detected = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->on_motion_detected = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->on_motion_detected;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_motion_detected",_("on_motion_detected"));
}
static void conf_edit_on_movie_start(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_movie_start = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->on_movie_start = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->on_movie_start;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_movie_start",_("on_movie_start"));
}
static void conf_edit_on_movie_end(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_movie_end = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->on_movie_end = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->on_movie_end;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_movie_end",_("on_movie_end"));
}
static void conf_edit_on_camera_lost(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_camera_lost = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->on_camera_lost = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->on_camera_lost;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_camera_lost",_("on_camera_lost"));
}
static void conf_edit_on_camera_found(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_camera_found = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->on_camera_found = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->on_camera_found;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_camera_found",_("on_camera_found"));
}

static void conf_edit_picture_output(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->picture_output = "off";
    } else if (pact == PARM_ACT_SET){
        if ((parm == "on") || (parm == "off") ||
            (parm == "first") || (parm == "best"))  {
            cam->conf->picture_output = parm;
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_output %s"), parm);
        }
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->picture_output;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_output",_("picture_output"));
}
static void conf_edit_picture_output_motion(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->picture_output_motion = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->picture_output_motion, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->picture_output_motion);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_output_motion",_("picture_output_motion"));
}
static void conf_edit_picture_type(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->picture_type = "jpeg";
    } else if (pact == PARM_ACT_SET){
        if ((parm == "jpeg") || (parm == "webp") || (parm == "ppm"))  {
            cam->conf->picture_type = parm;
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_type %s"), parm);
        }
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->picture_type;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_type",_("picture_type"));
}
static void conf_edit_picture_quality(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->picture_quality = 75;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_quality %d"),parm_in);
        } else {
            cam->conf->picture_quality = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->picture_quality);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_quality",_("picture_quality"));
}
static void conf_edit_picture_exif(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->picture_exif = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->picture_exif = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->picture_exif;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_exif",_("picture_exif"));
}
static void conf_edit_picture_filename(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->picture_filename = "%v-%Y%m%d%H%M%S-%q";
    } else if (pact == PARM_ACT_SET){
        cam->conf->picture_filename = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->picture_filename;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_filename",_("picture_filename"));
}
static void conf_edit_snapshot_interval(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->snapshot_interval = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid snapshot_interval %d"),parm_in);
        } else {
            cam->conf->snapshot_interval = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->snapshot_interval);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snapshot_interval",_("snapshot_interval"));
}
static void conf_edit_snapshot_filename(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->snapshot_filename = "%v-%Y%m%d%H%M%S-snapshot";
    } else if (pact == PARM_ACT_SET){
        cam->conf->snapshot_filename = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->snapshot_filename;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snapshot_filename",_("snapshot_filename"));
}

static void conf_edit_movie_output(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->movie_output = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->movie_output, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->movie_output);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_output",_("movie_output"));
}
static void conf_edit_movie_output_motion(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->movie_output_motion = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->movie_output_motion, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->movie_output_motion);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_output_motion",_("movie_output_motion"));
}
static void conf_edit_movie_max_time(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->movie_max_time = 120;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid movie_max_time %d"),parm_in);
        } else {
            cam->conf->movie_max_time = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->movie_max_time);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_max_time",_("movie_max_time"));
}
static void conf_edit_movie_bps(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->movie_bps = 400000;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 9999999)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid movie_bps %d"),parm_in);
        } else {
            cam->conf->movie_bps = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->movie_bps);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_bps",_("movie_bps"));
}
static void conf_edit_movie_quality(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->movie_quality = 60;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid movie_quality %d"),parm_in);
        } else {
            cam->conf->movie_quality = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->movie_quality);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_quality",_("movie_quality"));
}
static void conf_edit_movie_codec(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->movie_codec = "mkv";
    } else if (pact == PARM_ACT_SET){
        cam->conf->movie_codec = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->movie_codec;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_codec",_("movie_codec"));
}
static void conf_edit_movie_passthrough(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->movie_passthrough = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->movie_passthrough, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->movie_passthrough);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_passthrough",_("movie_passthrough"));
}
static void conf_edit_movie_filename(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->movie_filename = "%v-%Y%m%d%H%M%S";
    } else if (pact == PARM_ACT_SET){
        cam->conf->movie_filename = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->movie_filename;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_filename",_("movie_filename"));
}
static void conf_edit_movie_extpipe_use(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->movie_extpipe_use = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->movie_extpipe_use, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->movie_extpipe_use);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_extpipe_use",_("movie_extpipe_use"));
}
static void conf_edit_movie_extpipe(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->movie_extpipe = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->movie_extpipe = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->movie_extpipe;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_extpipe",_("movie_extpipe"));
}

static void conf_edit_timelapse_interval(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->timelapse_interval = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_interval %d"),parm_in);
        } else {
            cam->conf->timelapse_interval = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->timelapse_interval);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_interval",_("timelapse_interval"));
}
static void conf_edit_timelapse_mode(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->timelapse_mode = "daily";
    } else if (pact == PARM_ACT_SET){
        if ((parm == "hourly") || (parm == "daily") ||
            (parm == "weekly-sunday") || (parm == "weekly-monday") ||
            (parm == "monthly") || (parm == "manual"))  {
            cam->conf->timelapse_mode = parm;
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_mode %s"), parm);
        }
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->timelapse_mode;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_mode",_("timelapse_mode"));
}
static void conf_edit_timelapse_fps(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->timelapse_fps = 30;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 2) || (parm_in > 1000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_fps %d"),parm_in);
        } else {
            cam->conf->timelapse_fps = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->timelapse_fps);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_fps",_("timelapse_fps"));
}
static void conf_edit_timelapse_codec(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->timelapse_codec = "mpg";
    } else if (pact == PARM_ACT_SET){
        if ((parm == "mpg") || (parm == "mpeg4"))  {
            cam->conf->timelapse_codec = parm;
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_codec %s"), parm);
        }
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->timelapse_codec;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_codec",_("timelapse_codec"));
}
static void conf_edit_timelapse_filename(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->timelapse_filename = "%Y%m%d-timelapse";
    } else if (pact == PARM_ACT_SET){
        cam->conf->timelapse_filename = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->timelapse_filename;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_filename",_("timelapse_filename"));
}

static void conf_edit_video_pipe(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->video_pipe = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->video_pipe = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->video_pipe;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","video_pipe",_("video_pipe"));
}
static void conf_edit_video_pipe_motion(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->video_pipe_motion = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->video_pipe_motion = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->video_pipe_motion;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","video_pipe_motion",_("video_pipe_motion"));
}

static void conf_edit_webcontrol_port(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->webcontrol_port = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 65535)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_port %d"),parm_in);
        } else {
            cam->conf->webcontrol_port = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->webcontrol_port);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_port",_("webcontrol_port"));
}
static void conf_edit_webcontrol_ipv6(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->webcontrol_ipv6 = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->webcontrol_ipv6, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->webcontrol_ipv6);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_ipv6",_("webcontrol_ipv6"));
}
static void conf_edit_webcontrol_localhost(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->webcontrol_localhost = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->webcontrol_localhost, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->webcontrol_localhost);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_localhost",_("webcontrol_localhost"));
}
static void conf_edit_webcontrol_parms(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->webcontrol_parms = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 3)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_parms %d"),parm_in);
        } else {
            cam->conf->webcontrol_parms = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->webcontrol_parms);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_parms",_("webcontrol_parms"));
}
static void conf_edit_webcontrol_interface(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->webcontrol_interface = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_interface %d"),parm_in);
        } else {
            cam->conf->webcontrol_interface = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->webcontrol_interface);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_interface",_("webcontrol_interface"));
}
static void conf_edit_webcontrol_auth_method(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->webcontrol_auth_method = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_auth_method %d"),parm_in);
        } else {
            cam->conf->webcontrol_auth_method = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->webcontrol_auth_method);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_auth_method",_("webcontrol_auth_method"));
}
static void conf_edit_webcontrol_authentication(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->webcontrol_authentication = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->webcontrol_authentication = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->webcontrol_authentication;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_authentication",_("webcontrol_authentication"));
}
static void conf_edit_webcontrol_tls(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->webcontrol_tls = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->webcontrol_tls, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->webcontrol_tls);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_tls",_("webcontrol_tls"));
}
static void conf_edit_webcontrol_cert(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->webcontrol_cert = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->webcontrol_cert = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->webcontrol_cert;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_cert",_("webcontrol_cert"));
}
static void conf_edit_webcontrol_key(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->webcontrol_key = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->webcontrol_key = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->webcontrol_key;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_key",_("webcontrol_key"));
}
static void conf_edit_webcontrol_cors_header(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int retcd;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->webcontrol_cors_header = "";
    } else if (pact == PARM_ACT_SET){
        // A complicated regex to validate a url found here:
        // https://stackoverflow.com/questions/38608116/how-to-check-a-specified-string-is-a-valid-url-or-not-using-c-code
        const char *regex_str = "^(https?:\\/\\/)?([\\da-z\\.-]+)\\.([a-z\\.]{2,6})([\\/\\w \\.-]*)*\\/?$";
        regex_t regex;
        if (regcomp(&regex, regex_str, REG_EXTENDED) != 0) {
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,_("Error compiling regex in copy_uri"));
            return;
        }
        retcd = regexec(&regex, parm.c_str(), 0, NULL, 0);
        if ((parm != "*") && (retcd == REG_NOMATCH)) {
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,_("Invalid origin for cors_header"));
        } else {
            cam->conf->webcontrol_cors_header = parm;
        }
        regfree(&regex);
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->webcontrol_cors_header;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_cors_header",_("webcontrol_cors_header"));
}

static void conf_edit_stream_port(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->stream_port = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 65535)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_port %d"),parm_in);
        } else {
            cam->conf->stream_port = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->stream_port);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_port",_("stream_port"));
}
static void conf_edit_stream_localhost(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->stream_localhost = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->stream_localhost, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->stream_localhost);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_localhost",_("stream_localhost"));
}
static void conf_edit_stream_auth_method(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->stream_auth_method = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_auth_method %d"),parm_in);
        } else {
            cam->conf->stream_auth_method = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->stream_auth_method);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_auth_method",_("stream_auth_method"));
}
static void conf_edit_stream_authentication(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->stream_authentication = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->stream_authentication = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->stream_authentication;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_authentication",_("stream_authentication"));
}
static void conf_edit_stream_tls(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->stream_tls = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->stream_tls, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->stream_tls);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_tls",_("stream_tls"));
}
static void conf_edit_stream_cors_header(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int retcd;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->stream_cors_header = "";
    } else if (pact == PARM_ACT_SET){
        // A complicated regex to validate a url found here:
        // https://stackoverflow.com/questions/38608116/how-to-check-a-specified-string-is-a-valid-url-or-not-using-c-code
        const char *regex_str = "^(https?:\\/\\/)?([\\da-z\\.-]+)\\.([a-z\\.]{2,6})([\\/\\w \\.-]*)*\\/?$";
        regex_t regex;
        if (regcomp(&regex, regex_str, REG_EXTENDED) != 0) {
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,_("Error compiling regex in copy_uri"));
            return;
        }
        retcd = regexec(&regex, parm.c_str(), 0, NULL, 0);
        if ((parm != "*") && (retcd == REG_NOMATCH)) {
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,_("Invalid origin for cors_header"));
        } else {
            cam->conf->stream_cors_header = parm;
        }
        regfree(&regex);
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->stream_cors_header;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_cors_header",_("stream_cors_header"));
}
static void conf_edit_stream_preview_scale(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->stream_preview_scale = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 1000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_preview_scale %d"),parm_in);
        } else {
            cam->conf->stream_preview_scale = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->stream_preview_scale);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_scale",_("stream_preview_scale"));
}
static void conf_edit_stream_preview_newline(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->stream_preview_newline = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->stream_preview_newline, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->stream_preview_newline);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_newline",_("stream_preview_newline"));
}
static void conf_edit_stream_preview_method(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->stream_preview_method = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 4)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_preview_method %d"),parm_in);
        } else {
            cam->conf->stream_preview_method = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->stream_preview_method);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_method",_("stream_preview_method"));
}
static void conf_edit_stream_quality(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->stream_quality = 50;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_quality %d"),parm_in);
        } else {
            cam->conf->stream_quality = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->stream_quality);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_quality",_("stream_quality"));
}
static void conf_edit_stream_grey(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->stream_grey = FALSE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->stream_grey, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->stream_grey);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_grey",_("stream_grey"));
}
static void conf_edit_stream_motion(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->stream_motion = FALSE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->stream_motion, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->stream_motion);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_motion",_("stream_motion"));
}
static void conf_edit_stream_maxrate(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->stream_maxrate = 1;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_maxrate %d"),parm_in);
        } else {
            cam->conf->stream_maxrate = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->stream_maxrate);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_maxrate",_("stream_maxrate"));
}
static void conf_edit_database_type(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->database_type = "";
    } else if (pact == PARM_ACT_SET){
        if ((parm == "mysql") || (parm == "mariadb") ||
            (parm == "postgresql") || (parm == "sqlite3")) {
            cam->conf->database_type = parm;
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid database_type %s"), parm);
        }
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->database_type;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_type",_("database_type"));
}
static void conf_edit_database_dbname(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->database_dbname = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->database_dbname = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->database_dbname;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_dbname",_("database_dbname"));
}
static void conf_edit_database_host(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->database_host = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->database_host = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->database_host;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_host",_("database_host"));
}
static void conf_edit_database_port(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->database_port = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 65535)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid database_port %d"),parm_in);
        } else {
            cam->conf->database_port = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->database_port);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_port",_("database_port"));
}
static void conf_edit_database_user(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->database_user = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->database_user = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->database_user;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_user",_("database_user"));
}
static void conf_edit_database_password(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->database_password = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->database_password = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->database_password;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_password",_("database_password"));
}
static void conf_edit_database_busy_timeout(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->database_busy_timeout = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 10000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid database_busy_timeout %d"),parm_in);
        } else {
            cam->conf->database_busy_timeout = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->database_busy_timeout);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_busy_timeout",_("database_busy_timeout"));
}

static void conf_edit_sql_log_picture(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->sql_log_picture = FALSE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->sql_log_picture, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->sql_log_picture);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_log_picture",_("sql_log_picture"));
}
static void conf_edit_sql_log_snapshot(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->sql_log_snapshot = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->sql_log_snapshot, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->sql_log_snapshot);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_log_snapshot",_("sql_log_snapshot"));
}
static void conf_edit_sql_log_movie(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->sql_log_movie = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->sql_log_movie, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->sql_log_movie);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_log_movie",_("sql_log_movie"));
}
static void conf_edit_sql_log_timelapse(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->sql_log_timelapse = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->sql_log_timelapse, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->sql_log_timelapse);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_log_timelapse",_("sql_log_timelapse"));
}
static void conf_edit_sql_query_start(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->sql_query_start = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->sql_query_start = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->sql_query_start;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_query_start",_("sql_query_start"));
}
static void conf_edit_sql_query_stop(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->sql_query_stop = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->sql_query_stop = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->sql_query_stop;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_query_stop",_("sql_query_stop"));
}
static void conf_edit_sql_query(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->sql_query = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->sql_query = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->sql_query;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_query",_("sql_query"));
}

static void conf_edit_track_type(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->track_type = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 5)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid track_type %d"),parm_in);
        } else {
            cam->conf->track_type = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->track_type);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_type",_("track_type"));
}
static void conf_edit_track_auto(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf->track_auto = TRUE;
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_bool(cam->conf->track_auto, parm);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(parm, cam->conf->track_auto);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_auto",_("track_auto"));
}
static void conf_edit_track_move_wait(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->track_move_wait = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid track_move_wait %d"),parm_in);
        } else {
            cam->conf->track_move_wait = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->track_move_wait);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_move_wait",_("track_move_wait"));
}
static void conf_edit_track_generic_move(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        cam->conf->track_generic_move = "";
    } else if (pact == PARM_ACT_SET){
        cam->conf->track_generic_move = parm;
    } else if (pact == PARM_ACT_GET){
        parm = cam->conf->track_generic_move;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_generic_move",_("track_generic_move"));
}
static void conf_edit_track_step_angle_x(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->track_step_angle_x = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid track_step_angle_x %d"),parm_in);
        } else {
            cam->conf->track_step_angle_x = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->track_step_angle_x);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_step_angle_x",_("track_step_angle_x"));
}
static void conf_edit_track_step_angle_y(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf->track_step_angle_y = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid track_step_angle_y %d"),parm_in);
        } else {
            cam->conf->track_step_angle_y = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        parm = std::to_string(cam->conf->track_step_angle_y);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_step_angle_y",_("track_step_angle_y"));
}

static void conf_edit_cat00(struct ctx_motapp *motapp, std::string cmd, std::string &parm_val, enum PARM_ACT pact) {

    if (cmd == "daemon") {                        conf_edit_daemon(motapp, parm_val, pact);
    } else if (cmd == "conf_filename"){           conf_edit_conf_filename(motapp, parm_val, pact);
    } else if (cmd == "setup_mode") {             conf_edit_setup_mode(motapp, parm_val, pact);
    } else if (cmd == "pid_file"){                conf_edit_pid_file(motapp, parm_val, pact);
    } else if (cmd == "log_file"){                conf_edit_log_file(motapp, parm_val, pact);
    } else if (cmd == "log_level"){               conf_edit_log_level(motapp, parm_val, pact);
    } else if (cmd == "log_type"){                conf_edit_log_type(motapp, parm_val, pact);
    } else if (cmd == "native_language"){         conf_edit_native_language(motapp, parm_val, pact);
    }

}
static void conf_edit_cat01(struct ctx_cam *cam, std::string parm_nm, std::string &parm_val, enum PARM_ACT pact) {

    if (parm_nm == "quiet"){                        conf_edit_quiet(cam, parm_val, pact);
    } else if (parm_nm == "camera_dir"){            conf_edit_camera_dir(cam, parm_val, pact);
    } else if (parm_nm == "camera_name"){           conf_edit_camera_name(cam, parm_val, pact);
    } else if (parm_nm == "camera_id"){             conf_edit_camera_id(cam, parm_val, pact);
    } else if (parm_nm == "target_dir"){            conf_edit_target_dir(cam, parm_val, pact);
    } else if (parm_nm == "videodevice"){           conf_edit_videodevice(cam, parm_val, pact);
    } else if (parm_nm == "vid_control_params"){    conf_edit_vid_control_params(cam, parm_val, pact);
    } else if (parm_nm == "v4l2_palette"){          conf_edit_v4l2_palette(cam, parm_val, pact);
    } else if (parm_nm == "input"){                 conf_edit_input(cam, parm_val, pact);
    } else if (parm_nm == "norm"){                  conf_edit_norm(cam, parm_val, pact);
    } else if (parm_nm == "frequency"){             conf_edit_frequency(cam, parm_val, pact);
    } else if (parm_nm == "tuner_device"){          conf_edit_tuner_device(cam, parm_val, pact);
    } else if (parm_nm == "roundrobin_frames"){     conf_edit_roundrobin_frames(cam, parm_val, pact);
    } else if (parm_nm == "roundrobin_skip"){       conf_edit_roundrobin_skip(cam, parm_val, pact);
    } else if (parm_nm == "roundrobin_switchfilter"){   conf_edit_roundrobin_switchfilter(cam, parm_val, pact);
    } else if (parm_nm == "netcam_url"){            conf_edit_netcam_url(cam, parm_val, pact);
    } else if (parm_nm == "netcam_highres"){        conf_edit_netcam_highres(cam, parm_val, pact);
    } else if (parm_nm == "netcam_userpass"){       conf_edit_netcam_userpass(cam, parm_val, pact);
    } else if (parm_nm == "netcam_use_tcp"){        conf_edit_netcam_use_tcp(cam, parm_val, pact);
    } else if (parm_nm == "mmalcam_name"){          conf_edit_mmalcam_name(cam, parm_val, pact);
    } else if (parm_nm == "mmalcam_control_params"){conf_edit_mmalcam_control_params(cam, parm_val, pact);
    }

}
static void conf_edit_cat02(struct ctx_cam *cam, std::string parm_nm, std::string &parm_val, enum PARM_ACT pact) {

    if (parm_nm == "width"){                          conf_edit_width(cam, parm_val, pact);
    } else if (parm_nm == "height"){                  conf_edit_height(cam, parm_val, pact);
    } else if (parm_nm == "framerate"){               conf_edit_framerate(cam, parm_val, pact);
    } else if (parm_nm == "minimum_frame_time"){      conf_edit_minimum_frame_time(cam, parm_val, pact);
    } else if (parm_nm == "rotate"){                  conf_edit_rotate(cam, parm_val, pact);
    } else if (parm_nm == "flip_axis"){               conf_edit_flip_axis(cam, parm_val, pact);
    } else if (parm_nm == "locate_motion_mode"){      conf_edit_locate_motion_mode(cam, parm_val, pact);
    } else if (parm_nm == "locate_motion_style"){     conf_edit_locate_motion_style(cam, parm_val, pact);
    } else if (parm_nm == "text_left"){               conf_edit_text_left(cam, parm_val, pact);
    } else if (parm_nm == "text_right"){              conf_edit_text_right(cam, parm_val, pact);
    } else if (parm_nm == "text_changes"){            conf_edit_text_changes(cam, parm_val, pact);
    } else if (parm_nm == "text_scale"){              conf_edit_text_scale(cam, parm_val, pact);
    } else if (parm_nm == "text_event"){              conf_edit_text_event(cam, parm_val, pact);
    } else if (parm_nm == "emulate_motion"){          conf_edit_emulate_motion(cam, parm_val, pact);
    } else if (parm_nm == "primary_method"){          conf_edit_primary_method(cam, parm_val, pact);
    } else if (parm_nm == "threshold"){               conf_edit_threshold(cam, parm_val, pact);
    } else if (parm_nm == "threshold_maximum"){       conf_edit_threshold_maximum(cam, parm_val, pact);
    } else if (parm_nm == "threshold_sdevx"){         conf_edit_threshold_sdevx(cam, parm_val, pact);
    } else if (parm_nm == "threshold_sdevy"){         conf_edit_threshold_sdevy(cam, parm_val, pact);
    } else if (parm_nm == "threshold_sdevxy"){        conf_edit_threshold_sdevxy(cam, parm_val, pact);
    } else if (parm_nm == "threshold_tune"){          conf_edit_threshold_tune(cam, parm_val, pact);
    } else if (parm_nm == "secondary_interval"){      conf_edit_secondary_interval(cam, parm_val, pact);
    } else if (parm_nm == "secondary_method"){        conf_edit_secondary_method(cam, parm_val, pact);
    } else if (parm_nm == "secondary_model"){         conf_edit_secondary_model(cam, parm_val, pact);
    } else if (parm_nm == "secondary_config"){        conf_edit_secondary_config(cam, parm_val, pact);
    } else if (parm_nm == "secondary_method2"){       conf_edit_secondary_method2(cam, parm_val, pact);
    } else if (parm_nm == "secondary_model2"){        conf_edit_secondary_model2(cam, parm_val, pact);
    } else if (parm_nm == "secondary_config2"){       conf_edit_secondary_config2(cam, parm_val, pact);
    } else if (parm_nm == "secondary_method3"){       conf_edit_secondary_method3(cam, parm_val, pact);
    } else if (parm_nm == "secondary_model3"){        conf_edit_secondary_model3(cam, parm_val, pact);
    } else if (parm_nm == "secondary_config3"){       conf_edit_secondary_config3(cam, parm_val, pact);
    } else if (parm_nm == "noise_level"){             conf_edit_noise_level(cam, parm_val, pact);
    } else if (parm_nm == "noise_tune"){              conf_edit_noise_tune(cam, parm_val, pact);
    } else if (parm_nm == "despeckle_filter"){        conf_edit_despeckle_filter(cam, parm_val, pact);
    } else if (parm_nm == "area_detect"){             conf_edit_area_detect(cam, parm_val, pact);
    } else if (parm_nm == "mask_file"){               conf_edit_mask_file(cam, parm_val, pact);
    } else if (parm_nm == "mask_privacy"){            conf_edit_mask_privacy(cam, parm_val, pact);
    } else if (parm_nm == "smart_mask_speed"){        conf_edit_smart_mask_speed(cam, parm_val, pact);
    } else if (parm_nm == "lightswitch_percent"){     conf_edit_lightswitch_percent(cam, parm_val, pact);
    } else if (parm_nm == "lightswitch_frames"){      conf_edit_lightswitch_frames(cam, parm_val, pact);
    } else if (parm_nm == "minimum_motion_frames"){   conf_edit_minimum_motion_frames(cam, parm_val, pact);
    } else if (parm_nm == "event_gap"){               conf_edit_event_gap(cam, parm_val, pact);
    } else if (parm_nm == "pre_capture"){             conf_edit_pre_capture(cam, parm_val, pact);
    } else if (parm_nm == "post_capture"){            conf_edit_post_capture(cam, parm_val, pact);
    }

}
static void conf_edit_cat03(struct ctx_cam *cam, std::string parm_nm, std::string &parm_val, enum PARM_ACT pact) {

    if (parm_nm == "on_event_start"){                 conf_edit_on_event_start(cam, parm_val, pact);
    } else if (parm_nm == "on_event_end"){            conf_edit_on_event_end(cam, parm_val, pact);
    } else if (parm_nm == "on_picture_save"){         conf_edit_on_picture_save(cam, parm_val, pact);
    } else if (parm_nm == "on_area_detected"){        conf_edit_on_area_detected(cam, parm_val, pact);
    } else if (parm_nm == "on_motion_detected"){      conf_edit_on_motion_detected(cam, parm_val, pact);
    } else if (parm_nm == "on_movie_start"){          conf_edit_on_movie_start(cam, parm_val, pact);
    } else if (parm_nm == "on_movie_end"){            conf_edit_on_movie_end(cam, parm_val, pact);
    } else if (parm_nm == "on_camera_lost"){          conf_edit_on_camera_lost(cam, parm_val, pact);
    } else if (parm_nm == "on_camera_found"){         conf_edit_on_camera_found(cam, parm_val, pact);
    } else if (parm_nm == "picture_output"){          conf_edit_picture_output(cam, parm_val, pact);
    } else if (parm_nm == "picture_output_motion"){   conf_edit_picture_output_motion(cam, parm_val, pact);
    } else if (parm_nm == "picture_type"){            conf_edit_picture_type(cam, parm_val, pact);
    } else if (parm_nm == "picture_quality"){         conf_edit_picture_quality(cam, parm_val, pact);
    } else if (parm_nm == "picture_exif"){            conf_edit_picture_exif(cam, parm_val, pact);
    } else if (parm_nm == "picture_filename"){        conf_edit_picture_filename(cam, parm_val, pact);
    } else if (parm_nm == "snapshot_interval"){       conf_edit_snapshot_interval(cam, parm_val, pact);
    } else if (parm_nm == "snapshot_filename"){       conf_edit_snapshot_filename(cam, parm_val, pact);
    } else if (parm_nm == "movie_output"){            conf_edit_movie_output(cam, parm_val, pact);
    } else if (parm_nm == "movie_output_motion"){     conf_edit_movie_output_motion(cam, parm_val, pact);
    } else if (parm_nm == "movie_max_time"){          conf_edit_movie_max_time(cam, parm_val, pact);
    } else if (parm_nm == "movie_bps"){               conf_edit_movie_bps(cam, parm_val, pact);
    } else if (parm_nm == "movie_quality"){           conf_edit_movie_quality(cam, parm_val, pact);
    } else if (parm_nm == "movie_codec"){             conf_edit_movie_codec(cam, parm_val, pact);
    } else if (parm_nm == "movie_passthrough"){       conf_edit_movie_passthrough(cam, parm_val, pact);
    } else if (parm_nm == "movie_filename"){          conf_edit_movie_filename(cam, parm_val, pact);
    } else if (parm_nm == "movie_extpipe_use"){       conf_edit_movie_extpipe_use(cam, parm_val, pact);
    } else if (parm_nm == "movie_extpipe"){           conf_edit_movie_extpipe(cam, parm_val, pact);
    } else if (parm_nm == "timelapse_interval"){      conf_edit_timelapse_interval(cam, parm_val, pact);
    } else if (parm_nm == "timelapse_mode"){          conf_edit_timelapse_mode(cam, parm_val, pact);
    } else if (parm_nm == "timelapse_fps"){           conf_edit_timelapse_fps(cam, parm_val, pact);
    } else if (parm_nm == "timelapse_codec"){         conf_edit_timelapse_codec(cam, parm_val, pact);
    } else if (parm_nm == "timelapse_filename"){      conf_edit_timelapse_filename(cam, parm_val, pact);
    } else if (parm_nm == "video_pipe"){              conf_edit_video_pipe(cam, parm_val, pact);
    } else if (parm_nm == "video_pipe_motion"){       conf_edit_video_pipe_motion(cam, parm_val, pact);
    }

}
static void conf_edit_cat04(struct ctx_cam *cam, std::string parm_nm, std::string &parm_val, enum PARM_ACT pact) {
    if (parm_nm == "webcontrol_port"){                    conf_edit_webcontrol_port(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_ipv6"){             conf_edit_webcontrol_ipv6(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_localhost"){        conf_edit_webcontrol_localhost(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_parms"){            conf_edit_webcontrol_parms(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_interface"){        conf_edit_webcontrol_interface(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_auth_method"){      conf_edit_webcontrol_auth_method(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_authentication"){   conf_edit_webcontrol_authentication(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_tls"){              conf_edit_webcontrol_tls(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_cert"){             conf_edit_webcontrol_cert(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_key"){              conf_edit_webcontrol_key(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_cors_header"){      conf_edit_webcontrol_cors_header(cam, parm_val, pact);
    } else if (parm_nm == "stream_port"){                 conf_edit_stream_port(cam, parm_val, pact);
    } else if (parm_nm == "stream_localhost"){            conf_edit_stream_localhost(cam, parm_val, pact);
    } else if (parm_nm == "stream_auth_method"){          conf_edit_stream_auth_method(cam, parm_val, pact);
    } else if (parm_nm == "stream_authentication"){       conf_edit_stream_authentication(cam, parm_val, pact);
    } else if (parm_nm == "stream_tls"){                  conf_edit_stream_tls(cam, parm_val, pact);
    } else if (parm_nm == "stream_cors_header"){          conf_edit_stream_cors_header(cam, parm_val, pact);
    } else if (parm_nm == "stream_preview_scale"){        conf_edit_stream_preview_scale(cam, parm_val, pact);
    } else if (parm_nm == "stream_preview_newline"){      conf_edit_stream_preview_newline(cam, parm_val, pact);
    } else if (parm_nm == "stream_preview_method"){       conf_edit_stream_preview_method(cam, parm_val, pact);
    } else if (parm_nm == "stream_quality"){              conf_edit_stream_quality(cam, parm_val, pact);
    } else if (parm_nm == "stream_grey"){                 conf_edit_stream_grey(cam, parm_val, pact);
    } else if (parm_nm == "stream_motion"){               conf_edit_stream_motion(cam, parm_val, pact);
    } else if (parm_nm == "stream_maxrate"){              conf_edit_stream_maxrate(cam, parm_val, pact);
    }

}
static void conf_edit_cat05(struct ctx_cam *cam, std::string parm_nm, std::string &parm_val, enum PARM_ACT pact) {
    if (parm_nm == "database_type"){                  conf_edit_database_type(cam, parm_val, pact);
    } else if (parm_nm == "database_dbname"){         conf_edit_database_dbname(cam, parm_val, pact);
    } else if (parm_nm == "database_host"){           conf_edit_database_host(cam, parm_val, pact);
    } else if (parm_nm == "database_port"){           conf_edit_database_port(cam, parm_val, pact);
    } else if (parm_nm == "database_user"){           conf_edit_database_user(cam, parm_val, pact);
    } else if (parm_nm == "database_password"){       conf_edit_database_password(cam, parm_val, pact);
    } else if (parm_nm == "database_busy_timeout"){   conf_edit_database_busy_timeout(cam, parm_val, pact);
    } else if (parm_nm == "sql_log_picture"){         conf_edit_sql_log_picture(cam, parm_val, pact);
    } else if (parm_nm == "sql_log_snapshot"){        conf_edit_sql_log_snapshot(cam, parm_val, pact);
    } else if (parm_nm == "sql_log_movie"){           conf_edit_sql_log_movie(cam, parm_val, pact);
    } else if (parm_nm == "sql_log_timelapse"){       conf_edit_sql_log_timelapse(cam, parm_val, pact);
    } else if (parm_nm == "sql_query_start"){         conf_edit_sql_query_start(cam, parm_val, pact);
    } else if (parm_nm == "sql_query_stop"){          conf_edit_sql_query_stop(cam, parm_val, pact);
    } else if (parm_nm == "sql_query"){               conf_edit_sql_query(cam, parm_val, pact);
    } else if (parm_nm == "track_type"){              conf_edit_track_type(cam, parm_val, pact);
    } else if (parm_nm == "track_auto"){              conf_edit_track_auto(cam, parm_val, pact);
    } else if (parm_nm == "track_move_wait"){         conf_edit_track_move_wait(cam, parm_val, pact);
    } else if (parm_nm == "track_generic_move"){      conf_edit_track_generic_move(cam, parm_val, pact);
    } else if (parm_nm == "track_step_angle_x"){      conf_edit_track_step_angle_x(cam, parm_val, pact);
    } else if (parm_nm == "track_step_angle_y"){      conf_edit_track_step_angle_y(cam, parm_val, pact);
    }

}

void conf_edit_dflt_app(struct ctx_motapp *motapp) {
    std::string dflt = "";

    conf_edit_conf_filename(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_log_file(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_log_type(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_pid_file(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_daemon(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_setup_mode(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_pid_file(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_log_file(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_log_level(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_log_type(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_native_language(motapp, dflt, PARM_ACT_DFLT);

}
void conf_edit_dflt_cam(struct ctx_cam *cam) {
    int indx;
    enum PARM_CAT pcat;
    std::string dflt = "";

    indx = 0;
    while (config_parms[indx].parm_name != "") {
        pcat = config_parms[indx].parm_cat;
        if ((config_parms[indx].parm_cat != PARM_CAT_00)) {
            if (pcat == PARM_CAT_01) {
                conf_edit_cat01(cam, config_parms[indx].parm_name, dflt,PARM_ACT_DFLT);
            } else if (pcat == PARM_CAT_02) {
                conf_edit_cat02(cam, config_parms[indx].parm_name, dflt, PARM_ACT_DFLT);
            } else if (pcat == PARM_CAT_03) {
                conf_edit_cat03(cam, config_parms[indx].parm_name, dflt, PARM_ACT_DFLT);
            } else if (pcat == PARM_CAT_04) {
                conf_edit_cat04(cam, config_parms[indx].parm_name, dflt, PARM_ACT_DFLT);
            } else if (pcat == PARM_CAT_05) {
                conf_edit_cat05(cam, config_parms[indx].parm_name, dflt, PARM_ACT_DFLT);
            }
        }
        indx++;
    }

}

int conf_edit_set_active(struct ctx_motapp *motapp, int threadnbr
        , std::string parm_nm, std::string parm_val){
    int indx;
    enum PARM_CAT pcat;

    indx = 0;
    while (config_parms[indx].parm_name != "") {
        if (parm_nm ==  config_parms[indx].parm_name) {
            pcat = config_parms[indx].parm_cat;
            if ((pcat == PARM_CAT_00) && (threadnbr == -1)) {
                conf_edit_cat00(motapp, parm_nm, parm_val, PARM_ACT_SET);
            } else if ((config_parms[indx].parm_cat != PARM_CAT_00) && (threadnbr != -1)) {
                if (pcat == PARM_CAT_01) {
                    conf_edit_cat01(motapp->cam_list[threadnbr], parm_nm, parm_val, PARM_ACT_SET);
                } else if (pcat == PARM_CAT_02) {
                    conf_edit_cat02(motapp->cam_list[threadnbr], parm_nm, parm_val, PARM_ACT_SET);
                } else if (pcat == PARM_CAT_03) {
                    conf_edit_cat03(motapp->cam_list[threadnbr], parm_nm, parm_val, PARM_ACT_SET);
                } else if (pcat == PARM_CAT_04) {
                    conf_edit_cat04(motapp->cam_list[threadnbr], parm_nm, parm_val, PARM_ACT_SET);
                } else if (pcat == PARM_CAT_05) {
                    conf_edit_cat05(motapp->cam_list[threadnbr], parm_nm, parm_val, PARM_ACT_SET);
                }
            }
            return 0;
        }
        indx++;
    }
    return -1;

}

static void conf_edit_depr_vid(struct ctx_motapp *motapp, int threadnbr
        , std::string parm_nm, std::string newname, std::string parm_val){
    std::string parm_curr, parm_new;

    conf_edit_vid_control_params(motapp->cam_list[threadnbr], parm_curr, PARM_ACT_GET);
    if (parm_curr == ""){
        if (parm_nm == "power_line_frequency"){
            parm_new = "\"power line frequency\"=" + parm_val;
        } else {
            parm_new = parm_nm + "=" + parm_val;
        }
    } else {
        if (parm_nm == "power_line_frequency"){
            parm_new = parm_curr + ", \"power line frequency\"=" + parm_val;
        } else {
            parm_new = parm_curr +", " + parm_nm + "=" + parm_val;
        }
    }
    conf_edit_set_active(motapp, threadnbr, newname, parm_new);

}

static void conf_edit_depr_web(struct ctx_motapp *motapp, int threadnbr
        , std::string newname, std::string &parm_val){
    std::string parm_new;

    if ((parm_val == "1") || (parm_val == "yes") || (parm_val == "on")) {
        parm_new = "0";
    } else {
        parm_new = "1";
    }
    conf_edit_set_active(motapp, threadnbr, newname, parm_new);
}

static void conf_edit_depr_tdbl(struct ctx_motapp *motapp, int threadnbr
        , std::string newname, std::string &parm_val){
    std::string parm_new;

    if ((parm_val == "1") || (parm_val == "yes") || (parm_val == "on")) {
        parm_new = "2";
    } else {
        parm_new = "1";
    }
    conf_edit_set_active(motapp, threadnbr, newname, parm_new);
}


static int conf_edit_set_depr(struct ctx_motapp *motapp, int threadnbr
        ,std::string &parm_nm, std::string &parm_val){

    int indx;

    indx = 0;
    while (config_parms_depr[indx].parm_name != "") {
        if (parm_nm ==  config_parms_depr[indx].parm_name) {
            MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, "%s after version %s"
                , config_parms_depr[indx].info
                , config_parms_depr[indx].last_version);

            if ((config_parms_depr[indx].parm_name == "brightness") ||
                (config_parms_depr[indx].parm_name == "contrast") ||
                (config_parms_depr[indx].parm_name == "saturation") ||
                (config_parms_depr[indx].parm_name == "hue") ||
                (config_parms_depr[indx].parm_name == "power_line_frequency")) {
                conf_edit_depr_vid(motapp, threadnbr, parm_nm, config_parms_depr[indx].newname, parm_val);

            } else if ((config_parms_depr[indx].parm_name == "webcontrol_html_output")) {
                conf_edit_depr_web(motapp, threadnbr, config_parms_depr[indx].newname, parm_val);

            } else if ((config_parms_depr[indx].parm_name == "text_double")) {
                conf_edit_depr_tdbl(motapp, threadnbr, config_parms_depr[indx].newname, parm_val);

            } else {
                conf_edit_set_active(motapp, threadnbr, config_parms_depr[indx].newname, parm_val);
            }
            return 0;
        }
        indx++;
    }
    return -1;
}

void conf_edit_get(struct ctx_cam *cam, std::string parm_nm, std::string &parm_val, enum PARM_CAT parm_cat){

    if (parm_cat == PARM_CAT_00) {         conf_edit_cat00(cam->motapp, parm_nm, parm_val, PARM_ACT_GET);
    } else if (parm_cat == PARM_CAT_01) {  conf_edit_cat01(cam, parm_nm, parm_val, PARM_ACT_GET);
    } else if (parm_cat == PARM_CAT_02) {  conf_edit_cat02(cam, parm_nm, parm_val, PARM_ACT_GET);
    } else if (parm_cat == PARM_CAT_03) {  conf_edit_cat03(cam, parm_nm, parm_val, PARM_ACT_GET);
    } else if (parm_cat == PARM_CAT_04) {  conf_edit_cat04(cam, parm_nm, parm_val, PARM_ACT_GET);
    } else if (parm_cat == PARM_CAT_05) {  conf_edit_cat05(cam, parm_nm, parm_val, PARM_ACT_GET);
    } else {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,"%s",_("Program coding error"));
        parm_val = "";
    }

}

/* Interim overload until webu goes to c++ with std::string */
void conf_edit_get(struct ctx_cam *cam, std::string parm_nm, char *parm_chr, enum PARM_CAT parm_cat){
    std::string parm_val(parm_chr);

    if (parm_cat == PARM_CAT_00) {         conf_edit_cat00(cam->motapp, parm_nm, parm_val, PARM_ACT_GET);
    } else if (parm_cat == PARM_CAT_01) {  conf_edit_cat01(cam, parm_nm, parm_val, PARM_ACT_GET);
    } else if (parm_cat == PARM_CAT_02) {  conf_edit_cat02(cam, parm_nm, parm_val, PARM_ACT_GET);
    } else if (parm_cat == PARM_CAT_03) {  conf_edit_cat03(cam, parm_nm, parm_val, PARM_ACT_GET);
    } else if (parm_cat == PARM_CAT_04) {  conf_edit_cat04(cam, parm_nm, parm_val, PARM_ACT_GET);
    } else if (parm_cat == PARM_CAT_05) {  conf_edit_cat05(cam, parm_nm, parm_val, PARM_ACT_GET);
    } else {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,"%s",_("Program coding error"));
        parm_val = "";
    }

    parm_val.copy(parm_chr,512);

}
void conf_edit_set(struct ctx_motapp *motapp, int threadnbr
        ,std::string parm_nm, std::string parm_val){

    if (conf_edit_set_active(motapp, threadnbr, parm_nm, parm_val) == 0) return;

    if (conf_edit_set_depr(motapp, threadnbr, parm_nm, parm_val) == 0) return;

    if (threadnbr != -1){
        MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, _("Unknown config option \"%s\""), parm_nm.c_str());
    }

}
void conf_edit_set(struct ctx_motapp *motapp, int threadnbr
        ,const char *parm_nm_chr, std::string parm_val){
    std::string parm_nm(parm_nm_chr);

    if (conf_edit_set_active(motapp, threadnbr, parm_nm, parm_val) == 0) return;

    if (conf_edit_set_depr(motapp, threadnbr, parm_nm, parm_val) == 0) return;

    if (threadnbr != -1){
        MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, _("Unknown config option \"%s\""), parm_nm.c_str());
    }

}

void conf_edit_set(struct ctx_motapp *motapp, int threadnbr
        ,std::string parm_nm, const char *parm_val_chr){
    std::string parm_val(parm_val_chr);

    if (conf_edit_set_active(motapp, threadnbr, parm_nm, parm_val) == 0) return;

    if (conf_edit_set_depr(motapp, threadnbr, parm_nm, parm_val) == 0) return;

    if (threadnbr != -1){
        MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, _("Unknown config option \"%s\""), parm_nm.c_str());
    }

}
void conf_edit_set(struct ctx_motapp *motapp, int threadnbr
        ,const char *parm_nm_chr, const char *parm_val_chr){
    std::string parm_val(parm_val_chr);
    std::string parm_nm(parm_nm_chr);

    if (conf_edit_set_active(motapp, threadnbr, parm_nm, parm_val) == 0) return;

    if (conf_edit_set_depr(motapp, threadnbr, parm_nm, parm_val) == 0) return;

    if (threadnbr != -1){
        MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, _("Unknown config option \"%s\""), parm_nm.c_str());
    }

}

