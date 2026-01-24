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
 * conf_profile.hpp - Configuration Profile Management Interface
 *
 * Header file defining the configuration profile class for storing and
 * managing named camera settings profiles in SQLite, enabling quick
 * switching between preset configurations (day/night/low-sensitivity).
 *
 */

#ifndef _INCLUDE_CONF_PROFILE_HPP_
#define _INCLUDE_CONF_PROFILE_HPP_

#ifdef HAVE_SQLITE3DB
    #include <sqlite3.h>
#endif

#include <map>
#include <vector>
#include <string>
#include <ctime>

/* Forward declarations */
class cls_motapp;
class cls_config;

/* Profile metadata structure */
struct ctx_profile_info {
    int         profile_id;
    int         camera_id;
    std::string name;
    std::string description;
    bool        is_default;
    time_t      created_at;
    time_t      updated_at;
    int         param_count;
};

/* Profile manager class - handles configuration profile storage and retrieval */
class cls_config_profile {
    public:
        cls_config_profile(cls_motapp *p_app);
        ~cls_config_profile();

        /* Profile CRUD operations */
        int create_profile(int camera_id, const std::string &name,
                          const std::string &desc,
                          const std::map<std::string, std::string> &params);
        int load_profile(int profile_id, std::map<std::string, std::string> &params);
        int update_profile(int profile_id, const std::map<std::string, std::string> &params);
        int delete_profile(int profile_id);

        /* Profile queries */
        std::vector<ctx_profile_info> list_profiles(int camera_id);
        bool get_profile_info(int profile_id, ctx_profile_info &info);
        int get_default_profile(int camera_id);
        int set_default_profile(int profile_id);

        /* Snapshot current config - extracts profileable parameters */
        std::map<std::string, std::string> snapshot_config(cls_config *cfg);

        /* Apply profile to config - returns list of params that need restart */
        std::vector<std::string> apply_profile(cls_config *cfg, int profile_id);

        /* Thread safety */
        pthread_mutex_t mutex_profile;

        /* Status */
        bool enabled;  /* True if database initialized successfully */

    private:
        cls_motapp *app;

#ifdef HAVE_SQLITE3DB
        sqlite3 *db;
        std::string db_path;

        /* Database operations */
        int init_database();
        void close_database();
        int exec_sql(const std::string &sql);
        int exec_sql_params(const std::string &sql,
                           const std::vector<std::string> &params);

        /* Parameter filtering */
        bool is_profileable_param(const std::string &param_name);

        /* Libcamera controls mapping */
        std::string get_libcam_param_value(cls_config *cfg, const std::string &param_name);
        void set_libcam_param_value(cls_config *cfg, const std::string &param_name,
                                   const std::string &value);
#endif
};

#endif /* _INCLUDE_CONF_PROFILE_HPP_ */
