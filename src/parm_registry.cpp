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
 * parm_registry.cpp - Parameter Registry Implementation
 *
 * Provides O(1) parameter lookup via hash map, replacing the O(n) linear
 * array iteration in the original config_parms[] design.
 *
 * Initialization:
 * - Reads from existing config_parms[] array at startup
 * - Assigns scope (APP/CAM/SND) based on category
 * - Builds hash map and category indices
 *
 * See doc/plans/ConfigParam-Refactor-20251211-1730.md for design details.
 */

#include "motion.hpp"
#include "parm_registry.hpp"
#include "conf.hpp"

/* Static empty vector for out-of-range returns */
const std::vector<const ctx_parm_ext*> ctx_parm_registry::empty_vec;

/*
 * Determine parameter scope based on category
 *
 * - CAT_00 (system): APP only
 * - CAT_01-17: Camera-related
 * - CAT_13 (webcontrol): APP only
 * - CAT_15-16 (database/sql): APP only
 * - CAT_18 (sound): SND only
 *
 * Note: Some parameters in camera categories are also used by the app
 * (like device_name, config_dir), but they are primarily camera parameters.
 */
static int get_scope_for_category(enum PARM_CAT cat)
{
    switch (cat) {
    case PARM_CAT_00:   /* system: daemon, logging, etc */
        return PARM_SCOPE_APP;

    case PARM_CAT_13:   /* webcontrol */
    case PARM_CAT_15:   /* database */
    case PARM_CAT_16:   /* sql */
        return PARM_SCOPE_APP;

    case PARM_CAT_18:   /* sound */
        return PARM_SCOPE_SND;

    case PARM_CAT_01:   /* camera setup */
    case PARM_CAT_02:   /* source (v4l2, netcam, libcam) */
    case PARM_CAT_03:   /* image */
    case PARM_CAT_04:   /* overlay */
    case PARM_CAT_05:   /* method (detection) */
    case PARM_CAT_06:   /* masks */
    case PARM_CAT_07:   /* detect */
    case PARM_CAT_08:   /* scripts */
    case PARM_CAT_09:   /* picture */
    case PARM_CAT_10:   /* movies */
    case PARM_CAT_11:   /* timelapse */
    case PARM_CAT_12:   /* pipes */
    case PARM_CAT_14:   /* streams */
    case PARM_CAT_17:   /* tracking */
        return PARM_SCOPE_CAM;

    default:
        return PARM_SCOPE_ALL;
    }
}

/*
 * Singleton instance accessor
 * C++11 guarantees thread-safe initialization of function-local statics
 */
ctx_parm_registry& ctx_parm_registry::instance()
{
    static ctx_parm_registry registry;
    return registry;
}

/*
 * Private constructor - builds registry from config_parms[]
 */
ctx_parm_registry::ctx_parm_registry()
{
    /* Pre-allocate category vectors */
    by_cat.resize(PARM_CAT_MAX);

    /* Iterate through config_parms[] array from conf.cpp */
    int indx = 0;
    while (config_parms[indx].parm_name != "") {
        const ctx_parm &src = config_parms[indx];

        /* Create extended parameter entry */
        ctx_parm_ext ext;
        ext.parm_name = src.parm_name;
        ext.parm_type = src.parm_type;
        ext.parm_cat = src.parm_cat;
        ext.webui_level = src.webui_level;
        ext.scope = get_scope_for_category(src.parm_cat);

        /* Store in master vector */
        size_t vec_idx = parm_vec.size();
        parm_vec.push_back(ext);

        /* Add to hash map for O(1) lookup */
        parm_map[ext.parm_name] = vec_idx;

        indx++;
    }

    /* Build category index (pointers into parm_vec) */
    for (size_t i = 0; i < parm_vec.size(); i++) {
        int cat_idx = static_cast<int>(parm_vec[i].parm_cat);
        if (cat_idx >= 0 && cat_idx < PARM_CAT_MAX) {
            by_cat[cat_idx].push_back(&parm_vec[i]);
        }
    }
}

/*
 * O(1) lookup by parameter name
 */
const ctx_parm_ext *ctx_parm_registry::find(const std::string &name) const
{
    auto it = parm_map.find(name);
    if (it != parm_map.end()) {
        return &parm_vec[it->second];
    }
    return nullptr;
}

/*
 * Get parameters by category
 */
const std::vector<const ctx_parm_ext*>& ctx_parm_registry::by_category(enum PARM_CAT cat) const
{
    int cat_idx = static_cast<int>(cat);
    if (cat_idx >= 0 && cat_idx < static_cast<int>(by_cat.size())) {
        return by_cat[cat_idx];
    }
    return empty_vec;
}

/*
 * Get parameters by scope
 * Returns parameters where (parm.scope & requested_scope) != 0
 */
std::vector<const ctx_parm_ext*> ctx_parm_registry::by_scope(int scope) const
{
    std::vector<const ctx_parm_ext*> result;
    result.reserve(parm_vec.size()); /* Avoid reallocs */

    for (const auto &parm : parm_vec) {
        if (parm.scope & scope) {
            result.push_back(&parm);
        }
    }

    return result;
}

/*
 * Check if a parameter can be hot-reloaded without restart
 * Uses the hot_reload flag from config_parms[] array
 */
bool is_hot_reloadable(const std::string &parm_name)
{
    int indx = 0;
    while (config_parms[indx].parm_name != "") {
        if (config_parms[indx].parm_name == parm_name) {
            return config_parms[indx].hot_reload;
        }
        indx++;
    }
    return false;  /* Parameter not found */
}

/*
 * Get parameter info from the config_parms array
 * Returns pointer to ctx_parm entry, or nullptr if not found
 */
const ctx_parm* get_parm_info(const std::string &parm_name)
{
    int indx = 0;
    while (config_parms[indx].parm_name != "") {
        if (config_parms[indx].parm_name == parm_name) {
            return &config_parms[indx];
        }
        indx++;
    }
    return nullptr;  /* Parameter not found */
}
