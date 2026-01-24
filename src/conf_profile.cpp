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
 * conf_profile.cpp - Configuration Profile Persistence Implementation
 *
 * This module manages named configuration profiles stored in SQLite, allowing
 * users to save and restore camera and motion detection settings. Profiles
 * capture libcamera controls and detection parameters, enabling quick switching
 * between presets (e.g., "daytime", "nighttime", "low-sensitivity").
 *
 */

#include "motion.hpp"
#include "conf_profile.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include <sstream>
#include <iomanip>

cls_config_profile::cls_config_profile(cls_motapp *p_app)
{
    app = p_app;
    enabled = false;

#ifdef HAVE_SQLITE3DB
    db = nullptr;
    pthread_mutex_init(&mutex_profile, nullptr);

    /* Determine database path */
    if (app->cfg->parm_cam.target_dir != "") {
        db_path = app->cfg->parm_cam.target_dir + "/config_profiles.db";
    } else {
        db_path = app->cfg->parm_cam.config_dir + "/config_profiles.db";
    }

    /* Initialize database */
    if (init_database() == 0) {
        enabled = true;
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
            _("Configuration profiles enabled: %s"), db_path.c_str());
    } else {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
            _("Configuration profiles disabled: database initialization failed"));
    }
#else
    MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,
        _("Configuration profiles disabled: SQLite3 support not compiled"));
#endif
}

cls_config_profile::~cls_config_profile()
{
#ifdef HAVE_SQLITE3DB
    close_database();
    pthread_mutex_destroy(&mutex_profile);
#endif
}

#ifdef HAVE_SQLITE3DB

/* Initialize SQLite database and create schema */
int cls_config_profile::init_database()
{
    int retcd;
    char *err_msg = nullptr;

    /* Open database */
    retcd = sqlite3_open(db_path.c_str(), &db);
    if (retcd != SQLITE_OK) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
            _("Failed to open profile database: %s"), sqlite3_errmsg(db));
        return -1;
    }

    /* Enable foreign keys */
    const char *fk_sql = "PRAGMA foreign_keys = ON;";
    retcd = sqlite3_exec(db, fk_sql, nullptr, nullptr, &err_msg);
    if (retcd != SQLITE_OK) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
            _("Failed to enable foreign keys: %s"), err_msg);
        sqlite3_free(err_msg);
        return -1;
    }

    /* Create profiles table */
    const char *create_profiles_sql =
        "CREATE TABLE IF NOT EXISTS config_profiles ("
        "  profile_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  camera_id INTEGER NOT NULL DEFAULT 0,"
        "  profile_name TEXT NOT NULL,"
        "  description TEXT,"
        "  is_default BOOLEAN NOT NULL DEFAULT 0,"
        "  created_at INTEGER NOT NULL,"
        "  updated_at INTEGER NOT NULL,"
        "  UNIQUE(camera_id, profile_name)"
        ");";

    retcd = sqlite3_exec(db, create_profiles_sql, nullptr, nullptr, &err_msg);
    if (retcd != SQLITE_OK) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
            _("Failed to create profiles table: %s"), err_msg);
        sqlite3_free(err_msg);
        return -1;
    }

    /* Create profile_params table */
    const char *create_params_sql =
        "CREATE TABLE IF NOT EXISTS config_profile_params ("
        "  param_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  profile_id INTEGER NOT NULL,"
        "  param_name TEXT NOT NULL,"
        "  param_value TEXT NOT NULL,"
        "  FOREIGN KEY(profile_id) REFERENCES config_profiles(profile_id) ON DELETE CASCADE,"
        "  UNIQUE(profile_id, param_name)"
        ");";

    retcd = sqlite3_exec(db, create_params_sql, nullptr, nullptr, &err_msg);
    if (retcd != SQLITE_OK) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
            _("Failed to create params table: %s"), err_msg);
        sqlite3_free(err_msg);
        return -1;
    }

    /* Create indexes */
    const char *idx_profiles = "CREATE INDEX IF NOT EXISTS idx_profiles_camera ON config_profiles(camera_id);";
    const char *idx_default = "CREATE INDEX IF NOT EXISTS idx_profiles_default ON config_profiles(camera_id, is_default);";
    const char *idx_params = "CREATE INDEX IF NOT EXISTS idx_profile_params_profile ON config_profile_params(profile_id);";

    sqlite3_exec(db, idx_profiles, nullptr, nullptr, nullptr);
    sqlite3_exec(db, idx_default, nullptr, nullptr, nullptr);
    sqlite3_exec(db, idx_params, nullptr, nullptr, nullptr);

    /* Create trigger to enforce single default per camera */
    const char *trigger_default =
        "CREATE TRIGGER IF NOT EXISTS enforce_single_default "
        "BEFORE INSERT ON config_profiles "
        "WHEN NEW.is_default = 1 "
        "BEGIN "
        "  UPDATE config_profiles SET is_default = 0 "
        "  WHERE camera_id = NEW.camera_id AND is_default = 1; "
        "END;";

    retcd = sqlite3_exec(db, trigger_default, nullptr, nullptr, &err_msg);
    if (retcd != SQLITE_OK) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,
            _("Failed to create default trigger: %s"), err_msg);
        sqlite3_free(err_msg);
    }

    const char *trigger_default_update =
        "CREATE TRIGGER IF NOT EXISTS enforce_single_default_update "
        "BEFORE UPDATE ON config_profiles "
        "WHEN NEW.is_default = 1 AND OLD.is_default = 0 "
        "BEGIN "
        "  UPDATE config_profiles SET is_default = 0 "
        "  WHERE camera_id = NEW.camera_id AND is_default = 1 AND profile_id != NEW.profile_id; "
        "END;";

    retcd = sqlite3_exec(db, trigger_default_update, nullptr, nullptr, &err_msg);
    if (retcd != SQLITE_OK) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,
            _("Failed to create default update trigger: %s"), err_msg);
        sqlite3_free(err_msg);
    }

    return 0;
}

void cls_config_profile::close_database()
{
    if (db != nullptr) {
        sqlite3_close(db);
        db = nullptr;
    }
}

/* Execute SQL without parameters */
int cls_config_profile::exec_sql(const std::string &sql)
{
    char *err_msg = nullptr;
    int retcd = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);

    if (retcd != SQLITE_OK) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
            _("SQL execution failed: %s - %s"), err_msg, sql.c_str());
        sqlite3_free(err_msg);
        return -1;
    }

    return 0;
}

/* Check if parameter should be included in profile */
bool cls_config_profile::is_profileable_param(const std::string &param_name)
{
    /* Libcamera controls (14 params) */
    if (param_name == "libcam_brightness" || param_name == "libcam_contrast" ||
        param_name == "libcam_gain" || param_name == "libcam_awb_enable" ||
        param_name == "libcam_awb_mode" || param_name == "libcam_awb_locked" ||
        param_name == "libcam_colour_temp" || param_name == "libcam_colour_gain_r" ||
        param_name == "libcam_colour_gain_b" || param_name == "libcam_af_mode" ||
        param_name == "libcam_lens_position" || param_name == "libcam_af_range" ||
        param_name == "libcam_af_speed" || param_name == "libcam_params") {
        return true;
    }

    /* Motion detection (16 params) */
    if (param_name == "threshold" || param_name == "threshold_maximum" ||
        param_name == "threshold_sdevx" || param_name == "threshold_sdevy" ||
        param_name == "threshold_sdevxy" || param_name == "threshold_ratio" ||
        param_name == "threshold_ratio_change" || param_name == "threshold_tune" ||
        param_name == "noise_level" || param_name == "noise_tune" ||
        param_name == "despeckle_filter" || param_name == "area_detect" ||
        param_name == "lightswitch_percent" || param_name == "lightswitch_frames" ||
        param_name == "minimum_motion_frames" || param_name == "event_gap") {
        return true;
    }

    /* Device settings (1 param) */
    if (param_name == "framerate") {
        return true;
    }

    return false;
}

/* Create new profile */
int cls_config_profile::create_profile(int camera_id, const std::string &name,
                                       const std::string &desc,
                                       const std::map<std::string, std::string> &params)
{
    if (!enabled) {
        return -1;
    }

    pthread_mutex_lock(&mutex_profile);

    /* Start transaction */
    exec_sql("BEGIN TRANSACTION;");

    /* Insert profile metadata */
    time_t now = time(nullptr);
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO config_profiles (camera_id, profile_name, description, "
                     "is_default, created_at, updated_at) VALUES (?, ?, ?, 0, ?, ?);";

    int retcd = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (retcd != SQLITE_OK) {
        exec_sql("ROLLBACK;");
        pthread_mutex_unlock(&mutex_profile);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
            _("Failed to prepare profile insert: %s"), sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_int(stmt, 1, camera_id);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, desc.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, (int64_t)now);
    sqlite3_bind_int64(stmt, 5, (int64_t)now);

    retcd = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (retcd != SQLITE_DONE) {
        exec_sql("ROLLBACK;");
        pthread_mutex_unlock(&mutex_profile);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
            _("Failed to insert profile: %s"), sqlite3_errmsg(db));
        return -1;
    }

    int64_t profile_id = sqlite3_last_insert_rowid(db);

    /* Insert parameters */
    const char *param_sql = "INSERT INTO config_profile_params (profile_id, param_name, param_value) "
                           "VALUES (?, ?, ?);";

    for (const auto &kv : params) {
        if (!is_profileable_param(kv.first)) {
            continue;  /* Skip non-profileable params */
        }

        retcd = sqlite3_prepare_v2(db, param_sql, -1, &stmt, nullptr);
        if (retcd != SQLITE_OK) {
            continue;
        }

        sqlite3_bind_int(stmt, 1, (int)profile_id);
        sqlite3_bind_text(stmt, 2, kv.first.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, kv.second.c_str(), -1, SQLITE_TRANSIENT);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    /* Commit transaction */
    exec_sql("COMMIT;");
    pthread_mutex_unlock(&mutex_profile);

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        _("Created profile '%s' (id=%d) for camera %d"), name.c_str(), (int)profile_id, camera_id);

    return (int)profile_id;
}

/* Load profile parameters */
int cls_config_profile::load_profile(int profile_id, std::map<std::string, std::string> &params)
{
    if (!enabled) {
        return -1;
    }

    pthread_mutex_lock(&mutex_profile);

    params.clear();

    sqlite3_stmt *stmt;
    const char *sql = "SELECT param_name, param_value FROM config_profile_params "
                     "WHERE profile_id = ?;";

    int retcd = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (retcd != SQLITE_OK) {
        pthread_mutex_unlock(&mutex_profile);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, profile_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(stmt, 0);
        const char *value = (const char *)sqlite3_column_text(stmt, 1);
        if (name && value) {
            params[name] = value;
        }
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&mutex_profile);

    return params.empty() ? -1 : 0;
}

/* Update profile parameters */
int cls_config_profile::update_profile(int profile_id,
                                       const std::map<std::string, std::string> &params)
{
    if (!enabled) {
        return -1;
    }

    pthread_mutex_lock(&mutex_profile);

    exec_sql("BEGIN TRANSACTION;");

    /* Delete existing params */
    sqlite3_stmt *stmt;
    const char *del_sql = "DELETE FROM config_profile_params WHERE profile_id = ?;";

    int retcd = sqlite3_prepare_v2(db, del_sql, -1, &stmt, nullptr);
    if (retcd == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, profile_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    /* Insert updated params */
    const char *ins_sql = "INSERT INTO config_profile_params (profile_id, param_name, param_value) "
                         "VALUES (?, ?, ?);";

    for (const auto &kv : params) {
        if (!is_profileable_param(kv.first)) {
            continue;
        }

        retcd = sqlite3_prepare_v2(db, ins_sql, -1, &stmt, nullptr);
        if (retcd != SQLITE_OK) {
            continue;
        }

        sqlite3_bind_int(stmt, 1, profile_id);
        sqlite3_bind_text(stmt, 2, kv.first.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, kv.second.c_str(), -1, SQLITE_TRANSIENT);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    /* Update timestamp */
    time_t now = time(nullptr);
    const char *upd_sql = "UPDATE config_profiles SET updated_at = ? WHERE profile_id = ?;";

    retcd = sqlite3_prepare_v2(db, upd_sql, -1, &stmt, nullptr);
    if (retcd == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, (int64_t)now);
        sqlite3_bind_int(stmt, 2, profile_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    exec_sql("COMMIT;");
    pthread_mutex_unlock(&mutex_profile);

    return 0;
}

/* Delete profile */
int cls_config_profile::delete_profile(int profile_id)
{
    if (!enabled) {
        return -1;
    }

    pthread_mutex_lock(&mutex_profile);

    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM config_profiles WHERE profile_id = ?;";

    int retcd = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (retcd != SQLITE_OK) {
        pthread_mutex_unlock(&mutex_profile);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, profile_id);
    retcd = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    pthread_mutex_unlock(&mutex_profile);

    if (retcd == SQLITE_DONE) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
            _("Deleted profile id=%d"), profile_id);
        return 0;
    }

    return -1;
}

/* List all profiles for a camera */
std::vector<ctx_profile_info> cls_config_profile::list_profiles(int camera_id)
{
    std::vector<ctx_profile_info> profiles;

    if (!enabled) {
        return profiles;
    }

    pthread_mutex_lock(&mutex_profile);

    sqlite3_stmt *stmt;
    const char *sql = "SELECT p.profile_id, p.camera_id, p.profile_name, p.description, "
                     "p.is_default, p.created_at, p.updated_at, "
                     "COUNT(pp.param_id) as param_count "
                     "FROM config_profiles p "
                     "LEFT JOIN config_profile_params pp ON p.profile_id = pp.profile_id "
                     "WHERE p.camera_id = ? "
                     "GROUP BY p.profile_id "
                     "ORDER BY p.is_default DESC, p.profile_name ASC;";

    int retcd = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (retcd != SQLITE_OK) {
        pthread_mutex_unlock(&mutex_profile);
        return profiles;
    }

    sqlite3_bind_int(stmt, 1, camera_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ctx_profile_info info;
        info.profile_id = sqlite3_column_int(stmt, 0);
        info.camera_id = sqlite3_column_int(stmt, 1);
        info.name = (const char *)sqlite3_column_text(stmt, 2);
        const char *desc = (const char *)sqlite3_column_text(stmt, 3);
        info.description = desc ? desc : "";
        info.is_default = sqlite3_column_int(stmt, 4) != 0;
        info.created_at = (time_t)sqlite3_column_int64(stmt, 5);
        info.updated_at = (time_t)sqlite3_column_int64(stmt, 6);
        info.param_count = sqlite3_column_int(stmt, 7);

        profiles.push_back(info);
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&mutex_profile);

    return profiles;
}

/* Get profile info by ID */
bool cls_config_profile::get_profile_info(int profile_id, ctx_profile_info &info)
{
    if (!enabled) {
        return false;
    }

    pthread_mutex_lock(&mutex_profile);

    sqlite3_stmt *stmt;
    const char *sql = "SELECT p.profile_id, p.camera_id, p.profile_name, p.description, "
                     "p.is_default, p.created_at, p.updated_at, "
                     "COUNT(pp.param_id) as param_count "
                     "FROM config_profiles p "
                     "LEFT JOIN config_profile_params pp ON p.profile_id = pp.profile_id "
                     "WHERE p.profile_id = ? "
                     "GROUP BY p.profile_id;";

    int retcd = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (retcd != SQLITE_OK) {
        pthread_mutex_unlock(&mutex_profile);
        return false;
    }

    sqlite3_bind_int(stmt, 1, profile_id);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        info.profile_id = sqlite3_column_int(stmt, 0);
        info.camera_id = sqlite3_column_int(stmt, 1);
        info.name = (const char *)sqlite3_column_text(stmt, 2);
        const char *desc = (const char *)sqlite3_column_text(stmt, 3);
        info.description = desc ? desc : "";
        info.is_default = sqlite3_column_int(stmt, 4) != 0;
        info.created_at = (time_t)sqlite3_column_int64(stmt, 5);
        info.updated_at = (time_t)sqlite3_column_int64(stmt, 6);
        info.param_count = sqlite3_column_int(stmt, 7);
        found = true;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&mutex_profile);

    return found;
}

/* Get default profile ID for camera */
int cls_config_profile::get_default_profile(int camera_id)
{
    if (!enabled) {
        return -1;
    }

    pthread_mutex_lock(&mutex_profile);

    sqlite3_stmt *stmt;
    const char *sql = "SELECT profile_id FROM config_profiles "
                     "WHERE camera_id = ? AND is_default = 1;";

    int retcd = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (retcd != SQLITE_OK) {
        pthread_mutex_unlock(&mutex_profile);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, camera_id);

    int profile_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        profile_id = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&mutex_profile);

    return profile_id;
}

/* Set default profile */
int cls_config_profile::set_default_profile(int profile_id)
{
    if (!enabled) {
        return -1;
    }

    pthread_mutex_lock(&mutex_profile);

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE config_profiles SET is_default = 1 WHERE profile_id = ?;";

    int retcd = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (retcd != SQLITE_OK) {
        pthread_mutex_unlock(&mutex_profile);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, profile_id);
    retcd = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    pthread_mutex_unlock(&mutex_profile);

    return (retcd == SQLITE_DONE) ? 0 : -1;
}

/* Snapshot current configuration */
std::map<std::string, std::string> cls_config_profile::snapshot_config(cls_config *cfg)
{
    std::map<std::string, std::string> params;

    if (!cfg) {
        return params;
    }

    /* Helper to convert value to string */
    auto to_str = [](auto val) {
        std::ostringstream oss;
        oss << val;
        return oss.str();
    };

    /* Libcamera controls - stored as virtual params in config */
    params["libcam_brightness"] = to_str(cfg->parm_cam.libcam_brightness);
    params["libcam_contrast"] = to_str(cfg->parm_cam.libcam_contrast);
    params["libcam_gain"] = to_str(cfg->parm_cam.libcam_gain);
    params["libcam_awb_enable"] = to_str(cfg->parm_cam.libcam_awb_enable);
    params["libcam_awb_mode"] = to_str(cfg->parm_cam.libcam_awb_mode);
    params["libcam_awb_locked"] = to_str(cfg->parm_cam.libcam_awb_locked);
    params["libcam_colour_temp"] = to_str(cfg->parm_cam.libcam_colour_temp);
    params["libcam_colour_gain_r"] = to_str(cfg->parm_cam.libcam_colour_gain_r);
    params["libcam_colour_gain_b"] = to_str(cfg->parm_cam.libcam_colour_gain_b);
    params["libcam_af_mode"] = to_str(cfg->parm_cam.libcam_af_mode);
    params["libcam_lens_position"] = to_str(cfg->parm_cam.libcam_lens_position);
    params["libcam_af_range"] = to_str(cfg->parm_cam.libcam_af_range);
    params["libcam_af_speed"] = to_str(cfg->parm_cam.libcam_af_speed);
    params["libcam_params"] = cfg->parm_cam.libcam_params;

    /* Motion detection settings */
    params["threshold"] = to_str(cfg->parm_cam.threshold);
    params["threshold_maximum"] = to_str(cfg->parm_cam.threshold_maximum);
    params["threshold_sdevx"] = to_str(cfg->parm_cam.threshold_sdevx);
    params["threshold_sdevy"] = to_str(cfg->parm_cam.threshold_sdevy);
    params["threshold_sdevxy"] = to_str(cfg->parm_cam.threshold_sdevxy);
    params["threshold_ratio"] = to_str(cfg->parm_cam.threshold_ratio);
    params["threshold_ratio_change"] = to_str(cfg->parm_cam.threshold_ratio_change);
    params["threshold_tune"] = to_str(cfg->parm_cam.threshold_tune);
    params["noise_level"] = to_str(cfg->parm_cam.noise_level);
    params["noise_tune"] = to_str(cfg->parm_cam.noise_tune);
    params["despeckle_filter"] = cfg->parm_cam.despeckle_filter;
    params["area_detect"] = cfg->parm_cam.area_detect;
    params["lightswitch_percent"] = to_str(cfg->parm_cam.lightswitch_percent);
    params["lightswitch_frames"] = to_str(cfg->parm_cam.lightswitch_frames);
    params["minimum_motion_frames"] = to_str(cfg->parm_cam.minimum_motion_frames);
    params["event_gap"] = to_str(cfg->parm_cam.event_gap);

    /* Device settings */
    params["framerate"] = to_str(cfg->parm_cam.framerate);

    return params;
}

/* Apply profile to configuration */
std::vector<std::string> cls_config_profile::apply_profile(cls_config *cfg, int profile_id)
{
    std::vector<std::string> needs_restart;

    if (!cfg || !enabled) {
        return needs_restart;
    }

    /* Load profile parameters */
    std::map<std::string, std::string> params;
    if (load_profile(profile_id, params) != 0) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,
            _("Failed to load profile %d"), profile_id);
        return needs_restart;
    }

    /* Apply each parameter using existing config edit mechanism */
    for (const auto &kv : params) {
        std::string parm_val;

        /* Use config's edit_set to apply the value */
        /* This will handle validation and hot-reload flags automatically */
        cfg->edit_set(kv.first, kv.second);

        /* Check if this param requires restart */
        if (kv.first == "framerate") {
            needs_restart.push_back(kv.first);
        }
    }

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        _("Applied profile %d (%zu parameters)"),
        profile_id, params.size());

    if (!needs_restart.empty()) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,
            _("Profile contains parameters requiring camera restart"));
    }

    return needs_restart;
}

#else

/* Stub implementations when SQLite3 not available */
int cls_config_profile::create_profile(int, const std::string &,
                                       const std::string &,
                                       const std::map<std::string, std::string> &)
{
    return -1;
}

int cls_config_profile::load_profile(int, std::map<std::string, std::string> &)
{
    return -1;
}

int cls_config_profile::update_profile(int, const std::map<std::string, std::string> &)
{
    return -1;
}

int cls_config_profile::delete_profile(int)
{
    return -1;
}

std::vector<ctx_profile_info> cls_config_profile::list_profiles(int)
{
    return std::vector<ctx_profile_info>();
}

bool cls_config_profile::get_profile_info(int, ctx_profile_info &)
{
    return false;
}

int cls_config_profile::get_default_profile(int)
{
    return -1;
}

int cls_config_profile::set_default_profile(int)
{
    return -1;
}

std::map<std::string, std::string> cls_config_profile::snapshot_config(cls_config *)
{
    return std::map<std::string, std::string>();
}

std::vector<std::string> cls_config_profile::apply_profile(cls_config *, int)
{
    return std::vector<std::string>();
}

#endif
