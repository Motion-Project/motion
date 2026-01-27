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

/*
 * conf_file.cpp - Configuration File I/O Implementation
 *
 * This module handles all configuration file operations, separating file I/O
 * concerns from the core cls_config parameter management class.
 *
 */

#include "motion.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "camera.hpp"
#include "sound.hpp"
#include "conf.hpp"
#include "conf_file.hpp"

extern struct ctx_parm config_parms[];

/*
 * Constructor - takes pointers to app and config instances
 */
cls_config_file::cls_config_file(cls_motapp *p_app, cls_config *p_config)
    : app(p_app), config(p_config)
{
}

/*
 * Initialize configuration from command line and config files
 *
 * Search order for motion.conf:
 * 1. Command line -c option
 * 2. Current working directory
 * 3. ~/.motion/motion.conf
 * 4. $configdir/motion.conf (build-time default)
 * 5. $sysconfdir/motion.conf (deprecated location)
 */
void cls_config_file::init()
{
    std::string filename;
    char path[PATH_MAX];
    struct stat statbuf;

    /* Process command line arguments first */
    cmdline();

    /* Check if config file was specified on command line */
    filename = "";
    if (app->conf_src->conf_filename != "") {
        filename = app->conf_src->conf_filename;
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename = "";
        }
    }

    /* Try current working directory */
    if (filename == "") {
        if (getcwd(path, sizeof(path)) == NULL) {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Error getcwd"));
            exit(-1);
        }
        filename = path + std::string("/motion.conf");
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename = "";
        }
    }

    /* Try home directory */
    if (filename == "") {
        filename = std::string(getenv("HOME")) + std::string("/.motion/motion.conf");
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename = "";
        }
    }

    /* Try build-time configdir */
    if (filename == "") {
        filename = std::string(configdir) + std::string("/motion.conf");
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename = "";
        }
    }

    /* Try deprecated sysconfdir */
    if (filename == "") {
        filename = std::string(sysconfdir) + std::string("/motion.conf");
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename = "";
        }
        if (filename != "") {
            MOTION_LOG(WRN, TYPE_ALL, SHOW_ERRNO,
                _("The configuration file location '%s' is deprecated."),
                sysconfdir);
            MOTION_LOG(WRN, TYPE_ALL, SHOW_ERRNO,
                _("The new default configuration file location is '%s'"),
                configdir);
        }
    }

    if (filename == "") {
        MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO,
            _("Could not open configuration file"));
        exit(-1);
    }

    config->edit_set("conf_filename", filename);

    /* Process the main config file */
    app->conf_src->conf_filename = filename;
    app->conf_src->from_conf_dir = false;

    /* Create a temporary file handler for the main config */
    cls_config_file main_file(app, app->conf_src);
    main_file.process();

    /* If no cameras or sounds defined, add a default camera */
    if ((app->cam_cnt == 0) && (app->snd_cnt == 0)) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
            _("No camera or sound configuration files specified."));
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
            _("Adding a camera configuration file."));
        app->conf_src->camera_add("", false);
    }

    /* Re-process command line to override config file settings */
    cmdline();

    /* Assign thread numbers */
    for (int indx = 0; indx < app->cam_cnt; indx++) {
        app->cam_list[indx]->threadnr = indx;
    }

    for (int indx = 0; indx < app->snd_cnt; indx++) {
        app->snd_list[indx]->threadnr = (indx + app->cam_cnt);
    }
}

/*
 * Process configuration file - parse lines and set parameters
 */
void cls_config_file::process()
{
    size_t stpos;
    std::string line, parm_nm, parm_vl;
    std::ifstream ifs;

    ifs.open(config->conf_filename);
    if (ifs.is_open() == false) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
            _("params_file not found: %s"),
            config->conf_filename.c_str());
        return;
    }

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        _("Processing config file %s"),
        config->conf_filename.c_str());

    while (std::getline(ifs, line)) {
        mytrim(line);
        stpos = line.find(" ");
        if (line.find('\t') != std::string::npos) {
            if (line.find('\t') < stpos) {
                stpos = line.find('\t');
            }
        }
        if (stpos > line.find("=")) {
            stpos = line.find("=");
        }
        if ((stpos != line.length() - 1) &&
            (stpos != 0) &&
            (line.substr(0, 1) != ";") &&
            (line.substr(0, 1) != "#")) {
            parm_nm = line.substr(0, stpos);
            if (stpos != std::string::npos) {
                parm_vl = line.substr(stpos + 1, line.length() - stpos);
            } else {
                parm_vl = "";
            }
            myunquote(parm_nm);
            myunquote(parm_vl);
            if ((parm_nm == "camera") && (app->conf_src == config)) {
                config->camera_add(parm_vl, false);
            } else if ((parm_nm == "sound") && (app->conf_src == config)) {
                config->sound_add(parm_vl, false);
            } else if ((parm_nm == "config_dir") && (app->conf_src == config)) {
                config->edit_set("config_dir", parm_vl);
                /* Process config_dir as camera/sound configs */
                DIR *dp;
                dirent *ep;
                std::string file;
                dp = opendir(parm_vl.c_str());
                if (dp != NULL) {
                    while ((ep = readdir(dp))) {
                        file.assign(ep->d_name);
                        if (file.length() >= 5) {
                            if (file.substr(file.length() - 5, 5) == ".conf") {
                                if (file.find("sound") == std::string::npos) {
                                    file = parm_vl + "/" + file;
                                    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
                                        _("Processing as camera config file %s"),
                                        file.c_str());
                                    config->camera_add(file, true);
                                } else {
                                    file = parm_vl + "/" + file;
                                    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
                                        _("Processing as sound config file %s"),
                                        file.c_str());
                                    config->sound_add(file, true);
                                }
                            }
                        }
                    }
                }
                closedir(dp);
            } else if ((parm_nm != "camera") && (parm_nm != "sound") &&
                       (parm_nm != "config_dir")) {
                config->edit_set(parm_nm, parm_vl);
            }
        } else if ((line != "") &&
                   (line.substr(0, 1) != ";") &&
                   (line.substr(0, 1) != "#")) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
                _("Unable to parse line: %s"), line.c_str());
        }
    }
    ifs.close();
}

/*
 * Parse command line arguments
 */
void cls_config_file::cmdline()
{
    int c;

    while ((c = getopt(app->argc, app->argv, "bc:d:hmn?p:k:l:")) != EOF)
        switch (c) {
        case 'c':
            config->edit_set("conf_filename", optarg);
            break;
        case 'b':
            config->edit_set("daemon", "on");
            break;
        case 'n':
            config->edit_set("daemon", "off");
            break;
        case 'd':
            config->edit_set("log_level", optarg);
            break;
        case 'k':
            config->edit_set("log_type", optarg);
            break;
        case 'p':
            config->edit_set("pid_file", optarg);
            break;
        case 'l':
            config->edit_set("log_file", optarg);
            break;
        case 'm':
            app->user_pause = "on";
            break;
        case 'h':
        case '?':
        default:
            config->usage();
            exit(1);
        }

    optind = 1;
}

/*
 * Log a single parameter with sensitive data redaction
 */
void cls_config_file::log_parm(const std::string &parm_nm, const std::string &parm_vl)
{
    if ((parm_nm == "netcam_url") ||
        (parm_nm == "netcam_userpass") ||
        (parm_nm == "netcam_high_url") ||
        (parm_nm == "webcontrol_authentication") ||
        (parm_nm == "webcontrol_user_authentication") ||
        (parm_nm == "webcontrol_key") ||
        (parm_nm == "webcontrol_cert") ||
        (parm_nm == "database_user") ||
        (parm_nm == "database_password")) {
        MOTION_SHT(INF, TYPE_ALL, NO_ERRNO,
            _("%-25s <redacted>"), parm_nm.c_str());
    } else {
        if ((parm_nm.compare(0, 4, "text") == 0) ||
            (parm_vl.compare(0, 1, " ") != 0)) {
            MOTION_SHT(INF, TYPE_ALL, NO_ERRNO,
                "%-25s %s", parm_nm.c_str(), parm_vl.c_str());
        } else {
            MOTION_SHT(INF, TYPE_ALL, NO_ERRNO,
                "%-25s \"%s\"", parm_nm.c_str(), parm_vl.c_str());
        }
    }
}

/*
 * Log all configuration parameters from all config files
 */
void cls_config_file::parms_log()
{
    int i, indx;
    std::string parm_vl, parm_main, parm_nm;
    std::list<std::string> parm_array;
    std::list<std::string>::iterator it;
    enum PARM_CAT parm_ct;
    enum PARM_TYP parm_typ;

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
        _("Logging configuration parameters from all files"));

    MOTION_SHT(INF, TYPE_ALL, NO_ERRNO,
        _("Config file: %s"), app->conf_src->conf_filename.c_str());

    i = 0;
    while (config_parms[i].parm_name != "") {
        parm_nm = config_parms[i].parm_name;
        parm_ct = config_parms[i].parm_cat;
        parm_typ = config_parms[i].parm_type;

        if ((parm_nm != "camera") && (parm_nm != "sound") &&
            (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
            (parm_typ != PARM_TYP_ARRAY)) {
            app->conf_src->edit_get(parm_nm, parm_vl, parm_ct);
            log_parm(parm_nm, parm_vl);
        }
        if (parm_typ == PARM_TYP_ARRAY) {
            app->conf_src->edit_get(parm_nm, parm_array, parm_ct);
            for (it = parm_array.begin(); it != parm_array.end(); it++) {
                log_parm(parm_nm, it->c_str());
            }
        }
        i++;
    }

    for (indx = 0; indx < app->cam_cnt; indx++) {
        MOTION_SHT(INF, TYPE_ALL, NO_ERRNO,
            _("Camera config file: %s"),
            app->cam_list[indx]->conf_src->conf_filename.c_str());
        i = 0;
        while (config_parms[i].parm_name != "") {
            parm_nm = config_parms[i].parm_name;
            parm_ct = config_parms[i].parm_cat;
            parm_typ = config_parms[i].parm_type;
            app->conf_src->edit_get(parm_nm, parm_main, parm_ct);

            app->cam_list[indx]->conf_src->edit_get(parm_nm, parm_vl, parm_ct);
            if ((parm_nm != "camera") && (parm_nm != "sound") &&
                (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
                (parm_main != parm_vl) && (parm_typ != PARM_TYP_ARRAY)) {
                log_parm(parm_nm, parm_vl);
            }
            if (parm_typ == PARM_TYP_ARRAY) {
                app->cam_list[indx]->conf_src->edit_get(parm_nm, parm_array, parm_ct);
                for (it = parm_array.begin(); it != parm_array.end(); it++) {
                    log_parm(parm_nm, it->c_str());
                }
            }
            i++;
        }
    }

    for (indx = 0; indx < app->snd_cnt; indx++) {
        MOTION_SHT(INF, TYPE_ALL, NO_ERRNO,
            _("Sound config file: %s"),
            app->snd_list[indx]->conf_src->conf_filename.c_str());
        i = 0;
        while (config_parms[i].parm_name != "") {
            parm_nm = config_parms[i].parm_name;
            parm_ct = config_parms[i].parm_cat;
            parm_typ = config_parms[i].parm_type;
            app->conf_src->edit_get(parm_nm, parm_main, parm_ct);
            app->snd_list[indx]->conf_src->edit_get(parm_nm, parm_vl, parm_ct);
            if ((parm_nm != "camera") && (parm_nm != "sound") &&
                (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
                (parm_main != parm_vl) && (parm_typ != PARM_TYP_ARRAY)) {
                log_parm(parm_nm, parm_vl);
            }
            if (parm_typ == PARM_TYP_ARRAY) {
                app->snd_list[indx]->conf_src->edit_get(parm_nm, parm_array, parm_ct);
                for (it = parm_array.begin(); it != parm_array.end(); it++) {
                    log_parm(parm_nm, it->c_str());
                }
            }
            i++;
        }
    }
}

/*
 * Write a single parameter to config file with category headers
 */
void cls_config_file::write_parms(FILE *conffile, const std::string &parm_nm,
                                   const std::string &parm_vl, enum PARM_CAT parm_ct, bool reset)
{
    static enum PARM_CAT prev_ct;

    if (reset) {
        prev_ct = PARM_CAT_00;
        return;
    }

    if (parm_ct != prev_ct) {
        fprintf(conffile, "\n%s", ";*************************************************\n");
        fprintf(conffile, "%s%s\n", ";*****   ", config->cat_desc(parm_ct, false).c_str());
        fprintf(conffile, "%s", ";*************************************************\n");
        prev_ct = parm_ct;
    }

    if (parm_vl.compare(0, 1, " ") == 0) {
        fprintf(conffile, "%s \"%s\"\n", parm_nm.c_str(), parm_vl.c_str());
    } else {
        fprintf(conffile, "%s %s\n", parm_nm.c_str(), parm_vl.c_str());
    }
}

/*
 * Write application-level configuration file
 */
void cls_config_file::write_app()
{
    int i, indx;
    std::string parm_vl, parm_nm;
    std::list<std::string> parm_array;
    std::list<std::string>::iterator it;
    enum PARM_CAT parm_ct;
    enum PARM_TYP parm_typ;
    char timestamp[32];
    FILE *conffile;

    time_t now = time(0);
    strftime(timestamp, 32, "%Y-%m-%dT%H:%M:%S", localtime(&now));

    conffile = myfopen(app->conf_src->conf_filename.c_str(), "we");
    if (conffile == NULL) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
            _("Failed to write configuration to %s"),
            app->conf_src->conf_filename.c_str());
        return;
    }

    fprintf(conffile, "; %s\n", app->conf_src->conf_filename.c_str());
    fprintf(conffile, ";\n; This config file was generated by Motion " VERSION "\n");
    fprintf(conffile, "; at %s\n", timestamp);
    fprintf(conffile, "\n\n");

    write_parms(conffile, "", "", PARM_CAT_00, true);

    i = 0;
    while (config_parms[i].parm_name != "") {
        parm_nm = config_parms[i].parm_name;
        parm_ct = config_parms[i].parm_cat;
        parm_typ = config_parms[i].parm_type;
        if ((parm_nm != "camera") && (parm_nm != "sound") &&
            (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
            (parm_typ != PARM_TYP_ARRAY)) {
            app->conf_src->edit_get(parm_nm, parm_vl, parm_ct);
            write_parms(conffile, parm_nm, parm_vl, parm_ct, false);
        }
        if (parm_typ == PARM_TYP_ARRAY) {
            app->conf_src->edit_get(parm_nm, parm_array, parm_ct);
            for (it = parm_array.begin(); it != parm_array.end(); it++) {
                write_parms(conffile, parm_nm, it->c_str(), parm_ct, false);
            }
        }
        i++;
    }

    for (indx = 0; indx < app->cam_cnt; indx++) {
        if (app->cam_list[indx]->conf_src->from_conf_dir == false) {
            write_parms(conffile, "camera",
                app->cam_list[indx]->conf_src->conf_filename,
                PARM_CAT_01, false);
        }
    }

    for (indx = 0; indx < app->snd_cnt; indx++) {
        if (app->snd_list[indx]->conf_src->from_conf_dir == false) {
            write_parms(conffile, "sound",
                app->snd_list[indx]->conf_src->conf_filename,
                PARM_CAT_01, false);
        }
    }

    fprintf(conffile, "\n");

    app->conf_src->edit_get("config_dir", parm_vl, PARM_CAT_01);
    write_parms(conffile, "config_dir", parm_vl, PARM_CAT_01, false);

    fprintf(conffile, "\n");
    myfclose(conffile);

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        _("Configuration written to %s"),
        app->conf_src->conf_filename.c_str());
}

/*
 * Write camera configuration files
 */
void cls_config_file::write_cam()
{
    int i, indx;
    std::string parm_vl, parm_main, parm_nm;
    std::list<std::string> parm_array;
    std::list<std::string>::iterator it;
    enum PARM_CAT parm_ct;
    enum PARM_TYP parm_typ;
    char timestamp[32];
    FILE *conffile;

    time_t now = time(0);
    strftime(timestamp, 32, "%Y-%m-%dT%H:%M:%S", localtime(&now));

    for (indx = 0; indx < app->cam_cnt; indx++) {
        conffile = myfopen(app->cam_list[indx]->conf_src->conf_filename.c_str(), "we");
        if (conffile == NULL) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
                _("Failed to write configuration to %s"),
                app->cam_list[indx]->conf_src->conf_filename.c_str());
            return;
        }
        fprintf(conffile, "; %s\n", app->cam_list[indx]->conf_src->conf_filename.c_str());
        fprintf(conffile, ";\n; This config file was generated by Motion " VERSION "\n");
        fprintf(conffile, "; at %s\n", timestamp);
        fprintf(conffile, "\n\n");
        write_parms(conffile, "", "", PARM_CAT_00, true);

        i = 0;
        while (config_parms[i].parm_name != "") {
            parm_nm = config_parms[i].parm_name;
            parm_ct = config_parms[i].parm_cat;
            parm_typ = config_parms[i].parm_type;
            if ((parm_nm != "camera") && (parm_nm != "sound") &&
                (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
                (parm_typ != PARM_TYP_ARRAY)) {
                app->conf_src->edit_get(parm_nm, parm_main, parm_ct);
                app->cam_list[indx]->conf_src->edit_get(parm_nm, parm_vl, parm_ct);
                if (parm_main != parm_vl) {
                    write_parms(conffile, parm_nm, parm_vl, parm_ct, false);
                }
            }
            if (parm_typ == PARM_TYP_ARRAY) {
                app->conf_src->edit_get(parm_nm, parm_array, parm_ct);
                for (it = parm_array.begin(); it != parm_array.end(); it++) {
                    write_parms(conffile, parm_nm, it->c_str(), parm_ct, false);
                }
            }
            i++;
        }
        fprintf(conffile, "\n");
        myfclose(conffile);

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
            _("Configuration written to %s"),
            app->cam_list[indx]->conf_src->conf_filename.c_str());
    }
}

/*
 * Write sound configuration files
 */
void cls_config_file::write_snd()
{
    int i, indx;
    std::string parm_vl, parm_main, parm_nm;
    std::list<std::string> parm_array;
    std::list<std::string>::iterator it;
    enum PARM_CAT parm_ct;
    enum PARM_TYP parm_typ;
    char timestamp[32];
    FILE *conffile;

    time_t now = time(0);
    strftime(timestamp, 32, "%Y-%m-%dT%H:%M:%S", localtime(&now));

    for (indx = 0; indx < app->snd_cnt; indx++) {
        conffile = myfopen(app->snd_list[indx]->conf_src->conf_filename.c_str(), "we");
        if (conffile == NULL) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
                _("Failed to write configuration to %s"),
                app->snd_list[indx]->conf_src->conf_filename.c_str());
            return;
        }
        fprintf(conffile, "; %s\n", app->snd_list[indx]->conf_src->conf_filename.c_str());
        fprintf(conffile, ";\n; This config file was generated by Motion " VERSION "\n");
        fprintf(conffile, "; at %s\n", timestamp);
        fprintf(conffile, "\n\n");
        write_parms(conffile, "", "", PARM_CAT_00, true);

        i = 0;
        while (config_parms[i].parm_name != "") {
            parm_nm = config_parms[i].parm_name;
            parm_ct = config_parms[i].parm_cat;
            parm_typ = config_parms[i].parm_type;
            if ((parm_nm != "camera") && (parm_nm != "sound") &&
                (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
                (parm_typ != PARM_TYP_ARRAY)) {
                app->conf_src->edit_get(parm_nm, parm_main, parm_ct);
                app->snd_list[indx]->conf_src->edit_get(parm_nm, parm_vl, parm_ct);
                if (parm_main != parm_vl) {
                    write_parms(conffile, parm_nm, parm_vl, parm_ct, false);
                }
            }
            if (parm_typ == PARM_TYP_ARRAY) {
                app->conf_src->edit_get(parm_nm, parm_array, parm_ct);
                for (it = parm_array.begin(); it != parm_array.end(); ++it) {
                    write_parms(conffile, parm_nm, it->c_str(), parm_ct, false);
                }
            }
            i++;
        }
        fprintf(conffile, "\n");
        myfclose(conffile);

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
            _("Configuration written to %s"),
            app->snd_list[indx]->conf_src->conf_filename.c_str());
    }
}

/*
 * Write all configuration files
 */
void cls_config_file::parms_write()
{
    write_app();
    write_cam();
    write_snd();
}
