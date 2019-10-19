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
#include "motion.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "conf.hpp"

enum PARM_ACT{
    PARM_ACT_DFLT
    ,PARM_ACT_SET
    ,PARM_ACT_GET
    ,PARM_ACT_FREE
};

static void conf_edit_get_string(char *parm, char *arg1) {
    if (parm == NULL){
        snprintf(arg1, PATH_MAX,"%s","");
    } else {
        snprintf(arg1,PATH_MAX,"%s",parm);
    }
}
static void conf_edit_get_int(int parm, char *arg1) {
    snprintf(arg1, 20, "%d", parm);
}
static void conf_edit_get_bool(int parm, char *arg1) {
    if (parm == TRUE){
        snprintf(arg1, PATH_MAX, "%s", "on");
    } else {
        snprintf(arg1, PATH_MAX, "%s", "off");
    }
}
static void conf_edit_set_string(char **parm_cam, char *arg1) {
    if (*parm_cam != NULL) free(*parm_cam);
    if (arg1 == NULL){
        *parm_cam = NULL;
    } else {
        *parm_cam = (char*)mymalloc(strlen(arg1)+1);
        snprintf(*parm_cam, strlen(arg1)+1, "%s", arg1);
    }
}

static void conf_edit_daemon(struct ctx_motapp *motapp, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        motapp->daemon = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            motapp->daemon = TRUE;
        } else {
            motapp->daemon = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(motapp->daemon, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","daemon",_("daemon"));
}
static void conf_edit_setup_mode(struct ctx_motapp *motapp, char *arg1, int pact) {
    if (pact == PARM_ACT_DFLT){
        motapp->setup_mode = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            motapp->setup_mode = TRUE;
        } else {
            motapp->setup_mode = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(motapp->setup_mode, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","setup_mode",_("setup_mode"));
}
static void conf_edit_conf_filename(struct ctx_motapp *motapp, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        conf_edit_set_string(&motapp->conf_filename,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&motapp->conf_filename,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&motapp->conf_filename,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(motapp->conf_filename,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_file",_("log_file"));
}
static void conf_edit_pid_file(struct ctx_motapp *motapp, char *arg1, int pact) {
    if (pact == PARM_ACT_DFLT) {
        conf_edit_set_string(&motapp->pid_file,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&motapp->pid_file,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&motapp->pid_file,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(motapp->pid_file,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pid_file",_("pid_file"));
}
static void conf_edit_log_file(struct ctx_motapp *motapp, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        conf_edit_set_string(&motapp->log_file,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&motapp->log_file,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&motapp->log_file,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(motapp->log_file,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_file",_("log_file"));
}
static void conf_edit_log_level(struct ctx_motapp *motapp, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        motapp->log_level = 6;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 1) || (parm_in > 9)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid log_level %d"),parm_in);
        } else {
            motapp->log_level = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(motapp->log_level, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_level",_("log_level"));
}
static void conf_edit_log_type(struct ctx_motapp *motapp, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&motapp->log_type_str,(char*)"ALL");
    } else if ((pact == PARM_ACT_FREE)){
        conf_edit_set_string(&motapp->log_type_str, NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 == NULL){
            conf_edit_set_string(&motapp->log_type_str,(char*)"ALL");
        } else {
            if (mystreq(arg1,"ALL") || mystreq(arg1,"COR") ||
                mystreq(arg1,"STR") || mystreq(arg1,"ENC") ||
                mystreq(arg1,"NET") || mystreq(arg1,"DBL") ||
                mystreq(arg1,"EVT") || mystreq(arg1,"TRK") ||
                mystreq(arg1,"VID") || mystreq(arg1,"ALL")) {
                conf_edit_set_string(&motapp->log_type_str, arg1);
            } else {
                conf_edit_set_string(&motapp->log_type_str,(char*)"ALL");
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid log_level %s"),arg1);
            }
        }
        conf_edit_set_string(&motapp->log_type_str,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(motapp->log_type_str,arg1);
    }

    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_type",_("log_type"));
}
static void conf_edit_native_language(struct ctx_motapp *motapp, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        motapp->native_language = TRUE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            motapp->native_language = TRUE;
        } else {
            motapp->native_language = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(motapp->native_language, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_changes",_("text_changes"));
}

static void conf_edit_quiet(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.quiet = TRUE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.quiet = TRUE;
        } else {
            cam->conf.quiet = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.quiet, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","quiet",_("quiet"));
}
static void conf_edit_camera_name(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        conf_edit_set_string(&cam->conf.camera_name,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.camera_name,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.camera_name,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.camera_name,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera_name",_("camera_name"));
}
static void conf_edit_camera_id(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.camera_id = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if (parm_in < 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid camera_id %d"),parm_in);
        } else {
            cam->conf.camera_id = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.camera_id, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera_id",_("camera_id"));
}
static void conf_edit_camera_dir(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        conf_edit_set_string(&cam->conf.camera_dir,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.camera_dir,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.camera_dir,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.camera_dir,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera_dir",_("camera_dir"));
}
static void conf_edit_target_dir(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.target_dir,(char*)".");
    } else if ((pact == PARM_ACT_FREE)){
        conf_edit_set_string(&cam->conf.target_dir,NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 == NULL){
            conf_edit_set_string(&cam->conf.target_dir,(char*)".");
        } else {
            conf_edit_set_string(&cam->conf.target_dir,arg1);
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.target_dir,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","target_dir",_("target_dir"));
}
static void conf_edit_videodevice(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.videodevice,(char*)"/dev/video0");
    } else if ((pact == PARM_ACT_FREE)){
        conf_edit_set_string(&cam->conf.videodevice,NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 == NULL){
            conf_edit_set_string(&cam->conf.videodevice,(char*)"/dev/video0");
        } else {
            conf_edit_set_string(&cam->conf.videodevice,arg1);
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.videodevice,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","videodevice",_("videodevice"));
}
static void conf_edit_vid_control_params(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.vid_control_params,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.vid_control_params,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.vid_control_params,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.vid_control_params,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","vid_control_params",_("vid_control_params"));
}
static void conf_edit_v4l2_palette(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.v4l2_palette = 17;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in >21)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid v4l2_palette %d"),parm_in);
        } else {
            cam->conf.v4l2_palette = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.v4l2_palette, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","v4l2_palette",_("v4l2_palette"));
}
static void conf_edit_input(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.input = -1;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < -1) || (parm_in > 7)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid input %d"),parm_in);
        } else {
            cam->conf.input = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.input, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","input",_("input"));
}
static void conf_edit_norm(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.norm = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 3)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid norm %d"),parm_in);
        } else {
            cam->conf.norm = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.norm, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","norm",_("norm"));
}
static void conf_edit_frequency(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.frequency = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 999999)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid frequency %d"),parm_in);
        } else {
            cam->conf.frequency = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.frequency, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","frequency",_("frequency"));
}
static void conf_edit_auto_brightness(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.auto_brightness = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 3)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid auto_brightness %d"),parm_in);
        } else {
            cam->conf.auto_brightness = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.auto_brightness, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","auto_brightness",_("auto_brightness"));
}
static void conf_edit_tuner_device(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.tuner_device,(char*)"/dev/tuner0");
    } else if ((pact == PARM_ACT_FREE)){
        conf_edit_set_string(&cam->conf.tuner_device,NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 == NULL){
            conf_edit_set_string(&cam->conf.tuner_device,(char*)"/dev/tuner0");
        } else {
            conf_edit_set_string(&cam->conf.tuner_device,arg1);
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.tuner_device,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","tuner_device",_("tuner_device"));
}
static void conf_edit_roundrobin_frames(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.roundrobin_frames = 1;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 1) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid roundrobin_frames %d"),parm_in);
        } else {
            cam->conf.roundrobin_frames = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.roundrobin_frames, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","roundrobin_frames",_("roundrobin_frames"));
}
static void conf_edit_roundrobin_skip(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.roundrobin_skip = 1;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 1) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid roundrobin_skip %d"),parm_in);
        } else {
            cam->conf.roundrobin_skip = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.roundrobin_skip, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","roundrobin_skip",_("roundrobin_skip"));
}
static void conf_edit_roundrobin_switchfilter(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.roundrobin_switchfilter = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.roundrobin_switchfilter = TRUE;
        } else {
            cam->conf.roundrobin_switchfilter = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.roundrobin_switchfilter, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","roundrobin_switchfilter",_("roundrobin_switchfilter"));
}
static void conf_edit_netcam_url(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        conf_edit_set_string(&cam->conf.netcam_url,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.netcam_url,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.netcam_url,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.netcam_url,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_url",_("netcam_url"));
}
static void conf_edit_netcam_highres(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        conf_edit_set_string(&cam->conf.netcam_highres,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.netcam_highres,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.netcam_highres,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.netcam_highres,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_highres",_("netcam_highres"));
}
static void conf_edit_netcam_userpass(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        conf_edit_set_string(&cam->conf.netcam_userpass,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.netcam_userpass,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.netcam_userpass,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.netcam_userpass,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_userpass",_("netcam_userpass"));
}
static void conf_edit_netcam_use_tcp(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.netcam_use_tcp = TRUE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.netcam_use_tcp = TRUE;
        } else {
            cam->conf.netcam_use_tcp = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.netcam_use_tcp, arg1);
    }
    return;
}
static void conf_edit_mmalcam_name(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.mmalcam_name,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.mmalcam_name,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.mmalcam_name,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.mmalcam_name,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mmalcam_name",_("mmalcam_name"));
}
static void conf_edit_mmalcam_control_params(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        conf_edit_set_string(&cam->conf.mmalcam_control_params,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.mmalcam_control_params,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.mmalcam_control_params,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.mmalcam_control_params,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mmalcam_control_params",_("mmalcam_control_params"));
}
static void conf_edit_width(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.width = 640;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 64) || (parm_in > 9999)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid width %d"),parm_in);
        } else {
            cam->conf.width = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.width, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","width",_("width"));
}
static void conf_edit_height(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.height = 480;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 64) || (parm_in > 9999)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid height %d"),parm_in);
        } else {
            cam->conf.height = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.height, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","height",_("height"));
}
static void conf_edit_framerate(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.framerate = 15;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 2) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid framerate %d"),parm_in);
        } else {
            cam->conf.framerate = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.framerate, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","framerate",_("framerate"));
}
static void conf_edit_minimum_frame_time(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.minimum_frame_time = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid minimum_frame_time %d"),parm_in);
        } else {
            cam->conf.minimum_frame_time = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.minimum_frame_time, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","minimum_frame_time",_("minimum_frame_time"));
}
static void conf_edit_rotate(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.rotate = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in != 0) && (parm_in != 90) &&
            (parm_in != 180) && (parm_in != 270) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid rotate %d"),parm_in);
        } else {
            cam->conf.rotate = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.rotate, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","rotate",_("rotate"));
}
static void conf_edit_flip_axis(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.flip_axis,(char*)"none");
    } else if ((pact == PARM_ACT_FREE)){
        conf_edit_set_string(&cam->conf.flip_axis,NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 == NULL){
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid flip_axis %s"),arg1);
        } else {
            if (mystreq(arg1,"none") || mystreq(arg1,"v") || mystreq(arg1,"h")) {
                conf_edit_set_string(&cam->conf.flip_axis, arg1);
            } else {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid flip_axis %s"),arg1);
            }
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.flip_axis,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","flip_axis",_("flip_axis"));
}
static void conf_edit_locate_motion_mode(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.locate_motion_mode,(char*)"off");
    } else if ((pact == PARM_ACT_FREE)){
        conf_edit_set_string(&cam->conf.locate_motion_mode,NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 == NULL){
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid locate_motion_mode %s"),arg1);
        } else {
            if (mystreq(arg1,"off") || mystreq(arg1,"on") || mystreq(arg1,"preview")) {
                conf_edit_set_string(&cam->conf.locate_motion_mode, arg1);
            } else {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid locate_motion_mode %s"),arg1);
            }
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.locate_motion_mode,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","locate_motion_mode",_("locate_motion_mode"));
}
static void conf_edit_locate_motion_style(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.locate_motion_style,(char*)"box");
    } else if ((pact == PARM_ACT_FREE)){
        conf_edit_set_string(&cam->conf.locate_motion_style,NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 == NULL){
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid locate_motion_style %s"),arg1);
        } else {
            if (mystreq(arg1,"box") || mystreq(arg1,"redbox") ||
                mystreq(arg1,"cross") || mystreq(arg1,"redcross"))  {
                conf_edit_set_string(&cam->conf.locate_motion_style, arg1);
            } else {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid locate_motion_style %s"),arg1);
            }
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.locate_motion_style,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","locate_motion_style",_("locate_motion_style"));
}
static void conf_edit_text_left(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.text_left,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.text_left,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.text_left,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.text_left,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_left",_("text_left"));
}
static void conf_edit_text_right(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.text_right,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.text_right,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.text_right,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.text_right,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_right",_("text_right"));
}
static void conf_edit_text_changes(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.text_changes = TRUE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.text_changes = TRUE;
        } else {
            cam->conf.text_changes = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.text_changes, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_changes",_("text_changes"));
}
static void conf_edit_text_scale(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.text_scale = 1;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 1) || (parm_in > 10)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid text_scale %d"),parm_in);
        } else {
            cam->conf.text_scale = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.text_scale, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_scale",_("text_scale"));
}
static void conf_edit_text_event(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.text_event,(char*)"%Y%m%d%H%M%S");
    } else if ((pact == PARM_ACT_FREE)){
        conf_edit_set_string(&cam->conf.text_event,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.text_event,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.text_event,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_event",_("text_event"));
}

static void conf_edit_emulate_motion(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.emulate_motion = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.emulate_motion = TRUE;
        } else {
            cam->conf.emulate_motion = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.emulate_motion, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","emulate_motion",_("emulate_motion"));
}
static void conf_edit_threshold(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.threshold = 1500;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 1) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold %d"),parm_in);
        } else {
            cam->conf.threshold = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.threshold, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold",_("threshold"));
}
static void conf_edit_threshold_maximum(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.threshold_maximum = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_maximum %d"),parm_in);
        } else {
            cam->conf.threshold_maximum = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.threshold_maximum, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_maximum",_("threshold_maximum"));
}
static void conf_edit_threshold_tune(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.threshold_tune = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.threshold_tune = TRUE;
        } else {
            cam->conf.threshold_tune = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.threshold_tune, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_tune",_("threshold_tune"));
}
static void conf_edit_noise_level(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.noise_level = 32;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 1) || (parm_in > 255)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid noise_level %d"),parm_in);
        } else {
            cam->conf.noise_level = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.noise_level, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","noise_level",_("noise_level"));
}
static void conf_edit_noise_tune(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.noise_tune = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.noise_tune = TRUE;
        } else {
            cam->conf.noise_tune = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.noise_tune, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","noise_tune",_("noise_tune"));
}
static void conf_edit_despeckle_filter(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        conf_edit_set_string(&cam->conf.despeckle_filter,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.despeckle_filter,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.despeckle_filter,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.despeckle_filter,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","despeckle_filter",_("despeckle_filter"));
}
static void conf_edit_area_detect(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.area_detect,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.area_detect,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.area_detect,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.area_detect,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","area_detect",_("area_detect"));
}
static void conf_edit_mask_file(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.mask_file,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.mask_file,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.mask_file,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.mask_file,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mask_file",_("mask_file"));
}
static void conf_edit_mask_privacy(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) {
        conf_edit_set_string(&cam->conf.mask_privacy,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.mask_privacy,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.mask_privacy,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.mask_privacy,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mask_privacy",_("mask_privacy"));
}
static void conf_edit_smart_mask_speed(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.smart_mask_speed = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 10)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid smart_mask_speed %d"),parm_in);
        } else {
            cam->conf.smart_mask_speed = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.smart_mask_speed, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","smart_mask_speed",_("smart_mask_speed"));
}
static void conf_edit_lightswitch_percent(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.lightswitch_percent = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid lightswitch_percent %d"),parm_in);
        } else {
            cam->conf.lightswitch_percent = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.lightswitch_percent, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","lightswitch_percent",_("lightswitch_percent"));
}
static void conf_edit_lightswitch_frames(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.lightswitch_frames = 5;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 1) || (parm_in > 1000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid lightswitch_frames %d"),parm_in);
        } else {
            cam->conf.lightswitch_frames = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.lightswitch_frames, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","lightswitch_frames",_("lightswitch_frames"));
}
static void conf_edit_minimum_motion_frames(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.minimum_motion_frames = 1;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 1) || (parm_in > 10000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid minimum_motion_frames %d"),parm_in);
        } else {
            cam->conf.minimum_motion_frames = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.minimum_motion_frames, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","minimum_motion_frames",_("minimum_motion_frames"));
}
static void conf_edit_event_gap(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.event_gap = 60;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid event_gap %d"),parm_in);
        } else {
            cam->conf.event_gap = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.event_gap, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","event_gap",_("event_gap"));
}
static void conf_edit_pre_capture(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.pre_capture = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 10000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid pre_capture %d"),parm_in);
        } else {
            cam->conf.pre_capture = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.pre_capture, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pre_capture",_("pre_capture"));
}
static void conf_edit_post_capture(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.post_capture = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid post_capture %d"),parm_in);
        } else {
            cam->conf.post_capture = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.post_capture, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","post_capture",_("post_capture"));
}

static void conf_edit_on_event_start(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.on_event_start,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.on_event_start,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.on_event_start,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.on_event_start,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_event_start",_("on_event_start"));
}
static void conf_edit_on_event_end(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.on_event_end,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.on_event_end,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.on_event_end,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.on_event_end,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_event_end",_("on_event_end"));
}
static void conf_edit_on_picture_save(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.on_picture_save,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.on_picture_save,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.on_picture_save,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.on_picture_save,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_picture_save",_("on_picture_save"));
}
static void conf_edit_on_area_detected(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.on_area_detected,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.on_area_detected,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.on_area_detected,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.on_area_detected,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_area_detected",_("on_area_detected"));
}
static void conf_edit_on_motion_detected(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.on_motion_detected,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.on_motion_detected,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.on_motion_detected,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.on_motion_detected,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_motion_detected",_("on_motion_detected"));
}
static void conf_edit_on_movie_start(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.on_movie_start,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.on_movie_start,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.on_movie_start,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.on_movie_start,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_movie_start",_("on_movie_start"));
}
static void conf_edit_on_movie_end(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.on_movie_end,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.on_movie_end,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.on_movie_end,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.on_movie_end,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_movie_end",_("on_movie_end"));
}
static void conf_edit_on_camera_lost(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.on_camera_lost,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.on_camera_lost,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.on_camera_lost,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.on_camera_lost,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_camera_lost",_("on_camera_lost"));
}
static void conf_edit_on_camera_found(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.on_camera_found,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.on_camera_found,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.on_camera_found,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.on_camera_found,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_camera_found",_("on_camera_found"));
}

static void conf_edit_picture_output(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.picture_output,(char*)"off");
    } else if ((pact == PARM_ACT_FREE)){
        conf_edit_set_string(&cam->conf.picture_output,NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 == NULL){
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_output %s"),arg1);
        } else {
            if (mystreq(arg1,"on") || mystreq(arg1,"off") ||
                mystreq(arg1,"first") || mystreq(arg1,"best"))  {
                conf_edit_set_string(&cam->conf.picture_output, arg1);
            } else {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_output %s"),arg1);
            }
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.picture_output,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_output",_("picture_output"));
}
static void conf_edit_picture_output_motion(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.picture_output_motion = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.picture_output_motion = TRUE;
        } else {
            cam->conf.picture_output_motion = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.picture_output_motion, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_output_motion",_("picture_output_motion"));
}
static void conf_edit_picture_type(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact)  {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.picture_type,(char*)"jpeg");
    } else if ((pact == PARM_ACT_FREE)){
        conf_edit_set_string(&cam->conf.picture_type,NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 == NULL){
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_type %s"),arg1);
        } else {
            if (mystreq(arg1,"jpeg") || mystreq(arg1,"webp") ||
                mystreq(arg1,"ppm"))  {
                conf_edit_set_string(&cam->conf.picture_type, arg1);
            } else {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_type %s"),arg1);
            }
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.picture_type,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_type",_("picture_type"));
}
static void conf_edit_picture_quality(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.picture_quality = 75;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 1) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_quality %d"),parm_in);
        } else {
            cam->conf.picture_quality = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.picture_quality, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_quality",_("picture_quality"));
}
static void conf_edit_picture_exif(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.picture_exif,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.picture_exif,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.picture_exif,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.picture_exif,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_exif",_("picture_exif"));
}
static void conf_edit_picture_filename(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.picture_filename,(char*)"%v-%Y%m%d%H%M%S-%q");
    } else if ((pact == PARM_ACT_FREE)){
        conf_edit_set_string(&cam->conf.picture_filename,NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 != NULL){
            conf_edit_set_string(&cam->conf.picture_filename,arg1);
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.picture_filename,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_filename",_("picture_filename"));
}
static void conf_edit_snapshot_interval(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.snapshot_interval = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid snapshot_interval %d"),parm_in);
        } else {
            cam->conf.snapshot_interval = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.snapshot_interval, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snapshot_interval",_("snapshot_interval"));
}
static void conf_edit_snapshot_filename(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.snapshot_filename,(char*)"%v-%Y%m%d%H%M%S-snapshot");
    } else if ((pact == PARM_ACT_FREE)){
        conf_edit_set_string(&cam->conf.snapshot_filename,NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 != NULL){
            conf_edit_set_string(&cam->conf.snapshot_filename,arg1);
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.snapshot_filename,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snapshot_filename",_("snapshot_filename"));
}

static void conf_edit_movie_output(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.movie_output = TRUE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.movie_output = TRUE;
        } else {
            cam->conf.movie_output = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.movie_output, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_output",_("movie_output"));
}
static void conf_edit_movie_output_motion(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.movie_output_motion = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.movie_output_motion = TRUE;
        } else {
            cam->conf.movie_output_motion = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.movie_output_motion, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_output_motion",_("movie_output_motion"));
}
static void conf_edit_movie_max_time(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.movie_max_time = 120;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid movie_max_time %d"),parm_in);
        } else {
            cam->conf.movie_max_time = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.movie_max_time, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_max_time",_("movie_max_time"));
}
static void conf_edit_movie_bps(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.movie_bps = 400000;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 9999999)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid movie_bps %d"),parm_in);
        } else {
            cam->conf.movie_bps = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.movie_bps, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_bps",_("movie_bps"));
}
static void conf_edit_movie_quality(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.movie_quality = 60;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid movie_quality %d"),parm_in);
        } else {
            cam->conf.movie_quality = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.movie_quality, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_quality",_("movie_quality"));
}
static void conf_edit_movie_codec(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact)  {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.movie_codec,(char*)"mkv");
    } else if ((pact == PARM_ACT_FREE)){
        conf_edit_set_string(&cam->conf.movie_codec,NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 != NULL){
            conf_edit_set_string(&cam->conf.movie_codec,arg1);
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.movie_codec,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_codec",_("movie_codec"));
}
static void conf_edit_movie_passthrough(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.movie_passthrough = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.movie_passthrough = TRUE;
        } else {
            cam->conf.movie_passthrough = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.movie_passthrough, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_passthrough",_("movie_passthrough"));
}
static void conf_edit_movie_filename(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.movie_filename,(char*)"%v-%Y%m%d%H%M%S");
    } else if ((pact == PARM_ACT_FREE)){
        conf_edit_set_string(&cam->conf.movie_filename,NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 != NULL){
            conf_edit_set_string(&cam->conf.movie_filename,arg1);
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.movie_filename,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_filename",_("movie_filename"));
}
static void conf_edit_movie_extpipe_use(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.movie_extpipe_use = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.movie_extpipe_use = TRUE;
        } else {
            cam->conf.movie_extpipe_use = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.movie_extpipe_use, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_extpipe_use",_("movie_extpipe_use"));
}
static void conf_edit_movie_extpipe(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact){
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.movie_extpipe,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.movie_extpipe,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.movie_extpipe,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.movie_extpipe,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_extpipe",_("movie_extpipe"));
}

static void conf_edit_timelapse_interval(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.timelapse_interval = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_interval %d"),parm_in);
        } else {
            cam->conf.timelapse_interval = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.timelapse_interval, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_interval",_("timelapse_interval"));
}
static void conf_edit_timelapse_mode(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact)  {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.timelapse_mode,(char*)"daily");
    } else if ((pact == PARM_ACT_FREE)){
        conf_edit_set_string(&cam->conf.timelapse_mode,NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 == NULL){
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_mode %s"),arg1);
        } else {
            if (mystreq(arg1,"hourly") || mystreq(arg1,"daily") ||
                mystreq(arg1,"weekly-sunday") || mystreq(arg1,"weekly-monday") ||
                mystreq(arg1,"monthly") || mystreq(arg1,"manual"))  {
                conf_edit_set_string(&cam->conf.timelapse_mode, arg1);
            } else {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_mode %s"),arg1);
            }
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.timelapse_mode,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_mode",_("timelapse_mode"));
}
static void conf_edit_timelapse_fps(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.timelapse_fps = 30;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 2) || (parm_in > 1000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_fps %d"),parm_in);
        } else {
            cam->conf.timelapse_fps = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.timelapse_fps, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_fps",_("timelapse_fps"));
}
static void conf_edit_timelapse_codec(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.timelapse_codec,(char*)"mpg");
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.timelapse_codec,NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 == NULL){
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_codec %s"),arg1);
        } else {
            if (mystreq(arg1,"mpg") || mystreq(arg1,"mpeg4"))  {
                conf_edit_set_string(&cam->conf.timelapse_codec, arg1);
            } else {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_codec %s"),arg1);
            }
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.timelapse_codec,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_codec",_("timelapse_codec"));
}
static void conf_edit_timelapse_filename(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.timelapse_filename,(char*)"%Y%m%d-timelapse");
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.timelapse_filename,NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 != NULL){
            conf_edit_set_string(&cam->conf.timelapse_filename,arg1);
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.timelapse_filename,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_filename",_("timelapse_filename"));
}

static void conf_edit_video_pipe(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.video_pipe,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.video_pipe,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.video_pipe,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.video_pipe,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","video_pipe",_("video_pipe"));
}
static void conf_edit_video_pipe_motion(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.video_pipe_motion,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.video_pipe_motion,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.video_pipe_motion,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.video_pipe_motion,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","video_pipe_motion",_("video_pipe_motion"));
}

static void conf_edit_webcontrol_port(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.webcontrol_port = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 65535)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_port %d"),parm_in);
        } else {
            cam->conf.webcontrol_port = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.webcontrol_port, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_port",_("webcontrol_port"));
}
static void conf_edit_webcontrol_ipv6(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.webcontrol_ipv6 = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.webcontrol_ipv6 = TRUE;
        } else {
            cam->conf.webcontrol_ipv6 = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.webcontrol_ipv6, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_ipv6",_("webcontrol_ipv6"));
}
static void conf_edit_webcontrol_localhost(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact)  {
    if (pact == PARM_ACT_DFLT){
        cam->conf.webcontrol_localhost = TRUE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.webcontrol_localhost = TRUE;
        } else {
            cam->conf.webcontrol_localhost = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.webcontrol_localhost, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_localhost",_("webcontrol_localhost"));
}
static void conf_edit_webcontrol_parms(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.webcontrol_parms = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 3)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_parms %d"),parm_in);
        } else {
            cam->conf.webcontrol_parms = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.webcontrol_parms, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_parms",_("webcontrol_parms"));
}
static void conf_edit_webcontrol_interface(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.webcontrol_interface = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 2)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_interface %d"),parm_in);
        } else {
            cam->conf.webcontrol_interface = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.webcontrol_interface, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_interface",_("webcontrol_interface"));
}
static void conf_edit_webcontrol_auth_method(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.webcontrol_auth_method = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 2)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_auth_method %d"),parm_in);
        } else {
            cam->conf.webcontrol_auth_method = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.webcontrol_auth_method, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_auth_method",_("webcontrol_auth_method"));
}
static void conf_edit_webcontrol_authentication(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.webcontrol_authentication,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.webcontrol_authentication,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.webcontrol_authentication,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.webcontrol_authentication,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_authentication",_("webcontrol_authentication"));
}
static void conf_edit_webcontrol_tls(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.webcontrol_tls = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.webcontrol_tls = TRUE;
        } else {
            cam->conf.webcontrol_tls = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.webcontrol_tls, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_tls",_("webcontrol_tls"));
}
static void conf_edit_webcontrol_cert(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.webcontrol_cert,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.webcontrol_cert,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.webcontrol_cert,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.webcontrol_cert,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_cert",_("webcontrol_cert"));
}
static void conf_edit_webcontrol_key(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.webcontrol_key,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.webcontrol_key,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.webcontrol_key,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.webcontrol_key,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_key",_("webcontrol_key"));
}
static void conf_edit_webcontrol_cors_header(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.webcontrol_cors_header,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.webcontrol_cors_header,NULL);
    } else if (pact == PARM_ACT_SET){
        // A complicated regex to validate a url found here:
        // https://stackoverflow.com/questions/38608116/how-to-check-a-specified-string-is-a-valid-url-or-not-using-c-code
        const char *regex_str = "^(https?:\\/\\/)?([\\da-z\\.-]+)\\.([a-z\\.]{2,6})([\\/\\w \\.-]*)*\\/?$";
        regex_t regex;
        if (regcomp(&regex, regex_str, REG_EXTENDED) != 0) {
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,_("Error compiling regex in copy_uri"));
            return;
        }
        // A single asterisk is also valid
        if (mystrne(arg1, "*") && regexec(&regex, arg1, 0, NULL, 0) == REG_NOMATCH) {
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,_("Invalid origin for cors_header"));
            regfree(&regex);
            return;
        }
        regfree(&regex);
        conf_edit_set_string(&cam->conf.webcontrol_cors_header,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.webcontrol_cors_header,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_cors_header",_("webcontrol_cors_header"));
}

static void conf_edit_stream_port(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.stream_port = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 65535)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_port %d"),parm_in);
        } else {
            cam->conf.stream_port = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.stream_port, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_port",_("stream_port"));
}
static void conf_edit_stream_localhost(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.stream_localhost = TRUE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.stream_localhost = TRUE;
        } else {
            cam->conf.stream_localhost = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.stream_localhost, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_localhost",_("stream_localhost"));
}
static void conf_edit_stream_auth_method(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.stream_auth_method = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 2)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_auth_method %d"),parm_in);
        } else {
            cam->conf.stream_auth_method = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.stream_auth_method, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_auth_method",_("stream_auth_method"));
}
static void conf_edit_stream_authentication(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact)  {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.stream_authentication,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.stream_authentication,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.stream_authentication,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.stream_authentication,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_authentication",_("stream_authentication"));
}
static void conf_edit_stream_tls(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.stream_tls = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.stream_tls = TRUE;
        } else {
            cam->conf.stream_tls = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.stream_tls, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_tls",_("stream_tls"));
}
static void conf_edit_stream_cors_header(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.stream_cors_header,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.stream_cors_header,NULL);
    } else if (pact == PARM_ACT_SET){
        // A complicated regex to validate a url found here:
        // https://stackoverflow.com/questions/38608116/how-to-check-a-specified-string-is-a-valid-url-or-not-using-c-code
        const char *regex_str = "^(https?:\\/\\/)?([\\da-z\\.-]+)\\.([a-z\\.]{2,6})([\\/\\w \\.-]*)*\\/?$";
        regex_t regex;
        if (regcomp(&regex, regex_str, REG_EXTENDED) != 0) {
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,_("Error compiling regex in copy_uri"));
            return;
        }
        // A single asterisk is also valid
        if (mystrne(arg1, "*") && regexec(&regex, arg1, 0, NULL, 0) == REG_NOMATCH) {
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,_("Invalid origin for cors_header"));
            regfree(&regex);
            return;
        }
        regfree(&regex);
        conf_edit_set_string(&cam->conf.stream_cors_header,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.stream_cors_header,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_cors_header",_("stream_cors_header"));
}
static void conf_edit_stream_preview_scale(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.stream_preview_scale = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 1) || (parm_in > 1000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_preview_scale %d"),parm_in);
        } else {
            cam->conf.stream_preview_scale = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.stream_preview_scale, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_scale",_("stream_preview_scale"));
}
static void conf_edit_stream_preview_newline(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.stream_preview_newline = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.stream_preview_newline = TRUE;
        } else {
            cam->conf.stream_preview_newline = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.stream_preview_newline, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_newline",_("stream_preview_newline"));
}
static void conf_edit_stream_preview_method(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.stream_preview_method = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 4)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_preview_method %d"),parm_in);
        } else {
            cam->conf.stream_preview_method = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.stream_preview_method, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_method",_("stream_preview_method"));
}
static void conf_edit_stream_quality(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.stream_quality = 50;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 1) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_quality %d"),parm_in);
        } else {
            cam->conf.stream_quality = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.stream_quality, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_quality",_("stream_quality"));
}
static void conf_edit_stream_grey(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.stream_grey = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.stream_grey = TRUE;
        } else {
            cam->conf.stream_grey = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.stream_grey, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_grey",_("stream_grey"));
}
static void conf_edit_stream_motion(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.stream_motion = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.stream_motion = TRUE;
        } else {
            cam->conf.stream_motion = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.stream_motion, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_motion",_("stream_motion"));
}
static void conf_edit_stream_maxrate(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.stream_maxrate = 1;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 1) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_maxrate %d"),parm_in);
        } else {
            cam->conf.stream_maxrate = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.stream_maxrate, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_maxrate",_("stream_maxrate"));
}
static void conf_edit_database_type(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.database_type,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.database_type,NULL);
    } else if (pact == PARM_ACT_SET){
        if (arg1 == NULL){
            conf_edit_set_string(&cam->conf.database_type, NULL);
        } else {
            if (mystreq(arg1,"mysql") || mystreq(arg1,"postgresql") || mystreq(arg1,"sqlite3")) {
                conf_edit_set_string(&cam->conf.database_type, arg1);
            } else {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid database_type %s"),arg1);
            }
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.database_type,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_type",_("database_type"));
}
static void conf_edit_database_dbname(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.database_dbname,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.database_dbname,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.database_dbname,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.database_dbname,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_dbname",_("database_dbname"));
}
static void conf_edit_database_host(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.database_host,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.database_host,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.database_host,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.database_host,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_host",_("database_host"));
}
static void conf_edit_database_port(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.database_port = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 65535)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid database_port %d"),parm_in);
        } else {
            cam->conf.database_port = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.database_port, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_port",_("database_port"));
}
static void conf_edit_database_user(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.database_user,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.database_user,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.database_user,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.database_user,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_user",_("database_user"));
}
static void conf_edit_database_password(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.database_password,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.database_password,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.database_password,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.database_password,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_password",_("database_password"));
}
static void conf_edit_database_busy_timeout(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.database_busy_timeout = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 10000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid database_busy_timeout %d"),parm_in);
        } else {
            cam->conf.database_busy_timeout = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.database_busy_timeout, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_busy_timeout",_("database_busy_timeout"));
}

static void conf_edit_sql_log_picture(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.sql_log_picture = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.sql_log_picture = TRUE;
        } else {
            cam->conf.sql_log_picture = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.sql_log_picture, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_log_picture",_("sql_log_picture"));
}
static void conf_edit_sql_log_snapshot(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        cam->conf.sql_log_snapshot = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.sql_log_snapshot = TRUE;
        } else {
            cam->conf.sql_log_snapshot = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.sql_log_snapshot, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_log_snapshot",_("sql_log_snapshot"));
}
static void conf_edit_sql_log_movie(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact)  {
    if (pact == PARM_ACT_DFLT){
        cam->conf.sql_log_movie = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.sql_log_movie = TRUE;
        } else {
            cam->conf.sql_log_movie = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.sql_log_movie, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_log_movie",_("sql_log_movie"));
}
static void conf_edit_sql_log_timelapse(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact)   {
    if (pact == PARM_ACT_DFLT){
        cam->conf.sql_log_timelapse = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.sql_log_timelapse = TRUE;
        } else {
            cam->conf.sql_log_timelapse = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.sql_log_timelapse, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_log_timelapse",_("sql_log_timelapse"));
}
static void conf_edit_sql_query_start(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.sql_query_start,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.sql_query_start,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.sql_query_start,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.sql_query_start,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_query_start",_("sql_query_start"));
}
static void conf_edit_sql_query_stop(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.sql_query_stop,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.sql_query_stop,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.sql_query_stop,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.sql_query_stop,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_query_stop",_("sql_query_stop"));
}
static void conf_edit_sql_query(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact)  {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.sql_query,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.sql_query,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.sql_query,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.sql_query,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_query",_("sql_query"));
}

static void conf_edit_track_type(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.track_type = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 5)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid track_type %d"),parm_in);
        } else {
            cam->conf.track_type = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.track_type, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_type",_("track_type"));
}
static void conf_edit_track_auto(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact)  {
    if (pact == PARM_ACT_DFLT){
        cam->conf.track_auto = FALSE;
    } else if (pact == PARM_ACT_SET){
        if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
            cam->conf.track_auto = TRUE;
        } else {
            cam->conf.track_auto = FALSE;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_bool(cam->conf.track_auto, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_auto",_("track_auto"));
}
static void conf_edit_track_move_wait(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.track_move_wait = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid track_move_wait %d"),parm_in);
        } else {
            cam->conf.track_move_wait = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.track_move_wait, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_move_wait",_("track_move_wait"));
}
static void conf_edit_track_generic_move(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact)  {
    if (pact == PARM_ACT_DFLT){
        conf_edit_set_string(&cam->conf.track_generic_move,NULL);
    } else if (pact == PARM_ACT_FREE){
        conf_edit_set_string(&cam->conf.track_generic_move,NULL);
    } else if (pact == PARM_ACT_SET){
        conf_edit_set_string(&cam->conf.track_generic_move,arg1);
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_string(cam->conf.track_generic_move,arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_generic_move",_("track_generic_move"));
}
static void conf_edit_track_step_angle_x(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.track_step_angle_x = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid track_step_angle_x %d"),parm_in);
        } else {
            cam->conf.track_step_angle_x = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.track_step_angle_x, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_step_angle_x",_("track_step_angle_x"));
}
static void conf_edit_track_step_angle_y(struct ctx_cam *cam, char *arg1, enum PARM_ACT pact) {
    int parm_in;
    if (pact == PARM_ACT_DFLT){
        cam->conf.track_step_angle_y = 0;
    } else if (pact == PARM_ACT_SET){
        parm_in = atoi(arg1);
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid track_step_angle_y %d"),parm_in);
        } else {
            cam->conf.track_step_angle_y = parm_in;
        }
    } else if (pact == PARM_ACT_GET){
        conf_edit_get_int(cam->conf.track_step_angle_y, arg1);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_step_angle_y",_("track_step_angle_y"));
}

static void conf_edit_cat00(struct ctx_motapp *motapp, const char *cmd, char *arg1, enum PARM_ACT pact) {
    if (mystreq(cmd,"daemon") ){                        conf_edit_daemon(motapp, arg1, pact);
    } else if (mystreq(cmd,"conf_filename")){           conf_edit_conf_filename(motapp, arg1, pact);
    } else if (mystreq(cmd,"setup_mode")) {             conf_edit_setup_mode(motapp, arg1, pact);
    } else if (mystreq(cmd,"pid_file")){                conf_edit_pid_file(motapp, arg1, pact);
    } else if (mystreq(cmd,"log_file")){                conf_edit_log_file(motapp, arg1, pact);
    } else if (mystreq(cmd,"log_level")){               conf_edit_log_level(motapp, arg1, pact);
    } else if (mystreq(cmd,"log_type")){                conf_edit_log_type(motapp, arg1, pact);
    } else if (mystreq(cmd,"native_language")){         conf_edit_native_language(motapp, arg1, pact);
    }

}
static void conf_edit_cat01(struct ctx_cam *cam, const char *cmd, char *arg1, enum PARM_ACT pact) {

    if (mystreq(cmd,"quiet")){                   conf_edit_quiet(cam, arg1, pact);
    } else if (mystreq(cmd,"camera_dir")){              conf_edit_camera_dir(cam, arg1, pact);
    } else if (mystreq(cmd,"camera_name")){             conf_edit_camera_name(cam, arg1, pact);
    } else if (mystreq(cmd,"camera_id")){               conf_edit_camera_id(cam, arg1, pact);
    } else if (mystreq(cmd,"target_dir")){              conf_edit_target_dir(cam, arg1, pact);
    } else if (mystreq(cmd,"videodevice") ){            conf_edit_videodevice(cam, arg1, pact);
    } else if (mystreq(cmd,"vid_control_params")) {     conf_edit_vid_control_params(cam, arg1, pact);
    } else if (mystreq(cmd,"v4l2_palette")){            conf_edit_v4l2_palette(cam, arg1, pact);
    } else if (mystreq(cmd,"input")){                   conf_edit_input(cam, arg1, pact);
    } else if (mystreq(cmd,"norm")){                    conf_edit_norm(cam, arg1, pact);
    } else if (mystreq(cmd,"frequency")){               conf_edit_frequency(cam, arg1, pact);
    } else if (mystreq(cmd,"auto_brightness")){         conf_edit_auto_brightness(cam, arg1, pact);
    } else if (mystreq(cmd,"tuner_device")){            conf_edit_tuner_device(cam, arg1, pact);
    } else if (mystreq(cmd,"roundrobin_frames")){       conf_edit_roundrobin_frames(cam, arg1, pact);
    } else if (mystreq(cmd,"roundrobin_skip")){         conf_edit_roundrobin_skip(cam, arg1, pact);
    } else if (mystreq(cmd,"roundrobin_switchfilter")){ conf_edit_roundrobin_switchfilter(cam, arg1, pact);
    } else if (mystreq(cmd,"netcam_url")){              conf_edit_netcam_url(cam, arg1, pact);
    } else if (mystreq(cmd,"netcam_highres")){          conf_edit_netcam_highres(cam, arg1, pact);
    } else if (mystreq(cmd,"netcam_userpass")){         conf_edit_netcam_userpass(cam, arg1, pact);
    } else if (mystreq(cmd,"netcam_use_tcp")){          conf_edit_netcam_use_tcp(cam, arg1, pact);
    } else if (mystreq(cmd,"mmalcam_name")){            conf_edit_mmalcam_name(cam, arg1, pact);
    } else if (mystreq(cmd,"mmalcam_control_params")){  conf_edit_mmalcam_control_params(cam, arg1, pact);
    }

}
static void conf_edit_cat02(struct ctx_cam *cam, const char *cmd, char *arg1, enum PARM_ACT pact) {

    if (mystreq(cmd,"width")){                          conf_edit_width(cam, arg1, pact);
    } else if (mystreq(cmd,"height")){                  conf_edit_height(cam, arg1, pact);
    } else if (mystreq(cmd,"framerate")){               conf_edit_framerate(cam, arg1, pact);
    } else if (mystreq(cmd,"minimum_frame_time")){      conf_edit_minimum_frame_time(cam, arg1, pact);
    } else if (mystreq(cmd,"rotate")){                  conf_edit_rotate(cam, arg1, pact);
    } else if (mystreq(cmd,"flip_axis")){               conf_edit_flip_axis(cam, arg1, pact);
    } else if (mystreq(cmd,"locate_motion_mode")){      conf_edit_locate_motion_mode(cam, arg1, pact);
    } else if (mystreq(cmd,"locate_motion_style")){     conf_edit_locate_motion_style(cam, arg1, pact);
    } else if (mystreq(cmd,"text_left")){               conf_edit_text_left(cam, arg1, pact);
    } else if (mystreq(cmd,"text_right")){              conf_edit_text_right(cam, arg1, pact);
    } else if (mystreq(cmd,"text_changes")){            conf_edit_text_changes(cam, arg1, pact);
    } else if (mystreq(cmd,"text_scale")){              conf_edit_text_scale(cam, arg1, pact);
    } else if (mystreq(cmd,"text_event")){              conf_edit_text_event(cam, arg1, pact);
    } else if (mystreq(cmd,"emulate_motion")){          conf_edit_emulate_motion(cam, arg1, pact);
    } else if (mystreq(cmd,"threshold")){               conf_edit_threshold(cam, arg1, pact);
    } else if (mystreq(cmd,"threshold_maximum")){       conf_edit_threshold_maximum(cam, arg1, pact);
    } else if (mystreq(cmd,"threshold_tune")){          conf_edit_threshold_tune(cam, arg1, pact);
    } else if (mystreq(cmd,"noise_level")){             conf_edit_noise_level(cam, arg1, pact);
    } else if (mystreq(cmd,"noise_tune")){              conf_edit_noise_tune(cam, arg1, pact);
    } else if (mystreq(cmd,"despeckle_filter")){        conf_edit_despeckle_filter(cam, arg1, pact);
    } else if (mystreq(cmd,"area_detect")){             conf_edit_area_detect(cam, arg1, pact);
    } else if (mystreq(cmd,"mask_file")){               conf_edit_mask_file(cam, arg1, pact);
    } else if (mystreq(cmd,"mask_privacy")){            conf_edit_mask_privacy(cam, arg1, pact);
    } else if (mystreq(cmd,"smart_mask_speed")){        conf_edit_smart_mask_speed(cam, arg1, pact);
    } else if (mystreq(cmd,"lightswitch_percent")){     conf_edit_lightswitch_percent(cam, arg1, pact);
    } else if (mystreq(cmd,"lightswitch_frames")){      conf_edit_lightswitch_frames(cam, arg1, pact);
    } else if (mystreq(cmd,"minimum_motion_frames")){   conf_edit_minimum_motion_frames(cam, arg1, pact);
    } else if (mystreq(cmd,"event_gap")){               conf_edit_event_gap(cam, arg1, pact);
    } else if (mystreq(cmd,"pre_capture")){             conf_edit_pre_capture(cam, arg1, pact);
    } else if (mystreq(cmd,"post_capture")){            conf_edit_post_capture(cam, arg1, pact);
    }

}
static void conf_edit_cat03(struct ctx_cam *cam, const char *cmd, char *arg1, enum PARM_ACT pact) {

    if (mystreq(cmd,"on_event_start")){                 conf_edit_on_event_start(cam, arg1, pact);
    } else if (mystreq(cmd,"on_event_end")){            conf_edit_on_event_end(cam, arg1, pact);
    } else if (mystreq(cmd,"on_picture_save")){         conf_edit_on_picture_save(cam, arg1, pact);
    } else if (mystreq(cmd,"on_area_detected")){        conf_edit_on_area_detected(cam, arg1, pact);
    } else if (mystreq(cmd,"on_motion_detected")){      conf_edit_on_motion_detected(cam, arg1, pact);
    } else if (mystreq(cmd,"on_movie_start")){          conf_edit_on_movie_start(cam, arg1, pact);
    } else if (mystreq(cmd,"on_movie_end")){            conf_edit_on_movie_end(cam, arg1, pact);
    } else if (mystreq(cmd,"on_camera_lost")){          conf_edit_on_camera_lost(cam, arg1, pact);
    } else if (mystreq(cmd,"on_camera_found")){         conf_edit_on_camera_found(cam, arg1, pact);
    } else if (mystreq(cmd,"picture_output")){          conf_edit_picture_output(cam, arg1, pact);
    } else if (mystreq(cmd,"picture_output_motion")){   conf_edit_picture_output_motion(cam, arg1, pact);
    } else if (mystreq(cmd,"picture_type")){            conf_edit_picture_type(cam, arg1, pact);
    } else if (mystreq(cmd,"picture_quality")){         conf_edit_picture_quality(cam, arg1, pact);
    } else if (mystreq(cmd,"picture_exif")){            conf_edit_picture_exif(cam, arg1, pact);
    } else if (mystreq(cmd,"picture_filename")){        conf_edit_picture_filename(cam, arg1, pact);
    } else if (mystreq(cmd,"snapshot_interval")){       conf_edit_snapshot_interval(cam, arg1, pact);
    } else if (mystreq(cmd,"snapshot_filename")){       conf_edit_snapshot_filename(cam, arg1, pact);
    } else if (mystreq(cmd,"movie_output")){            conf_edit_movie_output(cam, arg1, pact);
    } else if (mystreq(cmd,"movie_output_motion")){     conf_edit_movie_output_motion(cam, arg1, pact);
    } else if (mystreq(cmd,"movie_max_time")){          conf_edit_movie_max_time(cam, arg1, pact);
    } else if (mystreq(cmd,"movie_bps")){               conf_edit_movie_bps(cam, arg1, pact);
    } else if (mystreq(cmd,"movie_quality")){           conf_edit_movie_quality(cam, arg1, pact);
    } else if (mystreq(cmd,"movie_codec")){             conf_edit_movie_codec(cam, arg1, pact);
    } else if (mystreq(cmd,"movie_passthrough")){       conf_edit_movie_passthrough(cam, arg1, pact);
    } else if (mystreq(cmd,"movie_filename")){          conf_edit_movie_filename(cam, arg1, pact);
    } else if (mystreq(cmd,"movie_extpipe_use")){       conf_edit_movie_extpipe_use(cam, arg1, pact);
    } else if (mystreq(cmd,"movie_extpipe")){           conf_edit_movie_extpipe(cam, arg1, pact);
    } else if (mystreq(cmd,"timelapse_interval")){      conf_edit_timelapse_interval(cam, arg1, pact);
    } else if (mystreq(cmd,"timelapse_mode")){          conf_edit_timelapse_mode(cam, arg1, pact);
    } else if (mystreq(cmd,"timelapse_fps")){           conf_edit_timelapse_fps(cam, arg1, pact);
    } else if (mystreq(cmd,"timelapse_codec")){         conf_edit_timelapse_codec(cam, arg1, pact);
    } else if (mystreq(cmd,"timelapse_filename")){      conf_edit_timelapse_filename(cam, arg1, pact);
    } else if (mystreq(cmd,"video_pipe")){              conf_edit_video_pipe(cam, arg1, pact);
    } else if (mystreq(cmd,"video_pipe_motion")){       conf_edit_video_pipe_motion(cam, arg1, pact);
    }

}
static void conf_edit_cat04(struct ctx_cam *cam, const char *cmd, char *arg1, enum PARM_ACT pact) {
    if (mystreq(cmd,"webcontrol_port")){                    conf_edit_webcontrol_port(cam, arg1, pact);
    } else if (mystreq(cmd,"webcontrol_ipv6")){             conf_edit_webcontrol_ipv6(cam, arg1, pact);
    } else if (mystreq(cmd,"webcontrol_localhost")){        conf_edit_webcontrol_localhost(cam, arg1, pact);
    } else if (mystreq(cmd,"webcontrol_parms")){            conf_edit_webcontrol_parms(cam, arg1, pact);
    } else if (mystreq(cmd,"webcontrol_interface")){        conf_edit_webcontrol_interface(cam, arg1, pact);
    } else if (mystreq(cmd,"webcontrol_auth_method")){      conf_edit_webcontrol_auth_method(cam, arg1, pact);
    } else if (mystreq(cmd,"webcontrol_authentication")){   conf_edit_webcontrol_authentication(cam, arg1, pact);
    } else if (mystreq(cmd,"webcontrol_tls")){              conf_edit_webcontrol_tls(cam, arg1, pact);
    } else if (mystreq(cmd,"webcontrol_cert")){             conf_edit_webcontrol_cert(cam, arg1, pact);
    } else if (mystreq(cmd,"webcontrol_key")){              conf_edit_webcontrol_key(cam, arg1, pact);
    } else if (mystreq(cmd,"webcontrol_cors_header")){      conf_edit_webcontrol_cors_header(cam, arg1, pact);
    } else if (mystreq(cmd,"stream_port")){                 conf_edit_stream_port(cam, arg1, pact);
    } else if (mystreq(cmd,"stream_localhost")){            conf_edit_stream_localhost(cam, arg1, pact);
    } else if (mystreq(cmd,"stream_auth_method")){          conf_edit_stream_auth_method(cam, arg1, pact);
    } else if (mystreq(cmd,"stream_authentication")){       conf_edit_stream_authentication(cam, arg1, pact);
    } else if (mystreq(cmd,"stream_tls")){                  conf_edit_stream_tls(cam, arg1, pact);
    } else if (mystreq(cmd,"stream_cors_header")){          conf_edit_stream_cors_header(cam, arg1, pact);
    } else if (mystreq(cmd,"stream_preview_scale")){        conf_edit_stream_preview_scale(cam, arg1, pact);
    } else if (mystreq(cmd,"stream_preview_newline")){      conf_edit_stream_preview_newline(cam, arg1, pact);
    } else if (mystreq(cmd,"stream_preview_method")){       conf_edit_stream_preview_method(cam, arg1, pact);
    } else if (mystreq(cmd,"stream_quality")){              conf_edit_stream_quality(cam, arg1, pact);
    } else if (mystreq(cmd,"stream_grey")){                 conf_edit_stream_grey(cam, arg1, pact);
    } else if (mystreq(cmd,"stream_motion")){               conf_edit_stream_motion(cam, arg1, pact);
    } else if (mystreq(cmd,"stream_maxrate")){              conf_edit_stream_maxrate(cam, arg1, pact);
    }

}
static void conf_edit_cat05(struct ctx_cam *cam, const char *cmd, char *arg1, enum PARM_ACT pact) {
    if (mystreq(cmd,"database_type")){                  conf_edit_database_type(cam, arg1, pact);
    } else if (mystreq(cmd,"database_dbname")){         conf_edit_database_dbname(cam, arg1, pact);
    } else if (mystreq(cmd,"database_host")){           conf_edit_database_host(cam, arg1, pact);
    } else if (mystreq(cmd,"database_port")){           conf_edit_database_port(cam, arg1, pact);
    } else if (mystreq(cmd,"database_user")){           conf_edit_database_user(cam, arg1, pact);
    } else if (mystreq(cmd,"database_password")){       conf_edit_database_password(cam, arg1, pact);
    } else if (mystreq(cmd,"database_busy_timeout")){   conf_edit_database_busy_timeout(cam, arg1, pact);
    } else if (mystreq(cmd,"sql_log_picture")){         conf_edit_sql_log_picture(cam, arg1, pact);
    } else if (mystreq(cmd,"sql_log_snapshot")){        conf_edit_sql_log_snapshot(cam, arg1, pact);
    } else if (mystreq(cmd,"sql_log_movie")){           conf_edit_sql_log_movie(cam, arg1, pact);
    } else if (mystreq(cmd,"sql_log_timelapse")){       conf_edit_sql_log_timelapse(cam, arg1, pact);
    } else if (mystreq(cmd,"sql_query_start")){         conf_edit_sql_query_start(cam, arg1, pact);
    } else if (mystreq(cmd,"sql_query_stop")){          conf_edit_sql_query_stop(cam, arg1, pact);
    } else if (mystreq(cmd,"sql_query")){               conf_edit_sql_query(cam, arg1, pact);
    } else if (mystreq(cmd,"track_type")){              conf_edit_track_type(cam, arg1, pact);
    } else if (mystreq(cmd,"track_auto")){              conf_edit_track_auto(cam, arg1, pact);
    } else if (mystreq(cmd,"track_move_wait")){         conf_edit_track_move_wait(cam, arg1, pact);
    } else if (mystreq(cmd,"track_generic_move")){      conf_edit_track_generic_move(cam, arg1, pact);
    } else if (mystreq(cmd,"track_step_angle_x")){      conf_edit_track_step_angle_x(cam, arg1, pact);
    } else if (mystreq(cmd,"track_step_angle_y")){      conf_edit_track_step_angle_y(cam, arg1, pact);
    }

}

void conf_edit_free(struct ctx_cam *cam) {
    int indx;
    enum PARM_CAT pcat;

    indx = 0;
    while (config_parms[indx].parm_name != NULL) {
        if (config_parms[indx].parm_type == PARM_TYP_STRING) {
            pcat = config_parms[indx].parm_cat;
            if (pcat == PARM_CAT_00) {
                conf_edit_cat00(cam->motapp, config_parms[indx].parm_name, NULL,PARM_ACT_FREE);
            } else if (pcat == PARM_CAT_01) {
                conf_edit_cat01(cam, config_parms[indx].parm_name, NULL,PARM_ACT_FREE);
            } else if (pcat == PARM_CAT_02) {
                conf_edit_cat02(cam, config_parms[indx].parm_name, NULL, PARM_ACT_FREE);
            } else if (pcat == PARM_CAT_03) {
                conf_edit_cat03(cam, config_parms[indx].parm_name, NULL, PARM_ACT_FREE);
            } else if (pcat == PARM_CAT_04) {
                conf_edit_cat04(cam, config_parms[indx].parm_name, NULL, PARM_ACT_FREE);
            } else if (pcat == PARM_CAT_05) {
                conf_edit_cat05(cam, config_parms[indx].parm_name, NULL, PARM_ACT_FREE);
            }
        }
        indx++;
    }
}

void conf_edit_dflt_app(struct ctx_motapp *motapp) {
    motapp->conf_filename = NULL;
    motapp->log_file = NULL;
    motapp->log_type_str = NULL;
    motapp->pid_file = NULL;
    conf_edit_daemon(motapp, NULL,PARM_ACT_DFLT);
    conf_edit_setup_mode(motapp, NULL, PARM_ACT_DFLT);
    conf_edit_pid_file(motapp, NULL, PARM_ACT_DFLT);
    conf_edit_log_file(motapp, NULL, PARM_ACT_DFLT);
    conf_edit_log_level(motapp, NULL, PARM_ACT_DFLT);
    conf_edit_log_type(motapp, NULL, PARM_ACT_DFLT);
    conf_edit_native_language(motapp, NULL, PARM_ACT_DFLT);

}
void conf_edit_dflt_cam(struct ctx_cam *cam) {
    int indx;
    enum PARM_CAT pcat;

    indx = 0;
    while (config_parms[indx].parm_name != NULL) {
        pcat = config_parms[indx].parm_cat;
        if ((config_parms[indx].parm_cat != PARM_CAT_00)) {
            if (pcat == PARM_CAT_01) {
                conf_edit_cat01(cam, config_parms[indx].parm_name, NULL,PARM_ACT_DFLT);
            } else if (pcat == PARM_CAT_02) {
                conf_edit_cat02(cam, config_parms[indx].parm_name, NULL, PARM_ACT_DFLT);
            } else if (pcat == PARM_CAT_03) {
                conf_edit_cat03(cam, config_parms[indx].parm_name, NULL, PARM_ACT_DFLT);
            } else if (pcat == PARM_CAT_04) {
                conf_edit_cat04(cam, config_parms[indx].parm_name, NULL, PARM_ACT_DFLT);
            } else if (pcat == PARM_CAT_05) {
                conf_edit_cat05(cam, config_parms[indx].parm_name, NULL, PARM_ACT_DFLT);
            }
        }
        indx++;
    }
}

int conf_edit_set_active(struct ctx_motapp *motapp, int threadnbr, char *cmd, char *arg1){
    int indx;
    enum PARM_CAT pcat;

    indx = 0;
    while (config_parms[indx].parm_name != NULL) {
        if (mystreq(cmd, config_parms[indx].parm_name)) {
            pcat = config_parms[indx].parm_cat;
            if ((pcat == PARM_CAT_00) && (threadnbr == -1)) {
                conf_edit_cat00(motapp, cmd, arg1, PARM_ACT_SET);
            } else if ((config_parms[indx].parm_cat != PARM_CAT_00) && (threadnbr != -1)) {
                if (pcat == PARM_CAT_01) {
                    conf_edit_cat01(motapp->cam_list[threadnbr], cmd, arg1, PARM_ACT_SET);
                } else if (pcat == PARM_CAT_02) {
                    conf_edit_cat02(motapp->cam_list[threadnbr], cmd, arg1, PARM_ACT_SET);
                } else if (pcat == PARM_CAT_03) {
                    conf_edit_cat03(motapp->cam_list[threadnbr], cmd, arg1, PARM_ACT_SET);
                } else if (pcat == PARM_CAT_04) {
                    conf_edit_cat04(motapp->cam_list[threadnbr], cmd, arg1, PARM_ACT_SET);
                } else if (pcat == PARM_CAT_05) {
                    conf_edit_cat05(motapp->cam_list[threadnbr], cmd, arg1, PARM_ACT_SET);
                }
            }
            return 0;
        }
        indx++;
    }
    return -1;

}

static void conf_edit_depr_vid(struct ctx_motapp *motapp
        , int threadnbr, char *cmd, char *arg1, char*newname){

    char parm_curr[PATH_MAX], parm_val[PATH_MAX];
    int retcd;

    conf_edit_vid_control_params(motapp->cam_list[threadnbr], parm_curr, PARM_ACT_GET);
    if (strlen(parm_curr) == 0){
        if (mystreq(cmd,"power_line_frequency")) {
            retcd = snprintf(parm_val, PATH_MAX,"%s=%s","\"power line frequency\"",arg1);
        } else {
            retcd = snprintf(parm_val, PATH_MAX,"%s=%s",cmd, arg1);
        }
    } else {
        if (mystreq(cmd,"power_line_frequency")) {
            retcd = snprintf(parm_val, PATH_MAX,"%s,%s=%s",parm_curr, "\"power line frequency\"", arg1);
        } else {
            retcd = snprintf(parm_val, PATH_MAX,"%s,%s=%s",parm_curr, cmd, arg1);
        }
    }

    if ((retcd < 0) || (retcd > PATH_MAX)){
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, "Error setting depreciated parameter");
        return;
    }
    conf_edit_set_active(motapp, threadnbr, newname,(char*)parm_val);

}

static void conf_edit_depr_web(struct ctx_motapp *motapp, int threadnbr, char *arg1, char*newname){

    if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
        conf_edit_set_active(motapp, threadnbr, newname, (char*)"0" );
    } else {
        conf_edit_set_active(motapp, threadnbr, newname, (char*)"1" );
    }
}

static void conf_edit_depr_tdbl(struct ctx_motapp *motapp, int threadnbr, char *arg1, char*newname){

    if (mystrceq(arg1, "1") || mystrceq(arg1, "yes") || mystrceq(arg1, "on")) {
        conf_edit_set_active(motapp, threadnbr, newname, (char*)"2");
    } else {
        conf_edit_set_active(motapp, threadnbr, newname, (char*)"1" );
    }
}


static int conf_edit_set_depr(struct ctx_motapp *motapp, int threadnbr, char *cmd, char *arg1){

    int indx;

    indx = 0;
    while (config_parms_depr[indx].parm_name != NULL) {
        if (mystreq(cmd, config_parms[indx].parm_name)) {
            MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, "%s after version %s"
                , config_parms_depr[indx].info
                , config_parms_depr[indx].last_version);

            if (mystreq(config_parms_depr[indx].parm_name,"brightness") ||
                mystreq(config_parms_depr[indx].parm_name,"contrast") ||
                mystreq(config_parms_depr[indx].parm_name,"saturation") ||
                mystreq(config_parms_depr[indx].parm_name,"hue") ||
                mystreq(config_parms_depr[indx].parm_name,"power_line_frequency")) {
                conf_edit_depr_vid(motapp, threadnbr, cmd, arg1, (char*)config_parms_depr[indx].newname);

            } else if (mystreq(config_parms_depr[indx].parm_name,"webcontrol_html_output")) {
                conf_edit_depr_web(motapp, threadnbr, arg1, (char*)config_parms_depr[indx].newname);

            } else if (mystreq(config_parms_depr[indx].parm_name,"text_double")) {
                conf_edit_depr_tdbl(motapp, threadnbr, arg1, (char*)config_parms_depr[indx].newname);

            } else {
                conf_edit_set_active(motapp, threadnbr, (char*)config_parms_depr[indx].newname, arg1);
            }
            return 0;
        }
        indx++;
    }
    return -1;
}

void conf_edit_set(struct ctx_motapp *motapp, int threadnbr, char *cmd, char *arg1){

    if (conf_edit_set_active(motapp, threadnbr, cmd, arg1) == 0) return;

    if (conf_edit_set_depr(motapp, threadnbr, cmd, arg1) == 0) return;

    MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, _("Unknown config option \"%s\""), cmd);

}

int conf_edit_get(struct ctx_cam *cam, const char *cmd, char *arg1, enum PARM_CAT pcat) {
    int retcd;

    retcd = 0;
    if (pcat == PARM_CAT_00) {         conf_edit_cat00(cam->motapp, cmd, arg1,PARM_ACT_GET);
    } else if (pcat == PARM_CAT_01) {  conf_edit_cat01(cam, cmd, arg1, PARM_ACT_GET);
    } else if (pcat == PARM_CAT_02) {  conf_edit_cat02(cam, cmd, arg1, PARM_ACT_GET);
    } else if (pcat == PARM_CAT_03) {  conf_edit_cat03(cam, cmd, arg1, PARM_ACT_GET);
    } else if (pcat == PARM_CAT_04) {  conf_edit_cat04(cam, cmd, arg1, PARM_ACT_GET);
    } else if (pcat == PARM_CAT_05) {  conf_edit_cat05(cam, cmd, arg1, PARM_ACT_GET);
    } else {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,"%s",_("Program coding error"));
        retcd = -1;
    }
    return retcd;

}

