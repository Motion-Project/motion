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
 * conf_file.hpp - Configuration File I/O Separation
 *
 * This module handles all configuration file operations, separating file I/O
 * concerns from the core cls_config parameter management class.
 *
 */

#ifndef _INCLUDE_CONF_FILE_HPP_
#define _INCLUDE_CONF_FILE_HPP_

#include <string>
#include <cstdio>

/* Forward declarations */
class cls_motapp;
class cls_config;
enum PARM_CAT;

/*
 * Configuration File I/O Handler
 *
 * Responsibilities:
 * - Loading configuration files (init, process)
 * - Command line argument parsing (cmdline)
 * - Saving configuration files (parms_write)
 * - Logging configuration state (parms_log)
 * - Deprecated parameter handling
 *
 * This class works with cls_config instances to perform I/O operations
 * while cls_config focuses on parameter storage and editing.
 */
class cls_config_file {
public:
    cls_config_file(cls_motapp *p_app, cls_config *p_config);

    /* Load configuration */
    void init();
    void process();
    void cmdline();

    /* Save configuration */
    void parms_write();

    /* Logging */
    void parms_log();

private:
    cls_motapp *app;
    cls_config *config;

    /* File processing helpers */
    void process_line(const std::string &line);
    void log_parm(const std::string &parm_nm, const std::string &parm_vl);

    /* Write helpers */
    void write_app();
    void write_cam();
    void write_snd();
    void write_parms(FILE *conffile, const std::string &parm_nm,
                     const std::string &parm_vl, enum PARM_CAT parm_ct, bool reset);
};

#endif /* _INCLUDE_CONF_FILE_HPP_ */
